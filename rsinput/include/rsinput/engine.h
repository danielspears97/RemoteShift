// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// RemoteShift input engine — keyboard & mouse to DualSense state.
//
// The engine is deterministic and platform-neutral: the host feeds it timestamped
// key, mouse and wheel events and calls Tick() at a steady rate. Tick() returns
// the complete emulated controller state, which the host hands straight to the
// Remote Play session (no virtual controller driver involved).
#pragma once

#include "mousestick.h"
#include "profile.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rs {

struct PadState {
	uint32_t buttons = 0;   // PadButton bitmask (== ChiakiControllerButton)
	uint8_t l2 = 0, r2 = 0;
	int16_t lx = 0, ly = 0; // stick axes, negative y = up (Remote Play convention)
	int16_t rx = 0, ry = 0;
	bool operator==(const PadState &o) const
	{
		return buttons == o.buttons && l2 == o.l2 && r2 == o.r2 && lx == o.lx && ly == o.ly && rx == o.rx && ry == o.ry;
	}
	bool operator!=(const PadState &o) const { return !(*this == o); }
};

struct EngineEvent {
	enum class Type { ModeChanged, CaptureToggleRequested };
	Type type;
	std::string mode;
};

struct EngineTelemetry {
	Vec2 mouse_velocity;   // counts/s (smoothed)
	float mouse_input_magnitude = 0.0f;
	Vec2 mouse_stick;      // mouse contribution, -1..1
	Vec2 key_left, key_right; // key contributions after ramps
	bool ads_active = false;
	float sens_scale = 1.0f;
	int held_keys = 0;
};

class Engine
{
public:
	Engine();

	// Replaces the profile. Keeps the current mode if it still exists.
	void SetProfile(const Profile &profile);
	const Profile &GetProfile() const { return profile_; }

	void KeyDown(Key k, uint64_t t_us);
	void KeyUp(Key k, uint64_t t_us);
	void MouseDelta(int dx, int dy, uint64_t t_us);
	// Positive = wheel up / right. One unit = one notch (WHEEL_DELTA / 120).
	void Wheel(int vertical_notches, int horizontal_notches, uint64_t t_us);
	// Release every key and reset transient state (used on focus loss / capture release).
	void ReleaseAll();

	bool SetMode(const std::string &mode);
	const std::string &ActiveMode() const { return active_mode_; }

	PadState Tick(uint64_t now_us);
	std::vector<EngineEvent> TakeEvents();
	const EngineTelemetry &Telemetry() const { return telemetry_; }
	bool IsHeld(Key k) const { return held_[static_cast<int>(k)]; }

private:
	struct BindingState {
		bool prev_active = false;
		bool toggled = false;
		uint64_t edge_us = 0;
	};
	struct WheelQueue {
		int pending = 0;
		bool active = false;
		uint64_t until_us = 0;
		uint64_t next_ok_us = 0;
	};

	const Mode *CurrentMode() const;
	void SwitchMode(const std::string &mode, uint64_t now_us);
	bool ChordHeld(const Binding &b) const;
	uint64_t ChordSeq(const Binding &b) const;
	void ResetBindingStates();
	void UpdateWheel(uint64_t now_us);

	Profile profile_;
	std::string active_mode_;
	int active_mode_index_ = -1;

	std::array<bool, kKeyCount> held_{};
	std::array<uint64_t, kKeyCount> press_seq_{};
	uint64_t seq_counter_ = 0;
	std::array<WheelQueue, 4> wheel_{};

	std::vector<BindingState> bstate_;
	// HoldMode bookkeeping: the chord that entered the temporary mode, and where to return.
	bool hold_mode_active_ = false;
	Binding hold_mode_binding_;
	std::string hold_return_mode_;

	MouseStick mouse_;
	Vec2 key_left_, key_right_;
	float l2_cur_ = 0.0f, r2_cur_ = 0.0f;
	uint64_t last_tick_us_ = 0;
	bool have_tick_ = false;

	std::vector<EngineEvent> events_;
	EngineTelemetry telemetry_;
};

} // namespace rs
