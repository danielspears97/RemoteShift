// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "rsinput/mousestick.h"

#include <algorithm>
#include <cmath>

namespace rs {

static float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float Sign(float v) { return v < 0.0f ? -1.0f : 1.0f; }

void MouseStick::Reset()
{
	samples_.clear();
	ema_ = Vec2();
	have_prev_report_ = false;
	have_last_ = false;
	last_input_mag_ = 0.0f;
}

void MouseStick::AddDelta(int dx, int dy, uint64_t t_us)
{
	if(dx == 0 && dy == 0)
		return;
	// Each report covers the interval since the previous one. Treating motion as spread over
	// that interval (instead of as an impulse) makes the velocity estimate independent of the
	// mouse's polling rate and of how report times line up with engine ticks.
	uint64_t span = kMaxReportSpanUs;
	if(have_prev_report_ && t_us > prev_report_us_)
		span = std::min<uint64_t>(t_us - prev_report_us_, kMaxReportSpanUs);
	span = std::max<uint64_t>(span, kMinReportSpanUs);
	if(have_prev_report_ && t_us < prev_report_us_)
		t_us = prev_report_us_; // never go backwards
	prev_report_us_ = t_us;
	have_prev_report_ = true;
	samples_.push_back({t_us, span, dx, dy});
	// Bound memory even if Update() is never called (e.g. paused session).
	while(samples_.size() > 4096)
		samples_.pop_front();
}

float MouseStick::Curve(const MouseConfig &cfg, float m)
{
	m = Clamp(m, 0.0f, 1.0f);
	switch(cfg.curve)
	{
		case CurveType::Linear:
			return m;
		case CurveType::Power:
			return std::pow(m, Clamp(cfg.curve_exponent, 0.1f, 5.0f));
		case CurveType::Custom:
		{
			const auto &pts = cfg.curve_points;
			if(pts.empty())
				return m;
			// Implicit (0,0) and (1,1) end points unless overridden.
			float px = 0.0f, py = 0.0f;
			for(const auto &p : pts)
			{
				if(m <= p.in)
				{
					float span = p.in - px;
					float t = span > 1e-6f ? (m - px) / span : 1.0f;
					return Clamp(py + t * (p.out - py), 0.0f, 1.0f);
				}
				px = p.in;
				py = p.out;
			}
			float span = 1.0f - px;
			float t = span > 1e-6f ? (m - px) / span : 1.0f;
			return Clamp(py + t * (1.0f - py), 0.0f, 1.0f);
		}
	}
	return m;
}

// Disc -> square elliptical grid mapping (inverse of the square->disc mapping
// x*sqrt(1 - y^2/2)). Lets diagonals reach the corners the way a physical
// DualSense stick does, which many games' per-axis readers expect.
static Vec2 DiscToSquare(Vec2 v)
{
	const float u = v.x, w = v.y;
	const float u2 = u * u, w2 = w * w;
	const float r2 = 2.0f * std::sqrt(2.0f);
	auto s = [](float a) { return std::sqrt(std::max(0.0f, a)); };
	Vec2 o;
	o.x = 0.5f * s(2.0f + u2 - w2 + r2 * u) - 0.5f * s(2.0f + u2 - w2 - r2 * u);
	o.y = 0.5f * s(2.0f - u2 + w2 + r2 * w) - 0.5f * s(2.0f - u2 + w2 - r2 * w);
	o.x = Clamp(o.x, -1.0f, 1.0f);
	o.y = Clamp(o.y, -1.0f, 1.0f);
	return o;
}

Vec2 MouseStick::Shape(const MouseConfig &cfg, Vec2 in)
{
	Vec2 out;
	float m = std::sqrt(in.x * in.x + in.y * in.y);
	if(m < 1e-6f)
		return out;
	const float dirx = in.x / m, diry = in.y / m;
	m = std::min(m, 1.0f);

	const float dz = Clamp(cfg.deadzone, 0.0f, 0.5f);
	if(m <= dz)
		return out;
	m = (m - dz) / (1.0f - dz);
	m = Curve(cfg, m);

	const float adz = Clamp(cfg.anti_deadzone, 0.0f, 0.9f);
	const float outer = Clamp(cfg.outer_limit, 0.1f, 1.0f);
	if(cfg.anti_deadzone_shape == DeadzoneShape::Radial)
	{
		float r = (adz + (1.0f - adz) * m) * outer;
		out.x = dirx * r;
		out.y = diry * r;
	}
	else
	{
		// Axial: offset each axis independently past the game's per-axis deadzone.
		// Components that are a tiny fraction of the motion are left alone so a mostly
		// vertical flick does not leak a large horizontal deflection.
		float ax = dirx * m, ay = diry * m;
		auto off = [&](float c) {
			if(std::fabs(c) < 0.02f)
				return c * outer;
			return Sign(c) * (adz + (1.0f - adz) * std::fabs(c)) * outer;
		};
		out.x = off(ax);
		out.y = off(ay);
	}

	const float sq = Clamp(cfg.square_saturation, 0.0f, 1.0f);
	if(sq > 0.0f)
	{
		Vec2 s = DiscToSquare(out);
		out.x += (s.x - out.x) * sq;
		out.y += (s.y - out.y) * sq;
	}
	out.x = Clamp(out.x, -1.0f, 1.0f);
	out.y = Clamp(out.y, -1.0f, 1.0f);
	return out;
}

Vec2 MouseStick::Update(const MouseConfig &cfg, uint64_t now_us, float sens_scale)
{
	const float window_ms = Clamp(cfg.sample_window_ms, 1.0f, 100.0f);
	const double window_us = window_ms * 1000.0;

	// The window ends at the newest report while reports keep arriving at their usual
	// cadence, so the gap before the next report does not read as a slowdown. Once no
	// report arrives within 1.5x the last report interval, the window slides with real
	// time and the stick returns to centre.
	double window_end = (double)now_us;
	if(!samples_.empty())
	{
		const Sample &last = samples_.back();
		double hold = std::min(1.5 * (double)last.span, 16000.0);
		window_end = std::min((double)now_us, std::max((double)last.t, (double)now_us - hold));
	}
	const double window_start = window_end - window_us;

	while(!samples_.empty() && (double)samples_.front().t <= window_start)
		samples_.pop_front();

	double sx = 0.0, sy = 0.0;
	for(const auto &s : samples_)
	{
		const double s1 = (double)s.t, s0 = s1 - (double)s.span;
		const double overlap = std::min(s1, window_end) - std::max(s0, window_start);
		if(overlap <= 0.0)
			continue;
		const double frac = overlap / (double)s.span;
		sx += s.dx * frac;
		sy += s.dy * frac;
	}
	Vec2 vel;
	vel.x = (float)(sx / (window_us / 1e6));
	vel.y = (float)(sy / (window_us / 1e6));

	// Exponential smoothing; time constant scales with the smoothing strength.
	float dt = have_last_ && now_us > last_update_us_ ? (float)(now_us - last_update_us_) / 1e6f : 0.0f;
	have_last_ = true;
	last_update_us_ = now_us;
	const float tau = Clamp(cfg.smoothing, 0.0f, 1.0f) * 0.060f;
	if(tau <= 1e-5f || dt <= 0.0f)
		ema_ = vel;
	else
	{
		float alpha = 1.0f - std::exp(-dt / tau);
		ema_.x += (vel.x - ema_.x) * alpha;
		ema_.y += (vel.y - ema_.y) * alpha;
	}
	// Snap tiny residuals to zero so the stick fully re-centres.
	if(samples_.empty() && std::fabs(ema_.x) < 1.0f && std::fabs(ema_.y) < 1.0f)
		ema_ = Vec2();

	const float gain = cfg.sensitivity * sens_scale / kReferenceCountsPerSecond;
	Vec2 n;
	n.x = ema_.x * gain * cfg.sens_x;
	n.y = ema_.y * gain * cfg.sens_y * (cfg.invert_y ? -1.0f : 1.0f);

	float m = std::sqrt(n.x * n.x + n.y * n.y);
	if(cfg.acceleration > 0.0f && m > 0.0f)
	{
		float boost = std::min(cfg.acceleration * m, std::max(0.0f, cfg.accel_cap));
		n.x *= 1.0f + boost;
		n.y *= 1.0f + boost;
		m *= 1.0f + boost;
	}
	last_input_mag_ = m;
	return Shape(cfg, n);
}

} // namespace rs
