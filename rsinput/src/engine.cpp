// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "rsinput/engine.h"

#include <algorithm>
#include <cmath>

namespace rs {

static float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int16_t ToAxis(float v)
{
	v = Clamp(v, -1.0f, 1.0f);
	long r = std::lround(v * 32767.0f);
	return (int16_t)std::max(-32767L, std::min(32767L, r));
}

static uint8_t ToTrigger(float v)
{
	return (uint8_t)std::lround(Clamp(v, 0.0f, 1.0f) * 255.0f);
}

// Move `cur` toward `target`, using the up rate when moving away from centre and the
// down rate when moving toward centre (including when reversing direction).
static float Ramp(float cur, float target, float up_ms, float down_ms, float dt)
{
	if(cur == target)
		return cur;
	const bool toward_centre = (cur > 0 && target < cur) || (cur < 0 && target > cur);
	float ms = toward_centre ? down_ms : up_ms;
	if(ms <= 0.0f || dt <= 0.0f)
	{
		if(ms <= 0.0f)
			return target;
		return cur;
	}
	float step = dt * 1000.0f / ms; // full scale per `ms`
	if(toward_centre && ((cur > 0 && target < 0) || (cur < 0 && target > 0)))
	{
		// Reversing: return to centre first at the down rate, then continue at the up rate.
		float mag = std::fabs(cur);
		if(step < mag)
			return cur > 0 ? cur - step : cur + step;
		float remaining_dt = dt * (1.0f - mag / step);
		float up_step = up_ms <= 0.0f ? 1e9f : remaining_dt * 1000.0f / up_ms;
		return target > 0 ? std::min(target, up_step) : std::max(target, -up_step);
	}
	if(cur < target)
		return std::min(target, cur + step);
	return std::max(target, cur - step);
}

Engine::Engine()
{
	Profile p;
	p.name = "Empty";
	Mode m;
	m.name = "Default";
	p.modes.push_back(m);
	p.default_mode = "Default";
	SetProfile(p);
}

void Engine::SetProfile(const Profile &profile)
{
	std::string keep = active_mode_;
	profile_ = profile;
	if(profile_.modes.empty())
	{
		Mode m;
		m.name = "Default";
		profile_.modes.push_back(m);
	}
	hold_mode_active_ = false;
	active_mode_index_ = -1;
	const std::string target = profile_.FindMode(keep) ? keep : (profile_.FindMode(profile_.default_mode) ? profile_.default_mode : profile_.modes.front().name);
	active_mode_.clear();
	SwitchMode(target, last_tick_us_);
}

const Mode *Engine::CurrentMode() const
{
	if(active_mode_index_ < 0 || active_mode_index_ >= (int)profile_.modes.size())
		return nullptr;
	return &profile_.modes[active_mode_index_];
}

bool Engine::SetMode(const std::string &mode)
{
	if(!profile_.FindMode(mode))
		return false;
	hold_mode_active_ = false;
	SwitchMode(mode, last_tick_us_);
	return true;
}

void Engine::SwitchMode(const std::string &mode, uint64_t now_us)
{
	(void)now_us;
	for(size_t i = 0; i < profile_.modes.size(); i++)
	{
		if(profile_.modes[i].name != mode)
			continue;
		bool changed = active_mode_ != mode;
		active_mode_ = mode;
		active_mode_index_ = (int)i;
		ResetBindingStates();
		if(changed)
			events_.push_back({EngineEvent::Type::ModeChanged, mode});
		return;
	}
}

void Engine::ResetBindingStates()
{
	const Mode *m = CurrentMode();
	bstate_.assign(m ? m->bindings.size() : 0, BindingState());
	// Keys that are already down must not fire press-edge actions in the new mode;
	// otherwise a key that toggles between two modes would bounce back immediately.
	if(m)
		for(size_t i = 0; i < m->bindings.size(); i++)
			bstate_[i].prev_active = ChordHeld(m->bindings[i]);
}

bool Engine::ChordHeld(const Binding &b) const
{
	if(b.key == Key::None || !held_[static_cast<int>(b.key)])
		return false;
	for(Key k : b.modifiers)
		if(!held_[static_cast<int>(k)])
			return false;
	return true;
}

uint64_t Engine::ChordSeq(const Binding &b) const
{
	return press_seq_[static_cast<int>(b.key)];
}

void Engine::KeyDown(Key k, uint64_t)
{
	int i = static_cast<int>(k);
	if(i <= 0 || i >= kKeyCount || IsWheelKey(k))
		return;
	if(!held_[i])
	{
		held_[i] = true;
		press_seq_[i] = ++seq_counter_;
	}
}

void Engine::KeyUp(Key k, uint64_t)
{
	int i = static_cast<int>(k);
	if(i <= 0 || i >= kKeyCount || IsWheelKey(k))
		return;
	held_[i] = false;
}

void Engine::MouseDelta(int dx, int dy, uint64_t t_us)
{
	mouse_.AddDelta(dx, dy, t_us);
}

void Engine::Wheel(int v, int h, uint64_t)
{
	auto add = [&](int idx, int n) { wheel_[idx].pending = std::min(wheel_[idx].pending + n, 16); };
	if(v > 0) add(0, v);
	if(v < 0) add(1, -v);
	if(h < 0) add(2, -h);
	if(h > 0) add(3, h);
}

void Engine::ReleaseAll()
{
	held_.fill(false);
	for(auto &w : wheel_)
		w = WheelQueue();
	mouse_.Reset();
	key_left_ = key_right_ = Vec2();
	l2_cur_ = r2_cur_ = 0.0f;
	if(hold_mode_active_)
	{
		hold_mode_active_ = false;
		SwitchMode(hold_return_mode_, last_tick_us_);
	}
	ResetBindingStates();
	for(auto &s : bstate_)
		s.toggled = false;
}

void Engine::UpdateWheel(uint64_t now)
{
	const uint64_t pulse = (uint64_t)(Clamp(profile_.wheel_pulse_ms, 8.0f, 500.0f) * 1000.0f);
	const uint64_t gap = (uint64_t)(Clamp(profile_.wheel_gap_ms, 8.0f, 500.0f) * 1000.0f);
	for(int i = 0; i < 4; i++)
	{
		WheelQueue &w = wheel_[i];
		const int key = static_cast<int>(Key::WheelUp) + i;
		if(w.active && now >= w.until_us)
		{
			w.active = false;
			w.next_ok_us = now + gap;
		}
		if(!w.active && w.pending > 0 && now >= w.next_ok_us)
		{
			w.active = true;
			w.pending--;
			w.until_us = now + pulse;
			press_seq_[key] = ++seq_counter_;
		}
		held_[key] = w.active;
	}
}

std::vector<EngineEvent> Engine::TakeEvents()
{
	std::vector<EngineEvent> out;
	out.swap(events_);
	return out;
}

PadState Engine::Tick(uint64_t now)
{
	float dt = have_tick_ && now > last_tick_us_ ? (float)(now - last_tick_us_) / 1e6f : 0.0f;
	dt = std::min(dt, 0.1f); // a stalled host should not cause a huge ramp step
	have_tick_ = true;
	last_tick_us_ = now;

	UpdateWheel(now);

	// Leave a held temporary mode as soon as its chord is released.
	if(hold_mode_active_ && !ChordHeld(hold_mode_binding_))
	{
		hold_mode_active_ = false;
		SwitchMode(hold_return_mode_, now);
	}

	// Mode-switch actions are processed first and at most once per tick, then the
	// (possibly new) mode's bindings produce the controller state.
	for(int pass = 0; pass < 2; pass++)
	{
		const Mode *mode = CurrentMode();
		if(!mode)
			break;
		std::string switch_to;
		bool hold_switch = false;
		Binding hold_binding;
		for(size_t i = 0; i < mode->bindings.size(); i++)
		{
			const Binding &b = mode->bindings[i];
			const ActionType t = b.action.type;
			if(t != ActionType::SetMode && t != ActionType::HoldMode && t != ActionType::CycleMode && t != ActionType::ToggleCapture)
				continue;
			bool held = ChordHeld(b);
			bool rising = held && !bstate_[i].prev_active;
			bstate_[i].prev_active = held;
			if(!rising)
				continue;
			if(t == ActionType::ToggleCapture)
				events_.push_back({EngineEvent::Type::CaptureToggleRequested, std::string()});
			else if(switch_to.empty())
			{
				if(t == ActionType::CycleMode)
					switch_to = profile_.modes[(active_mode_index_ + 1) % profile_.modes.size()].name;
				else
				{
					switch_to = b.action.mode;
					if(t == ActionType::HoldMode)
					{
						hold_switch = true;
						hold_binding = b;
					}
				}
			}
		}
		if(switch_to.empty() || !profile_.FindMode(switch_to) || switch_to == active_mode_)
			break;
		if(hold_switch)
		{
			hold_return_mode_ = hold_mode_active_ ? hold_return_mode_ : active_mode_;
			hold_mode_binding_ = hold_binding;
			hold_mode_active_ = true;
		}
		else
			hold_mode_active_ = false;
		SwitchMode(switch_to, now);
	}

	const Mode *mode = CurrentMode();
	PadState out;
	if(!mode)
		return out;

	// Most specific chord wins: if "Ctrl+E" is held, a plain "E" binding is suppressed.
	std::array<int, kKeyCount> best_mods;
	best_mods.fill(-1);
	for(const Binding &b : mode->bindings)
		if(ChordHeld(b))
		{
			int &bm = best_mods[static_cast<int>(b.key)];
			bm = std::max(bm, (int)b.modifiers.size());
		}

	struct Dir { float mag = 0.0f; uint64_t seq = 0; };
	Dir ls[4], rs_[4]; // up, down, left, right
	float l2_target = 0.0f, r2_target = 0.0f;
	bool ads = false;
	float sens_scale = 1.0f;

	for(size_t i = 0; i < mode->bindings.size(); i++)
	{
		const Binding &b = mode->bindings[i];
		const ActionType t = b.action.type;
		if(t == ActionType::SetMode || t == ActionType::HoldMode || t == ActionType::CycleMode || t == ActionType::ToggleCapture)
			continue;
		BindingState &st = bstate_[i];
		bool held = ChordHeld(b) && best_mods[static_cast<int>(b.key)] == (int)b.modifiers.size();
		bool rising = held && !st.prev_active;
		st.prev_active = held;
		if(rising)
			st.edge_us = now;

		bool active = false;
		switch(b.bind_mode)
		{
			case BindMode::Hold:
				active = held;
				break;
			case BindMode::Toggle:
				if(rising)
					st.toggled = !st.toggled;
				active = st.toggled;
				break;
			case BindMode::Turbo:
				if(held)
				{
					float hz = Clamp(b.turbo_hz, 1.0f, 30.0f);
					double phase = std::fmod((double)(now - st.edge_us) / 1e6 * hz, 1.0);
					active = phase < 0.5;
				}
				break;
		}
		if(!active)
			continue;

		const float v = Clamp(b.action.value, 0.0f, 1.0f);
		const uint64_t seq = ChordSeq(b);
		auto dir = [&](Dir *d, int idx) {
			if(v > d[idx].mag) d[idx].mag = v;
			if(seq > d[idx].seq) d[idx].seq = seq;
		};
		switch(t)
		{
			case ActionType::Button: out.buttons |= static_cast<uint32_t>(b.action.button); break;
			case ActionType::L2: l2_target = std::max(l2_target, v); break;
			case ActionType::R2: r2_target = std::max(r2_target, v); break;
			case ActionType::LeftStickUp: dir(ls, 0); break;
			case ActionType::LeftStickDown: dir(ls, 1); break;
			case ActionType::LeftStickLeft: dir(ls, 2); break;
			case ActionType::LeftStickRight: dir(ls, 3); break;
			case ActionType::RightStickUp: dir(rs_, 0); break;
			case ActionType::RightStickDown: dir(rs_, 1); break;
			case ActionType::RightStickLeft: dir(rs_, 2); break;
			case ActionType::RightStickRight: dir(rs_, 3); break;
			case ActionType::AdsHold: ads = true; break;
			case ActionType::SensitivityScale: sens_scale *= Clamp(b.action.value <= 0 ? 1.0f : b.action.value, 0.05f, 10.0f); break;
			default: break;
		}
	}

	auto resolve = [](const Dir *d, const KeyStickConfig &cfg) {
		auto axis = [&](const Dir &neg, const Dir &pos) -> float {
			bool n = neg.mag > 0.0f, p = pos.mag > 0.0f;
			if(n && p)
			{
				switch(cfg.socd)
				{
					case Socd::Neutral: return 0.0f;
					case Socd::LastWins: return pos.seq > neg.seq ? pos.mag : -neg.mag;
					case Socd::FirstWins: return pos.seq < neg.seq ? pos.mag : -neg.mag;
				}
			}
			return p ? pos.mag : (n ? -neg.mag : 0.0f);
		};
		Vec2 v;
		v.x = axis(d[2], d[3]);
		v.y = axis(d[0], d[1]); // up = negative
		if(cfg.circular_diagonals && v.x != 0.0f && v.y != 0.0f)
		{
			float len = std::sqrt(v.x * v.x + v.y * v.y);
			float target = std::max(std::fabs(v.x), std::fabs(v.y));
			v.x *= target / len;
			v.y *= target / len;
		}
		return v;
	};
	Vec2 lt = resolve(ls, mode->left_keys);
	Vec2 rt = resolve(rs_, mode->right_keys);
	key_left_.x = Ramp(key_left_.x, lt.x, mode->left_keys.ramp_up_ms, mode->left_keys.ramp_down_ms, dt);
	key_left_.y = Ramp(key_left_.y, lt.y, mode->left_keys.ramp_up_ms, mode->left_keys.ramp_down_ms, dt);
	key_right_.x = Ramp(key_right_.x, rt.x, mode->right_keys.ramp_up_ms, mode->right_keys.ramp_down_ms, dt);
	key_right_.y = Ramp(key_right_.y, rt.y, mode->right_keys.ramp_up_ms, mode->right_keys.ramp_down_ms, dt);
	l2_cur_ = Ramp(l2_cur_, l2_target, mode->trigger_ramp_ms, mode->trigger_ramp_ms, dt);
	r2_cur_ = Ramp(r2_cur_, r2_target, mode->trigger_ramp_ms, mode->trigger_ramp_ms, dt);

	if(mode->mouse.ads_follows_l2 && l2_cur_ > 0.5f)
		ads = true;
	float scale = sens_scale * (ads ? std::max(0.01f, mode->mouse.ads_multiplier) : 1.0f);
	Vec2 ms = mouse_.Update(mode->mouse, now, scale);

	Vec2 left = key_left_, right = key_right_;
	if(mode->mouse.target == StickTarget::RightStick) { right.x += ms.x; right.y += ms.y; }
	else if(mode->mouse.target == StickTarget::LeftStick) { left.x += ms.x; left.y += ms.y; }

	out.lx = ToAxis(left.x);
	out.ly = ToAxis(left.y);
	out.rx = ToAxis(right.x);
	out.ry = ToAxis(right.y);
	out.l2 = ToTrigger(l2_cur_);
	out.r2 = ToTrigger(r2_cur_);

	telemetry_.mouse_velocity = mouse_.Velocity();
	telemetry_.mouse_input_magnitude = mouse_.LastInputMagnitude();
	telemetry_.mouse_stick = ms;
	telemetry_.key_left = key_left_;
	telemetry_.key_right = key_right_;
	telemetry_.ads_active = ads;
	telemetry_.sens_scale = scale;
	telemetry_.held_keys = (int)std::count(held_.begin(), held_.end(), true);
	return out;
}

} // namespace rs
