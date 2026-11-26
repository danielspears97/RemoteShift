// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "rsinput/profile.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>

using nlohmann::json;

namespace rs {

const Mode *Profile::FindMode(const std::string &n) const
{
	for(const auto &m : modes)
		if(m.name == n)
			return &m;
	return nullptr;
}

// ---------------------------------------------------------------- names

static const std::pair<PadButton, const char *> kPadButtons[] = {
	{PadButton::Cross, "Cross"}, {PadButton::Circle, "Circle"}, {PadButton::Square, "Square"},
	{PadButton::Triangle, "Triangle"}, {PadButton::DpadLeft, "DpadLeft"}, {PadButton::DpadRight, "DpadRight"},
	{PadButton::DpadUp, "DpadUp"}, {PadButton::DpadDown, "DpadDown"}, {PadButton::L1, "L1"}, {PadButton::R1, "R1"},
	{PadButton::L3, "L3"}, {PadButton::R3, "R3"}, {PadButton::Options, "Options"}, {PadButton::Create, "Create"},
	{PadButton::Touchpad, "Touchpad"}, {PadButton::PS, "PS"},
};

const char *PadButtonName(PadButton b)
{
	for(const auto &p : kPadButtons)
		if(p.first == b)
			return p.second;
	return "Unknown";
}

bool PadButtonFromName(const std::string &n, PadButton *out)
{
	for(const auto &p : kPadButtons)
		if(n == p.second) { *out = p.first; return true; }
	if(n == "Share") { *out = PadButton::Create; return true; }
	return false;
}

static const std::pair<ActionType, const char *> kActions[] = {
	{ActionType::None, "None"}, {ActionType::Button, "Button"}, {ActionType::L2, "L2"}, {ActionType::R2, "R2"},
	{ActionType::LeftStickUp, "LeftStickUp"}, {ActionType::LeftStickDown, "LeftStickDown"},
	{ActionType::LeftStickLeft, "LeftStickLeft"}, {ActionType::LeftStickRight, "LeftStickRight"},
	{ActionType::RightStickUp, "RightStickUp"}, {ActionType::RightStickDown, "RightStickDown"},
	{ActionType::RightStickLeft, "RightStickLeft"}, {ActionType::RightStickRight, "RightStickRight"},
	{ActionType::SetMode, "SetMode"}, {ActionType::HoldMode, "HoldMode"}, {ActionType::CycleMode, "CycleMode"},
	{ActionType::AdsHold, "AdsHold"}, {ActionType::SensitivityScale, "SensitivityScale"},
	{ActionType::ToggleCapture, "ToggleCapture"},
};

const char *ActionTypeName(ActionType t)
{
	for(const auto &p : kActions)
		if(p.first == t)
			return p.second;
	return "None";
}

bool ActionTypeFromName(const std::string &n, ActionType *out)
{
	for(const auto &p : kActions)
		if(n == p.second) { *out = p.first; return true; }
	return false;
}

static const char *CurveName(CurveType c)
{
	switch(c) { case CurveType::Linear: return "linear"; case CurveType::Power: return "power"; default: return "custom"; }
}
static CurveType CurveFromName(const std::string &s)
{
	if(s == "linear") return CurveType::Linear;
	if(s == "custom") return CurveType::Custom;
	if(s == "power") return CurveType::Power;
	throw std::runtime_error("unknown curve type '" + s + "'");
}
static const char *SocdName(Socd s)
{
	switch(s) { case Socd::Neutral: return "neutral"; case Socd::FirstWins: return "first_wins"; default: return "last_wins"; }
}
static Socd SocdFromName(const std::string &s)
{
	if(s == "neutral") return Socd::Neutral;
	if(s == "first_wins") return Socd::FirstWins;
	if(s == "last_wins") return Socd::LastWins;
	throw std::runtime_error("unknown socd '" + s + "'");
}
static const char *TargetName(StickTarget t)
{
	switch(t) { case StickTarget::LeftStick: return "left_stick"; case StickTarget::RightStick: return "right_stick"; default: return "none"; }
}
static StickTarget TargetFromName(const std::string &s)
{
	if(s == "left_stick") return StickTarget::LeftStick;
	if(s == "right_stick") return StickTarget::RightStick;
	if(s == "none") return StickTarget::None;
	throw std::runtime_error("unknown mouse target '" + s + "'");
}
static const char *BindModeName(BindMode m)
{
	switch(m) { case BindMode::Toggle: return "toggle"; case BindMode::Turbo: return "turbo"; default: return "hold"; }
}
static BindMode BindModeFromName(const std::string &s)
{
	if(s == "toggle") return BindMode::Toggle;
	if(s == "turbo") return BindMode::Turbo;
	if(s == "hold") return BindMode::Hold;
	throw std::runtime_error("unknown binding mode '" + s + "'");
}

// ---------------------------------------------------------------- to json

static json ActionToJson(const Action &a)
{
	json j;
	j["type"] = ActionTypeName(a.type);
	if(a.type == ActionType::Button)
		j["button"] = PadButtonName(a.button);
	if(a.type == ActionType::SetMode || a.type == ActionType::HoldMode)
		j["mode"] = a.mode;
	if(a.value != 1.0f)
		j["value"] = a.value;
	return j;
}

static json BindingToJson(const Binding &b)
{
	json j;
	json mods = json::array();
	for(Key k : b.modifiers)
		mods.push_back(KeyName(k));
	if(!mods.empty())
		j["modifiers"] = mods;
	j["key"] = KeyName(b.key);
	j["action"] = ActionToJson(b.action);
	if(b.bind_mode != BindMode::Hold)
		j["bind_mode"] = BindModeName(b.bind_mode);
	if(b.bind_mode == BindMode::Turbo)
		j["turbo_hz"] = b.turbo_hz;
	return j;
}

static json MouseToJson(const MouseConfig &m)
{
	json j;
	j["target"] = TargetName(m.target);
	j["sensitivity"] = m.sensitivity;
	j["sens_x"] = m.sens_x;
	j["sens_y"] = m.sens_y;
	j["invert_y"] = m.invert_y;
	j["ads_multiplier"] = m.ads_multiplier;
	j["ads_follows_l2"] = m.ads_follows_l2;
	j["sample_window_ms"] = m.sample_window_ms;
	j["smoothing"] = m.smoothing;
	j["acceleration"] = m.acceleration;
	j["accel_cap"] = m.accel_cap;
	j["deadzone"] = m.deadzone;
	j["curve"] = CurveName(m.curve);
	j["curve_exponent"] = m.curve_exponent;
	json pts = json::array();
	for(const auto &p : m.curve_points)
		pts.push_back({p.in, p.out});
	j["curve_points"] = pts;
	j["anti_deadzone"] = m.anti_deadzone;
	j["anti_deadzone_shape"] = m.anti_deadzone_shape == DeadzoneShape::Axial ? "axial" : "radial";
	j["outer_limit"] = m.outer_limit;
	j["square_saturation"] = m.square_saturation;
	return j;
}

static json KeyStickToJson(const KeyStickConfig &k)
{
	return json{{"ramp_up_ms", k.ramp_up_ms}, {"ramp_down_ms", k.ramp_down_ms},
		{"socd", SocdName(k.socd)}, {"circular_diagonals", k.circular_diagonals}};
}

std::string ProfileToJson(const Profile &p, int indent)
{
	json j;
	j["format"] = "remoteshift-profile";
	j["version"] = 1;
	j["name"] = p.name;
	j["game"] = p.game;
	j["description"] = p.description;
	j["default_mode"] = p.default_mode;
	j["wheel_pulse_ms"] = p.wheel_pulse_ms;
	j["wheel_gap_ms"] = p.wheel_gap_ms;
	json modes = json::array();
	for(const auto &m : p.modes)
	{
		json jm;
		jm["name"] = m.name;
		jm["mouse"] = MouseToJson(m.mouse);
		jm["left_keys"] = KeyStickToJson(m.left_keys);
		jm["right_keys"] = KeyStickToJson(m.right_keys);
		jm["trigger_ramp_ms"] = m.trigger_ramp_ms;
		json binds = json::array();
		for(const auto &b : m.bindings)
			binds.push_back(BindingToJson(b));
		jm["bindings"] = binds;
		modes.push_back(jm);
	}
	j["modes"] = modes;
	return j.dump(indent);
}

// ---------------------------------------------------------------- from json

template<typename T>
static T Get(const json &j, const char *key, T def)
{
	auto it = j.find(key);
	if(it == j.end() || it->is_null())
		return def;
	try { return it->get<T>(); }
	catch(const json::exception &) { throw std::runtime_error(std::string("field '") + key + "' has the wrong type"); }
}

static Key ParseKey(const json &j, const std::string &ctx)
{
	if(!j.is_string())
		throw std::runtime_error(ctx + ": key must be a string");
	Key k = KeyFromName(j.get<std::string>());
	if(k == Key::None)
		throw std::runtime_error(ctx + ": unknown key '" + j.get<std::string>() + "'");
	return k;
}

static Action ActionFromJson(const json &j, const std::string &ctx)
{
	Action a;
	if(!j.is_object())
		throw std::runtime_error(ctx + ": action must be an object");
	std::string t = Get<std::string>(j, "type", "None");
	if(!ActionTypeFromName(t, &a.type))
		throw std::runtime_error(ctx + ": unknown action type '" + t + "'");
	if(a.type == ActionType::Button)
	{
		std::string b = Get<std::string>(j, "button", "");
		if(!PadButtonFromName(b, &a.button))
			throw std::runtime_error(ctx + ": unknown controller button '" + b + "'");
	}
	a.mode = Get<std::string>(j, "mode", "");
	a.value = Get<float>(j, "value", 1.0f);
	return a;
}

static MouseConfig MouseFromJson(const json &j)
{
	MouseConfig m;
	if(!j.is_object())
		return m;
	m.target = TargetFromName(Get<std::string>(j, "target", "right_stick"));
	m.sensitivity = Get(j, "sensitivity", m.sensitivity);
	m.sens_x = Get(j, "sens_x", m.sens_x);
	m.sens_y = Get(j, "sens_y", m.sens_y);
	m.invert_y = Get(j, "invert_y", m.invert_y);
	m.ads_multiplier = Get(j, "ads_multiplier", m.ads_multiplier);
	m.ads_follows_l2 = Get(j, "ads_follows_l2", m.ads_follows_l2);
	m.sample_window_ms = Get(j, "sample_window_ms", m.sample_window_ms);
	m.smoothing = Get(j, "smoothing", m.smoothing);
	m.acceleration = Get(j, "acceleration", m.acceleration);
	m.accel_cap = Get(j, "accel_cap", m.accel_cap);
	m.deadzone = Get(j, "deadzone", m.deadzone);
	m.curve = CurveFromName(Get<std::string>(j, "curve", "power"));
	m.curve_exponent = Get(j, "curve_exponent", m.curve_exponent);
	if(j.contains("curve_points") && j["curve_points"].is_array())
		for(const auto &p : j["curve_points"])
		{
			if(!p.is_array() || p.size() != 2)
				throw std::runtime_error("curve_points entries must be [in, out] pairs");
			m.curve_points.push_back({p[0].get<float>(), p[1].get<float>()});
		}
	m.anti_deadzone = Get(j, "anti_deadzone", m.anti_deadzone);
	m.anti_deadzone_shape = Get<std::string>(j, "anti_deadzone_shape", "radial") == "axial" ? DeadzoneShape::Axial : DeadzoneShape::Radial;
	m.outer_limit = Get(j, "outer_limit", m.outer_limit);
	m.square_saturation = Get(j, "square_saturation", m.square_saturation);
	return m;
}

static KeyStickConfig KeyStickFromJson(const json &j)
{
	KeyStickConfig k;
	if(!j.is_object())
		return k;
	k.ramp_up_ms = Get(j, "ramp_up_ms", k.ramp_up_ms);
	k.ramp_down_ms = Get(j, "ramp_down_ms", k.ramp_down_ms);
	k.socd = SocdFromName(Get<std::string>(j, "socd", "last_wins"));
	k.circular_diagonals = Get(j, "circular_diagonals", k.circular_diagonals);
	return k;
}

Profile ProfileFromJson(const std::string &text)
{
	json j;
	try { j = json::parse(text); }
	catch(const json::parse_error &e) { throw std::runtime_error(std::string("not valid JSON: ") + e.what()); }
	if(!j.is_object())
		throw std::runtime_error("profile must be a JSON object");
	if(Get<std::string>(j, "format", "remoteshift-profile") != "remoteshift-profile")
		throw std::runtime_error("not a RemoteShift profile");
	if(Get<int>(j, "version", 1) > 1)
		throw std::runtime_error("profile was made by a newer RemoteShift version");

	Profile p;
	p.name = Get<std::string>(j, "name", "Unnamed");
	p.game = Get<std::string>(j, "game", "");
	p.description = Get<std::string>(j, "description", "");
	p.default_mode = Get<std::string>(j, "default_mode", "");
	p.wheel_pulse_ms = Get(j, "wheel_pulse_ms", p.wheel_pulse_ms);
	p.wheel_gap_ms = Get(j, "wheel_gap_ms", p.wheel_gap_ms);
	if(!j.contains("modes") || !j["modes"].is_array() || j["modes"].empty())
		throw std::runtime_error("profile has no modes");
	for(const auto &jm : j["modes"])
	{
		Mode m;
		m.name = Get<std::string>(jm, "name", "");
		if(m.name.empty())
			throw std::runtime_error("a mode is missing its name");
		const std::string ctx = "mode '" + m.name + "'";
		m.mouse = MouseFromJson(jm.value("mouse", json::object()));
		m.left_keys = KeyStickFromJson(jm.value("left_keys", json::object()));
		m.right_keys = KeyStickFromJson(jm.value("right_keys", json::object()));
		m.trigger_ramp_ms = Get(jm, "trigger_ramp_ms", 0.0f);
		if(jm.contains("bindings"))
		{
			if(!jm["bindings"].is_array())
				throw std::runtime_error(ctx + ": bindings must be an array");
			int i = 0;
			for(const auto &jb : jm["bindings"])
			{
				const std::string bctx = ctx + " binding #" + std::to_string(++i);
				Binding b;
				b.key = ParseKey(jb.value("key", json()), bctx);
				if(jb.contains("modifiers"))
					for(const auto &mk : jb["modifiers"])
						b.modifiers.push_back(ParseKey(mk, bctx));
				b.action = ActionFromJson(jb.value("action", json::object()), bctx);
				b.bind_mode = BindModeFromName(Get<std::string>(jb, "bind_mode", "hold"));
				b.turbo_hz = Get(jb, "turbo_hz", b.turbo_hz);
				m.bindings.push_back(std::move(b));
			}
		}
		p.modes.push_back(std::move(m));
	}
	if(p.default_mode.empty())
		p.default_mode = p.modes.front().name;
	return p;
}

// ---------------------------------------------------------------- validation & description

std::vector<std::string> ValidateProfile(const Profile &p)
{
	std::vector<std::string> out;
	if(p.modes.empty())
		out.push_back("Profile has no modes.");
	std::set<std::string> names;
	for(const auto &m : p.modes)
	{
		if(!names.insert(m.name).second)
			out.push_back("Duplicate mode name '" + m.name + "'.");
	}
	if(!p.default_mode.empty() && !p.FindMode(p.default_mode))
		out.push_back("Default mode '" + p.default_mode + "' does not exist.");
	for(const auto &m : p.modes)
	{
		auto bad = [&](float v, float lo, float hi, const char *what) {
			if(!(v >= lo && v <= hi))
				out.push_back("Mode '" + m.name + "': " + what + " must be between " + std::to_string(lo).substr(0, 4) + " and " + std::to_string(hi).substr(0, 5) + ".");
		};
		bad(m.mouse.sensitivity, 0.01f, 50.0f, "sensitivity");
		bad(m.mouse.smoothing, 0.0f, 1.0f, "smoothing");
		bad(m.mouse.deadzone, 0.0f, 0.5f, "deadzone");
		bad(m.mouse.anti_deadzone, 0.0f, 0.9f, "anti-deadzone");
		bad(m.mouse.outer_limit, 0.1f, 1.0f, "outer limit");
		bad(m.mouse.square_saturation, 0.0f, 1.0f, "square saturation");
		bad(m.mouse.curve_exponent, 0.1f, 5.0f, "curve exponent");
		bad(m.mouse.sample_window_ms, 1.0f, 100.0f, "sample window");
		for(const auto &b : m.bindings)
		{
			if((b.action.type == ActionType::SetMode || b.action.type == ActionType::HoldMode) && !p.FindMode(b.action.mode))
				out.push_back("Mode '" + m.name + "': binding " + DescribeBinding(b) + " switches to missing mode '" + b.action.mode + "'.");
		}
	}
	return out;
}

std::string DescribeBinding(const Binding &b)
{
	std::string s;
	for(Key k : b.modifiers)
	{
		s += KeyName(k);
		s += " + ";
	}
	s += KeyName(b.key);
	return s;
}

std::string DescribeAction(const Action &a)
{
	std::ostringstream s;
	switch(a.type)
	{
		case ActionType::Button: s << PadButtonName(a.button); break;
		case ActionType::L2: s << "L2"; break;
		case ActionType::R2: s << "R2"; break;
		case ActionType::LeftStickUp: s << "Left stick up"; break;
		case ActionType::LeftStickDown: s << "Left stick down"; break;
		case ActionType::LeftStickLeft: s << "Left stick left"; break;
		case ActionType::LeftStickRight: s << "Left stick right"; break;
		case ActionType::RightStickUp: s << "Right stick up"; break;
		case ActionType::RightStickDown: s << "Right stick down"; break;
		case ActionType::RightStickLeft: s << "Right stick left"; break;
		case ActionType::RightStickRight: s << "Right stick right"; break;
		case ActionType::SetMode: return "Switch to " + a.mode;
		case ActionType::HoldMode: return "Hold for " + a.mode;
		case ActionType::CycleMode: return "Next mode";
		case ActionType::AdsHold: return "ADS sensitivity";
		case ActionType::SensitivityScale: s << "Sensitivity x" << a.value; return s.str();
		case ActionType::ToggleCapture: return "Capture / release mouse";
		case ActionType::None: return "Unbound";
	}
	if(a.type != ActionType::Button && std::fabs(a.value - 1.0f) > 0.001f)
		s << " (" << (int)std::lround(a.value * 100) << "%)";
	return s.str();
}

} // namespace rs
