// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// RemoteShift input engine — profile data model.
#pragma once

#include "keycodes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rs {

// Emulated DualSense outputs. Digital bit values intentionally match
// ChiakiControllerButton so the host can copy the mask directly.
enum class PadButton : uint32_t {
	Cross = 1u << 0, Circle = 1u << 1, Square = 1u << 2, Triangle = 1u << 3,
	DpadLeft = 1u << 4, DpadRight = 1u << 5, DpadUp = 1u << 6, DpadDown = 1u << 7,
	L1 = 1u << 8, R1 = 1u << 9, L3 = 1u << 10, R3 = 1u << 11,
	Options = 1u << 12, Create = 1u << 13, Touchpad = 1u << 14, PS = 1u << 15,
};

enum class ActionType : uint8_t {
	None,
	Button,          // press a digital button
	L2, R2,          // analog triggers, value = 0..1
	LeftStickUp, LeftStickDown, LeftStickLeft, LeftStickRight,
	RightStickUp, RightStickDown, RightStickLeft, RightStickRight,
	SetMode,         // switch to mode `mode` on press
	HoldMode,        // switch to `mode` while held, return to previous on release
	CycleMode,       // advance to next mode in profile order
	AdsHold,         // apply the ADS sensitivity multiplier while held (without pressing L2)
	SensitivityScale,// multiply mouse sensitivity by `value` while held (e.g. precision/sniper key)
	ToggleCapture,   // ask host to release/capture the mouse
};

enum class BindMode : uint8_t {
	Hold,   // active while the chord is held
	Toggle, // each press flips the output on/off
	Turbo,  // pulses while held at `turbo_hz`
};

struct Action {
	ActionType type = ActionType::None;
	PadButton button = PadButton::Cross; // for ActionType::Button
	float value = 1.0f;                   // trigger amount / stick magnitude / sensitivity scale
	std::string mode;                     // for SetMode / HoldMode
};

struct Binding {
	std::vector<Key> modifiers; // must all be held (order irrelevant)
	Key key = Key::None;        // the trigger key
	Action action;
	BindMode bind_mode = BindMode::Hold;
	float turbo_hz = 10.0f;
};

enum class CurveType : uint8_t { Linear, Power, Custom };
enum class DeadzoneShape : uint8_t { Radial, Axial };
enum class StickTarget : uint8_t { None, LeftStick, RightStick };

struct CurvePoint { float in; float out; };

// Mouse -> analog stick translation settings.
struct MouseConfig {
	StickTarget target = StickTarget::RightStick;
	float sensitivity = 1.0f;        // overall gain
	float sens_x = 1.0f;             // horizontal multiplier
	float sens_y = 1.0f;             // vertical multiplier
	bool invert_y = false;
	float ads_multiplier = 0.6f;     // applied while L2 is active or an AdsHold binding is held
	bool ads_follows_l2 = true;
	float sample_window_ms = 8.0f;   // velocity is measured over this window (handles low polling rates)
	float smoothing = 0.15f;         // 0 = off, 1 = heavy (EMA time constant up to 60 ms)
	float acceleration = 0.0f;       // 0 = off; extra gain proportional to speed
	float accel_cap = 2.0f;          // maximum gain added by acceleration
	float deadzone = 0.0f;           // ignore processed magnitude below this (0..0.5)
	CurveType curve = CurveType::Power;
	float curve_exponent = 0.8f;     // <1 boosts small motions (compensates games' exponential stick curves)
	std::vector<CurvePoint> curve_points; // for CurveType::Custom, sorted by `in`, 0..1
	float anti_deadzone = 0.18f;     // minimum output once moving, to escape the game's inner deadzone
	DeadzoneShape anti_deadzone_shape = DeadzoneShape::Radial;
	float outer_limit = 1.0f;        // maximum output radius
	float square_saturation = 0.0f;  // 0 = circular output, 1 = full circle-to-square (diagonals reach corners)
};

enum class Socd : uint8_t { LastWins, Neutral, FirstWins };

// Keys -> analog stick behaviour (WASD movement, driving steering, flight controls).
struct KeyStickConfig {
	float ramp_up_ms = 0.0f;    // time to go from 0 to full deflection (0 = instant)
	float ramp_down_ms = 0.0f;  // time to return to centre (0 = instant)
	Socd socd = Socd::LastWins; // opposite directions held together
	bool circular_diagonals = true; // normalise diagonals to radius 1
};

struct Mode {
	std::string name;
	std::vector<Binding> bindings;
	MouseConfig mouse;
	KeyStickConfig left_keys;
	KeyStickConfig right_keys;
	float trigger_ramp_ms = 0.0f; // gradual L2/R2 when driven by keys
};

struct Profile {
	std::string name;
	std::string game;            // free text, used to auto-select per game
	std::string description;
	std::vector<Mode> modes;
	std::string default_mode;
	float wheel_pulse_ms = 40.0f; // how long a wheel notch holds its bound button
	float wheel_gap_ms = 30.0f;   // release gap between queued notches

	const Mode *FindMode(const std::string &n) const;
};

// Serialisation (JSON). Throws std::runtime_error with a readable message on malformed input.
std::string ProfileToJson(const Profile &p, int indent = 2);
Profile ProfileFromJson(const std::string &json);

// Validation: returns human-readable problems (empty = valid). Unknown keys, missing modes, etc.
std::vector<std::string> ValidateProfile(const Profile &p);

const char *PadButtonName(PadButton b);
bool PadButtonFromName(const std::string &n, PadButton *out);
const char *ActionTypeName(ActionType t);
bool ActionTypeFromName(const std::string &n, ActionType *out);

std::string DescribeBinding(const Binding &b); // e.g. "LCtrl + E"
std::string DescribeAction(const Action &a);   // e.g. "Cross", "Left stick up (50%)"

} // namespace rs
