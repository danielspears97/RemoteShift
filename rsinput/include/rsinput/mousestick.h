// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// RemoteShift input engine — raw mouse motion to analog stick deflection.
#pragma once

#include "profile.h"

#include <cstdint>
#include <deque>

namespace rs {

struct Vec2 {
	float x = 0.0f, y = 0.0f;
};

// Mouse counts per second that map to full deflection at sensitivity 1.0 (before curve).
// 2000 counts/s is a 1.25 in/s hand movement on an 1600 DPI mouse.
constexpr float kReferenceCountsPerSecond = 2000.0f;

class MouseStick
{
public:
	void Reset();
	// Raw relative counts as delivered by the OS (Raw Input on Windows), timestamped in microseconds.
	void AddDelta(int dx, int dy, uint64_t t_us);
	// Produces stick deflection in [-1, 1] on both axes. `sens_scale` folds in ADS and per-binding scaling.
	Vec2 Update(const MouseConfig &cfg, uint64_t now_us, float sens_scale);

	// Last measured (smoothed) mouse velocity in counts/second, for diagnostics/visualisation.
	Vec2 Velocity() const { return ema_; }
	// Normalised magnitude after sensitivity/acceleration, before shaping (0..n).
	float LastInputMagnitude() const { return last_input_mag_; }

	// The pure shaping stage: normalised input vector -> stick output.
	// Exposed for curve previews in the UI and for unit tests.
	static Vec2 Shape(const MouseConfig &cfg, Vec2 in);
	static float Curve(const MouseConfig &cfg, float m);

private:
	static constexpr uint64_t kMaxReportSpanUs = 8000; // slowest common polling rate: 125 Hz
	static constexpr uint64_t kMinReportSpanUs = 125;  // 8000 Hz
	struct Sample { uint64_t t; uint64_t span; int dx, dy; };
	uint64_t prev_report_us_ = 0;
	bool have_prev_report_ = false;
	std::deque<Sample> samples_;
	Vec2 ema_;
	uint64_t last_update_us_ = 0;
	bool have_last_ = false;
	float last_input_mag_ = 0.0f;
};

} // namespace rs
