// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// RemoteShift input engine unit tests. Self-contained; run via ctest or directly.
#include "rsinput/engine.h"
#include "rsinput/keycodes.h"
#include "rsinput/mousestick.h"
#include "rsinput/profile.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

using namespace rs;

static int g_failed = 0, g_checks = 0;
static const char *g_test = "";

#define CHECK(cond) do { g_checks++; if(!(cond)) { g_failed++; std::printf("  FAIL %s:%d [%s] %s\n", __FILE__, __LINE__, g_test, #cond); } } while(0)
#define CHECK_NEAR(a, b, eps) do { g_checks++; double _a = (a), _b = (b); if(std::fabs(_a - _b) > (eps)) { g_failed++; std::printf("  FAIL %s:%d [%s] %s = %g, expected %g (+-%g)\n", __FILE__, __LINE__, g_test, #a, _a, _b, (double)(eps)); } } while(0)

static std::vector<std::pair<const char *, std::function<void()>>> &Tests()
{
	static std::vector<std::pair<const char *, std::function<void()>>> t;
	return t;
}
struct Reg { Reg(const char *n, std::function<void()> f) { Tests().push_back({n, f}); } };
#define TEST(name) static void name(); static Reg reg_##name(#name, name); static void name()

static std::string g_preset_dir = RSINPUT_PRESET_DIR;

static std::string ReadFile(const std::string &path)
{
	std::ifstream f(path, std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

// ------------------------------------------------------------------ helpers

static Binding B(Key k, Action a, std::vector<Key> mods = {}, BindMode bm = BindMode::Hold)
{
	Binding b;
	b.key = k;
	b.action = a;
	b.modifiers = mods;
	b.bind_mode = bm;
	return b;
}
static Action Btn(PadButton p) { Action a; a.type = ActionType::Button; a.button = p; return a; }
static Action Act(ActionType t, float v = 1.0f, std::string mode = "") { Action a; a.type = t; a.value = v; a.mode = mode; return a; }

static Profile OneMode(std::vector<Binding> binds, MouseConfig mc = MouseConfig())
{
	Profile p;
	p.name = "test";
	Mode m;
	m.name = "M";
	m.bindings = binds;
	m.mouse = mc;
	p.modes.push_back(m);
	p.default_mode = "M";
	return p;
}

static MouseConfig PlainMouse()
{
	MouseConfig m;
	m.smoothing = 0.0f;
	m.anti_deadzone = 0.0f;
	m.curve = CurveType::Linear;
	m.square_saturation = 0.0f;
	m.sample_window_ms = 8.0f;
	return m;
}

// Feeds constant mouse motion at `hz` polling for `ms`, ticking every 4 ms. Returns the
// right-stick X samples observed after the first 50 ms (steady state).
static std::vector<int> g_last_ry;
static std::vector<int> DriveMouse(Engine &e, double counts_per_sec, int hz, int ms, uint64_t &t, double jitter_us = 0, double y_ratio = 0.0)
{
	g_last_ry.clear();
	double carry_y = 0;
	std::vector<int> out;
	const double ev_period = 1e6 / hz;
	double next_ev = (double)t;
	const uint64_t end = t + (uint64_t)ms * 1000;
	unsigned rng = 12345;
	double carry = 0;
	uint64_t start = t;
	for(; t < end; t += 4000)
	{
		while(next_ev <= (double)t)
		{
			carry += counts_per_sec / hz;
			int c = (int)carry;
			carry -= c;
			carry_y += counts_per_sec * y_ratio / hz;
			int cy = (int)carry_y;
			carry_y -= cy;
			e.MouseDelta(c, cy, (uint64_t)next_ev);
			rng = rng * 1103515245u + 12345u;
			double j = jitter_us > 0 ? ((double)((rng >> 8) % 1000) / 1000.0 - 0.5) * 2 * jitter_us : 0;
			next_ev += ev_period + j;
		}
		PadState s = e.Tick(t);
		if(t - start >= 50000)
		{
			out.push_back(s.rx);
			g_last_ry.push_back(s.ry);
		}
	}
	return out;
}

static double Mean(const std::vector<int> &v) { double s = 0; for(int x : v) s += x; return v.empty() ? 0 : s / v.size(); }
static double StdDev(const std::vector<int> &v) { double m = Mean(v), s = 0; for(int x : v) s += (x - m) * (x - m); return v.empty() ? 0 : std::sqrt(s / v.size()); }

// ------------------------------------------------------------------ tests

TEST(key_names_roundtrip)
{
	for(int i = 1; i < kKeyCount; i++)
	{
		Key k = static_cast<Key>(i);
		CHECK(KeyFromName(KeyName(k)) == k);
	}
	CHECK(KeyFromName("shift") == Key::LShift);
	CHECK(KeyFromName("Mouse4") == Key::MouseX1);
	CHECK(KeyFromName("nonsense") == Key::None);
	CHECK(IsWheelKey(Key::WheelDown));
	CHECK(!IsWheelKey(Key::MouseLeft));
}

TEST(button_hold_and_release)
{
	Engine e;
	e.SetProfile(OneMode({B(Key::Space, Btn(PadButton::Cross))}));
	CHECK(e.Tick(1000).buttons == 0);
	e.KeyDown(Key::Space, 2000);
	CHECK(e.Tick(4000).buttons == (uint32_t)PadButton::Cross);
	e.KeyUp(Key::Space, 5000);
	CHECK(e.Tick(8000).buttons == 0);
}

TEST(multiple_bindings_per_button_and_per_key)
{
	Engine e;
	e.SetProfile(OneMode({B(Key::Enter, Btn(PadButton::Cross)), B(Key::Space, Btn(PadButton::Cross)),
		B(Key::E, Btn(PadButton::Triangle)), B(Key::E, Btn(PadButton::R1))}));
	e.KeyDown(Key::Space, 0);
	CHECK(e.Tick(1000).buttons == (uint32_t)PadButton::Cross);
	e.KeyUp(Key::Space, 0);
	e.KeyDown(Key::Enter, 0);
	CHECK(e.Tick(2000).buttons == (uint32_t)PadButton::Cross);
	e.KeyUp(Key::Enter, 0);
	e.KeyDown(Key::E, 0);
	CHECK(e.Tick(3000).buttons == ((uint32_t)PadButton::Triangle | (uint32_t)PadButton::R1));
}

TEST(modifier_chord_specificity)
{
	Engine e;
	e.SetProfile(OneMode({B(Key::W, Act(ActionType::LeftStickUp, 1.0f)), B(Key::W, Act(ActionType::LeftStickUp, 0.5f), {Key::LAlt}),
		B(Key::E, Btn(PadButton::DpadRight)), B(Key::E, Btn(PadButton::Square), {Key::LCtrl})}));
	e.KeyDown(Key::W, 0);
	CHECK(e.Tick(1000).ly == -32767);
	e.KeyDown(Key::LAlt, 0);
	CHECK_NEAR(e.Tick(2000).ly, -16384, 2);
	e.KeyDown(Key::LCtrl, 0);
	e.KeyDown(Key::E, 0);
	PadState s = e.Tick(3000);
	CHECK(s.buttons == (uint32_t)PadButton::Square); // plain E suppressed while Ctrl+E is held
	e.KeyUp(Key::LCtrl, 0);
	CHECK(e.Tick(4000).buttons == (uint32_t)PadButton::DpadRight);
}

TEST(wasd_diagonal_and_socd)
{
	Profile p = OneMode({B(Key::W, Act(ActionType::LeftStickUp)), B(Key::S, Act(ActionType::LeftStickDown)),
		B(Key::A, Act(ActionType::LeftStickLeft)), B(Key::D, Act(ActionType::LeftStickRight))});
	Engine e;
	e.SetProfile(p);
	e.KeyDown(Key::W, 0);
	e.KeyDown(Key::D, 0);
	PadState s = e.Tick(1000);
	CHECK_NEAR(s.lx, 23170, 2); // circular diagonal: 32767 / sqrt(2)
	CHECK_NEAR(s.ly, -23170, 2);
	e.KeyDown(Key::A, 0); // last wins: A pressed after D
	s = e.Tick(2000);
	CHECK(s.lx < 0);
	p.modes[0].left_keys.socd = Socd::Neutral;
	e.SetProfile(p);
	CHECK(e.Tick(3000).lx == 0);
	p.modes[0].left_keys.socd = Socd::FirstWins;
	p.modes[0].left_keys.circular_diagonals = false;
	e.SetProfile(p);
	s = e.Tick(4000);
	CHECK(s.lx == 32767);
	CHECK(s.ly == -32767);
}

static PadState Advance(Engine &e, uint64_t &t, uint64_t ms)
{
	PadState s;
	for(uint64_t end = t + ms * 1000; t < end;)
	{
		t += 4000;
		s = e.Tick(t);
	}
	return s;
}

TEST(gradual_steering_ramp)
{
	Profile p = OneMode({B(Key::D, Act(ActionType::LeftStickRight)), B(Key::A, Act(ActionType::LeftStickLeft))});
	p.modes[0].left_keys.ramp_up_ms = 200.0f;
	p.modes[0].left_keys.ramp_down_ms = 100.0f;
	Engine e;
	e.SetProfile(p);
	uint64_t t = 0;
	e.Tick(t);
	e.KeyDown(Key::D, t);
	CHECK_NEAR(Advance(e, t, 100).lx, 16384, 400); // half way after 100 ms
	CHECK_NEAR(Advance(e, t, 100).lx, 32767, 1);
	e.KeyUp(Key::D, t);
	CHECK_NEAR(Advance(e, t, 48).lx, 0.52 * 32767, 400); // returns at the faster down rate
	CHECK(Advance(e, t, 52).lx == 0);
	// Reversal: full right -> press left: back through centre at the down rate, then out at the up rate.
	e.KeyDown(Key::D, t);
	Advance(e, t, 200);
	e.KeyUp(Key::D, t);
	e.KeyDown(Key::A, t);
	CHECK_NEAR(Advance(e, t, 100).lx, 0, 400);
	CHECK_NEAR(Advance(e, t, 100).lx, -16384, 400);
}

TEST(trigger_values_and_ramp)
{
	Profile p = OneMode({B(Key::W, Act(ActionType::R2)), B(Key::S, Act(ActionType::L2, 0.5f))});
	Engine e;
	e.SetProfile(p);
	e.KeyDown(Key::W, 0);
	e.KeyDown(Key::S, 0);
	PadState s = e.Tick(1000);
	CHECK(s.r2 == 255);
	CHECK(s.l2 == 128);
	p.modes[0].trigger_ramp_ms = 100.0f;
	Engine e2;
	e2.SetProfile(p);
	e2.Tick(0);
	e2.KeyDown(Key::W, 0);
	CHECK_NEAR(e2.Tick(50000).r2, 128, 2);
	CHECK(e2.Tick(100000).r2 == 255);
}

TEST(mouse_steady_motion_and_recentre)
{
	Engine e;
	MouseConfig mc = PlainMouse();
	e.SetProfile(OneMode({}, mc));
	uint64_t t = 0;
	auto half = DriveMouse(e, 1000.0, 1000, 200, t); // 1000 cps = half deflection at sens 1
	CHECK_NEAR(Mean(half), 16384, 400);
	CHECK(StdDev(half) < 300);
	// stop moving: must return exactly to centre within one window
	t += 12000;
	CHECK(e.Tick(t).rx == 0);
	auto full = DriveMouse(e, 5000.0, 1000, 200, t);
	CHECK(Mean(full) > 32000); // saturates
}

TEST(mouse_low_polling_rate_is_stable)
{
	// A 125 Hz office mouse should produce the same deflection as a 1000 Hz gaming mouse.
	MouseConfig mc = PlainMouse();
	mc.sample_window_ms = 12.0f;
	mc.smoothing = 0.15f;
	Engine fast, slow;
	fast.SetProfile(OneMode({}, mc));
	slow.SetProfile(OneMode({}, mc));
	uint64_t t1 = 0, t2 = 0;
	auto a = DriveMouse(fast, 1200.0, 1000, 400, t1, 50);
	auto b = DriveMouse(slow, 1200.0, 125, 400, t2, 400);
	CHECK_NEAR(Mean(a), Mean(b), 0.02 * Mean(a));
	CHECK(StdDev(b) < 0.04 * Mean(b));
	std::printf("    1000Hz mean=%.0f sd=%.0f | 125Hz mean=%.0f sd=%.0f\n", Mean(a), StdDev(a), Mean(b), StdDev(b));
}

TEST(mouse_axis_sensitivity_and_invert)
{
	MouseConfig mc = PlainMouse();
	mc.sens_x = 1.0f;
	mc.sens_y = 0.5f;
	Engine e;
	e.SetProfile(OneMode({}, mc));
	uint64_t t = 0;
	auto rx = DriveMouse(e, 800.0, 1000, 150, t, 0, 1.0); // 800 cps on both axes
	CHECK_NEAR(Mean(rx), 0.4 * 32767, 0.025 * 0.4 * 32767);
	CHECK_NEAR(Mean(g_last_ry), 0.2 * 32767, 0.025 * 0.2 * 32767);
	mc.invert_y = true;
	e.SetProfile(OneMode({}, mc));
	DriveMouse(e, 800.0, 1000, 150, t, 0, 1.0);
	CHECK_NEAR(Mean(g_last_ry), -0.2 * 32767, 0.025 * 0.2 * 32767);
}

TEST(ads_multiplier_follows_l2_and_hold)
{
	MouseConfig mc = PlainMouse();
	mc.ads_multiplier = 0.5f;
	Engine e;
	e.SetProfile(OneMode({B(Key::MouseRight, Act(ActionType::L2)), B(Key::LShift, Act(ActionType::AdsHold)),
		B(Key::MouseX1, Act(ActionType::SensitivityScale, 0.25f))}, mc));
	uint64_t t = 0;
	CHECK_NEAR(Mean(DriveMouse(e, 1000.0, 1000, 150, t)), 16384, 200);
	e.KeyDown(Key::MouseRight, t);
	CHECK_NEAR(Mean(DriveMouse(e, 1000.0, 1000, 150, t)), 8192, 200);
	CHECK(e.Telemetry().ads_active);
	e.KeyUp(Key::MouseRight, t);
	e.KeyDown(Key::LShift, t);
	CHECK_NEAR(Mean(DriveMouse(e, 1000.0, 1000, 150, t)), 8192, 200);
	e.KeyUp(Key::LShift, t);
	e.KeyDown(Key::MouseX1, t);
	CHECK_NEAR(Mean(DriveMouse(e, 1000.0, 1000, 150, t)), 4096, 200);
	CHECK(!e.Telemetry().ads_active);
}

TEST(shape_deadzone_antideadzone_curve)
{
	MouseConfig mc = PlainMouse();
	mc.anti_deadzone = 0.2f;
	Vec2 o = MouseStick::Shape(mc, {0.01f, 0.0f});
	CHECK(o.x >= 0.2f && o.x < 0.21f); // tiny motion escapes the game's deadzone
	o = MouseStick::Shape(mc, {1.0f, 0.0f});
	CHECK_NEAR(o.x, 1.0, 1e-5);
	o = MouseStick::Shape(mc, {0.0f, 0.0f});
	CHECK(o.x == 0.0f && o.y == 0.0f);
	mc.deadzone = 0.1f;
	o = MouseStick::Shape(mc, {0.05f, 0.0f});
	CHECK(o.x == 0.0f);
	mc.deadzone = 0.0f;
	mc.anti_deadzone = 0.0f;
	mc.curve = CurveType::Power;
	mc.curve_exponent = 2.0f;
	CHECK_NEAR(MouseStick::Shape(mc, {0.5f, 0.0f}).x, 0.25, 1e-5);
	mc.curve = CurveType::Custom;
	mc.curve_points = {{0.5f, 0.8f}};
	CHECK_NEAR(MouseStick::Curve(mc, 0.25f), 0.4, 1e-5);
	CHECK_NEAR(MouseStick::Curve(mc, 0.75f), 0.9, 1e-5);
	mc.curve = CurveType::Linear;
	mc.outer_limit = 0.8f;
	CHECK_NEAR(MouseStick::Shape(mc, {2.0f, 0.0f}).x, 0.8, 1e-5);
}

TEST(axial_anti_deadzone)
{
	MouseConfig mc = PlainMouse();
	mc.anti_deadzone = 0.25f;
	mc.anti_deadzone_shape = DeadzoneShape::Axial;
	Vec2 o = MouseStick::Shape(mc, {0.1f, 0.1f});
	CHECK(o.x > 0.25f && o.y > 0.25f);
	o = MouseStick::Shape(mc, {0.5f, 0.005f}); // negligible vertical component is not boosted
	CHECK(std::fabs(o.y) < 0.02f);
}

TEST(square_saturation_reaches_corners)
{
	MouseConfig mc = PlainMouse();
	mc.square_saturation = 1.0f;
	Vec2 o = MouseStick::Shape(mc, {1.0f, 1.0f});
	CHECK_NEAR(o.x, 1.0, 0.01);
	CHECK_NEAR(o.y, 1.0, 0.01);
	o = MouseStick::Shape(mc, {1.0f, 0.0f});
	CHECK_NEAR(o.x, 1.0, 0.001);
	CHECK_NEAR(o.y, 0.0, 0.001);
	mc.square_saturation = 0.0f;
	o = MouseStick::Shape(mc, {1.0f, 1.0f});
	CHECK_NEAR(o.x, 0.7071, 0.001);
}

TEST(acceleration_increases_fast_motion_only)
{
	MouseConfig mc = PlainMouse();
	mc.acceleration = 1.0f;
	mc.accel_cap = 1.0f;
	Engine e;
	e.SetProfile(OneMode({}, mc));
	uint64_t t = 0;
	CHECK_NEAR(Mean(DriveMouse(e, 125.0, 1000, 200, t)), 0.0625 * 1.0625 * 32767, 60);
	CHECK_NEAR(Mean(DriveMouse(e, 500.0, 1000, 200, t)), 0.25 * 1.25 * 32767, 60);
}

TEST(smoothing_lags_then_converges)
{
	MouseConfig mc = PlainMouse();
	mc.smoothing = 0.5f; // tau 30 ms
	Engine e;
	e.SetProfile(OneMode({}, mc));
	uint64_t t = 0;
	e.Tick(t);
	std::vector<int> out;
	for(int i = 0; i < 100; i++)
	{
		t += 1000;
		e.MouseDelta(1, 0, t);
		if(i % 4 == 3)
			out.push_back(e.Tick(t).rx);
	}
	CHECK(out.front() < out.back());
	CHECK(out.back() > 0.93 * 16384 && out.back() <= 16384 + 50);
	CHECK(out[2] < 0.6 * 16384); // visibly lagging early on
}

TEST(wheel_pulses)
{
	Profile p = OneMode({B(Key::WheelDown, Btn(PadButton::DpadRight))});
	p.wheel_pulse_ms = 40;
	p.wheel_gap_ms = 30;
	Engine e;
	e.SetProfile(p);
	e.Wheel(-3, 0, 0);
	int presses = 0;
	bool prev = false;
	for(uint64_t t = 0; t < 400000; t += 4000)
	{
		bool on = e.Tick(t).buttons != 0;
		if(on && !prev)
			presses++;
		prev = on;
	}
	CHECK(presses == 3);
	CHECK(!prev);
}

TEST(toggle_and_turbo)
{
	Engine e;
	e.SetProfile(OneMode({B(Key::LCtrl, Btn(PadButton::L3), {}, BindMode::Toggle),
		B(Key::F, Btn(PadButton::Square), {}, BindMode::Turbo)}));
	e.KeyDown(Key::LCtrl, 0);
	CHECK(e.Tick(1000).buttons == (uint32_t)PadButton::L3);
	e.KeyUp(Key::LCtrl, 0);
	CHECK(e.Tick(2000).buttons == (uint32_t)PadButton::L3); // stays on
	e.KeyDown(Key::LCtrl, 0);
	CHECK(e.Tick(3000).buttons == 0);
	e.KeyUp(Key::LCtrl, 0);
	e.Tick(4000);
	// turbo at 10 Hz: on for 50 ms, off for 50 ms
	e.KeyDown(Key::F, 0);
	int transitions = 0;
	bool prev = false;
	for(uint64_t t = 10000; t < 510000; t += 2000)
	{
		bool on = (e.Tick(t).buttons & (uint32_t)PadButton::Square) != 0;
		if(on != prev)
			transitions++;
		prev = on;
	}
	CHECK(transitions >= 9 && transitions <= 11);
}

TEST(mode_switching)
{
	Profile p;
	Mode a, b;
	a.name = "Foot";
	a.bindings = {B(Key::W, Act(ActionType::LeftStickUp)), B(Key::F, Btn(PadButton::Triangle)), B(Key::F, Act(ActionType::SetMode, 1, "Car")),
		B(Key::LAlt, Act(ActionType::HoldMode, 1, "Car"))};
	b.name = "Car";
	b.bindings = {B(Key::W, Act(ActionType::R2)), B(Key::F, Btn(PadButton::Triangle)), B(Key::F, Act(ActionType::SetMode, 1, "Foot")),
		B(Key::Num1, Act(ActionType::CycleMode))};
	p.modes = {a, b};
	p.default_mode = "Foot";
	Engine e;
	e.SetProfile(p);
	e.TakeEvents();
	CHECK(e.ActiveMode() == "Foot");
	e.KeyDown(Key::F, 0);
	PadState s = e.Tick(1000);
	CHECK(e.ActiveMode() == "Car");
	CHECK(s.buttons == (uint32_t)PadButton::Triangle); // press continues across the switch
	s = e.Tick(2000);
	CHECK(e.ActiveMode() == "Car"); // no bounce back while F is still held
	e.KeyUp(Key::F, 0);
	e.Tick(3000);
	e.KeyDown(Key::W, 0);
	CHECK(e.Tick(4000).r2 == 255);
	e.KeyDown(Key::F, 0);
	e.Tick(5000);
	CHECK(e.ActiveMode() == "Foot");
	CHECK(e.Tick(6000).ly == -32767);
	e.KeyUp(Key::F, 0);
	// Hold mode
	e.KeyDown(Key::LAlt, 0);
	e.Tick(7000);
	CHECK(e.ActiveMode() == "Car");
	e.KeyUp(Key::LAlt, 0);
	e.Tick(8000);
	CHECK(e.ActiveMode() == "Foot");
	auto ev = e.TakeEvents();
	int changes = 0;
	for(auto &x : ev)
		if(x.type == EngineEvent::Type::ModeChanged)
			changes++;
	CHECK(changes == 4);
	CHECK(e.SetMode("Car"));
	CHECK(!e.SetMode("Nope"));
	e.KeyUp(Key::W, 0);
	e.KeyDown(Key::Num1, 0);
	e.Tick(9000);
	CHECK(e.ActiveMode() == "Foot");
}

TEST(release_all_clears_everything)
{
	Engine e;
	e.SetProfile(OneMode({B(Key::Space, Btn(PadButton::Cross)), B(Key::W, Act(ActionType::LeftStickUp))}, PlainMouse()));
	e.KeyDown(Key::Space, 0);
	e.KeyDown(Key::W, 0);
	e.MouseDelta(10, 10, 0);
	PadState s = e.Tick(1000);
	CHECK(s.buttons && s.ly && s.rx);
	e.ReleaseAll();
	s = e.Tick(2000);
	CHECK(s == PadState());
}

TEST(capture_toggle_event)
{
	Engine e;
	e.SetProfile(OneMode({B(Key::F8, Act(ActionType::ToggleCapture))}));
	e.KeyDown(Key::F8, 0);
	e.Tick(1000);
	e.Tick(2000);
	auto ev = e.TakeEvents();
	int n = 0;
	for(auto &x : ev)
		if(x.type == EngineEvent::Type::CaptureToggleRequested)
			n++;
	CHECK(n == 1);
}

TEST(profile_json_roundtrip)
{
	Profile p = OneMode({B(Key::W, Act(ActionType::LeftStickUp, 0.5f), {Key::LAlt}),
		B(Key::F, Act(ActionType::SetMode, 1, "M")), B(Key::Q, Btn(PadButton::L1), {}, BindMode::Turbo)});
	p.modes[0].mouse.curve = CurveType::Custom;
	p.modes[0].mouse.curve_points = {{0.3f, 0.5f}, {0.7f, 0.9f}};
	p.modes[0].left_keys.socd = Socd::Neutral;
	p.game = "Some Game";
	std::string j = ProfileToJson(p);
	Profile q = ProfileFromJson(j);
	CHECK(ProfileToJson(q) == j);
	CHECK(q.modes[0].bindings[0].modifiers.size() == 1);
	CHECK(q.modes[0].bindings[2].bind_mode == BindMode::Turbo);
	CHECK(q.modes[0].mouse.curve_points.size() == 2);
	CHECK(ValidateProfile(q).empty());
}

TEST(profile_json_errors_are_readable)
{
	auto expect_error = [](const std::string &json, const std::string &needle) {
		try { ProfileFromJson(json); }
		catch(const std::exception &ex) { return std::string(ex.what()).find(needle) != std::string::npos; }
		return false;
	};
	CHECK(expect_error("{", "not valid JSON"));
	CHECK(expect_error("{\"modes\": []}", "no modes"));
	CHECK(expect_error("{\"modes\": [{\"name\":\"a\",\"bindings\":[{\"key\":\"Wat\",\"action\":{\"type\":\"L2\"}}]}]}", "unknown key 'Wat'"));
	CHECK(expect_error("{\"modes\": [{\"name\":\"a\",\"bindings\":[{\"key\":\"W\",\"action\":{\"type\":\"Button\",\"button\":\"Z\"}}]}]}", "unknown controller button"));
	CHECK(expect_error("{\"format\":\"other\",\"modes\":[{\"name\":\"a\"}]}", "not a RemoteShift profile"));
	Profile p = OneMode({B(Key::F, Act(ActionType::SetMode, 1, "Ghost"))});
	auto problems = ValidateProfile(p);
	CHECK(problems.size() == 1);
}

TEST(bundled_presets_load_and_validate)
{
	for(const char *name : {"gta6.json", "default.json"})
	{
		std::string text = ReadFile(g_preset_dir + "/" + name);
		CHECK(!text.empty());
		Profile p;
		bool ok = true;
		try { p = ProfileFromJson(text); }
		catch(const std::exception &ex) { ok = false; std::printf("    %s: %s\n", name, ex.what()); }
		CHECK(ok);
		auto problems = ValidateProfile(p);
		for(auto &s : problems)
			std::printf("    %s: %s\n", name, s.c_str());
		CHECK(problems.empty());
	}
	Profile gta = ProfileFromJson(ReadFile(g_preset_dir + "/gta6.json"));
	CHECK(gta.modes.size() == 5);
	CHECK(gta.FindMode("On Foot") && gta.FindMode("Driving") && gta.FindMode("Flying") && gta.FindMode("Menus"));
	// Drive the real preset: F enters a vehicle and switches to Driving, W then accelerates.
	Engine e;
	e.SetProfile(gta);
	e.KeyDown(Key::W, 0);
	CHECK(e.Tick(1000).ly == -32767);
	e.KeyDown(Key::F, 1000);
	PadState s = e.Tick(2000);
	CHECK(e.ActiveMode() == "Driving");
	CHECK(s.buttons & (uint32_t)PadButton::Triangle);
	e.KeyUp(Key::F, 3000);
	e.KeyUp(Key::W, 3000);
	e.Tick(4000);
	e.KeyDown(Key::W, 5000);
	e.Tick(6000);
	CHECK(e.Tick(200000).r2 == 255);
	e.KeyDown(Key::Escape, 200000);
	s = e.Tick(204000);
	CHECK(e.ActiveMode() == "Menus");
	CHECK(s.buttons & (uint32_t)PadButton::Options);
	e.KeyUp(Key::Escape, 205000);
	e.KeyDown(Key::F1, 206000);
	e.Tick(208000);
	CHECK(e.ActiveMode() == "On Foot");
}

int main(int argc, char **argv)
{
	if(argc > 1)
		g_preset_dir = argv[1];
	for(auto &t : Tests())
	{
		g_test = t.first;
		int before = g_failed;
		t.second();
		std::printf("%s %s\n", g_failed == before ? "[ ok ]" : "[FAIL]", t.first);
	}
	std::printf("\n%d checks, %d failed, %zu tests\n", g_checks, g_failed, Tests().size());
	return g_failed ? 1 : 0;
}
