// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "rsinputcontroller.h"
#include "rsrawmouse.h"

#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <cstring>

using rs::Key;

// ---------------------------------------------------------------- key translation

// Windows Set-1 scan codes as reported by Qt (bit 8 = extended key).
static Key FromWindowsScanCode(quint32 sc)
{
	switch(sc)
	{
		case 0x01: return Key::Escape;
		case 0x02: return Key::Num1; case 0x03: return Key::Num2; case 0x04: return Key::Num3; case 0x05: return Key::Num4;
		case 0x06: return Key::Num5; case 0x07: return Key::Num6; case 0x08: return Key::Num7; case 0x09: return Key::Num8;
		case 0x0A: return Key::Num9; case 0x0B: return Key::Num0; case 0x0C: return Key::Minus; case 0x0D: return Key::Equals;
		case 0x0E: return Key::Backspace; case 0x0F: return Key::Tab;
		case 0x10: return Key::Q; case 0x11: return Key::W; case 0x12: return Key::E; case 0x13: return Key::R; case 0x14: return Key::T;
		case 0x15: return Key::Y; case 0x16: return Key::U; case 0x17: return Key::I; case 0x18: return Key::O; case 0x19: return Key::P;
		case 0x1A: return Key::LBracket; case 0x1B: return Key::RBracket; case 0x1C: return Key::Enter; case 0x11C: return Key::KpEnter;
		case 0x1D: return Key::LCtrl; case 0x11D: return Key::RCtrl;
		case 0x1E: return Key::A; case 0x1F: return Key::S; case 0x20: return Key::D; case 0x21: return Key::F; case 0x22: return Key::G;
		case 0x23: return Key::H; case 0x24: return Key::J; case 0x25: return Key::K; case 0x26: return Key::L;
		case 0x27: return Key::Semicolon; case 0x28: return Key::Apostrophe; case 0x29: return Key::Grave;
		case 0x2A: return Key::LShift; case 0x2B: return Key::Backslash;
		case 0x2C: return Key::Z; case 0x2D: return Key::X; case 0x2E: return Key::C; case 0x2F: return Key::V; case 0x30: return Key::B;
		case 0x31: return Key::N; case 0x32: return Key::M; case 0x33: return Key::Comma; case 0x34: return Key::Period;
		case 0x35: return Key::Slash; case 0x135: return Key::KpDivide; case 0x36: return Key::RShift;
		case 0x37: return Key::KpMultiply; case 0x137: return Key::PrintScreen;
		case 0x38: return Key::LAlt; case 0x138: return Key::RAlt; case 0x39: return Key::Space; case 0x3A: return Key::CapsLock;
		case 0x3B: return Key::F1; case 0x3C: return Key::F2; case 0x3D: return Key::F3; case 0x3E: return Key::F4; case 0x3F: return Key::F5;
		case 0x40: return Key::F6; case 0x41: return Key::F7; case 0x42: return Key::F8; case 0x43: return Key::F9; case 0x44: return Key::F10;
		case 0x46: return Key::ScrollLock;
		case 0x47: return Key::Kp7; case 0x147: return Key::Home; case 0x48: return Key::Kp8; case 0x148: return Key::Up;
		case 0x49: return Key::Kp9; case 0x149: return Key::PageUp; case 0x4A: return Key::KpSubtract;
		case 0x4B: return Key::Kp4; case 0x14B: return Key::Left; case 0x4C: return Key::Kp5; case 0x4D: return Key::Kp6; case 0x14D: return Key::Right;
		case 0x4E: return Key::KpAdd; case 0x4F: return Key::Kp1; case 0x14F: return Key::End; case 0x50: return Key::Kp2; case 0x150: return Key::Down;
		case 0x51: return Key::Kp3; case 0x151: return Key::PageDown; case 0x52: return Key::Kp0; case 0x152: return Key::Insert;
		case 0x53: return Key::KpDecimal; case 0x153: return Key::Delete; case 0x57: return Key::F11; case 0x58: return Key::F12;
		case 0x15B: return Key::LWin; case 0x15C: return Key::RWin; case 0x15D: return Key::Menu;
		default: return Key::None;
	}
}

// X11 / XCB key codes (evdev + 8).
static Key FromX11KeyCode(quint32 kc)
{
	static const Key row1[] = {Key::Num1, Key::Num2, Key::Num3, Key::Num4, Key::Num5, Key::Num6, Key::Num7, Key::Num8, Key::Num9, Key::Num0, Key::Minus, Key::Equals, Key::Backspace, Key::Tab};
	static const Key row2[] = {Key::Q, Key::W, Key::E, Key::R, Key::T, Key::Y, Key::U, Key::I, Key::O, Key::P, Key::LBracket, Key::RBracket, Key::Enter, Key::LCtrl};
	static const Key row3[] = {Key::A, Key::S, Key::D, Key::F, Key::G, Key::H, Key::J, Key::K, Key::L, Key::Semicolon, Key::Apostrophe, Key::Grave, Key::LShift, Key::Backslash};
	static const Key row4[] = {Key::Z, Key::X, Key::C, Key::V, Key::B, Key::N, Key::M, Key::Comma, Key::Period, Key::Slash, Key::RShift, Key::KpMultiply, Key::LAlt, Key::Space, Key::CapsLock};
	if(kc == 9) return Key::Escape;
	if(kc >= 10 && kc < 10 + 14) return row1[kc - 10];
	if(kc >= 24 && kc < 24 + 14) return row2[kc - 24];
	if(kc >= 38 && kc < 38 + 14) return row3[kc - 38];
	if(kc >= 52 && kc < 52 + 15) return row4[kc - 52];
	if(kc >= 67 && kc <= 76) return static_cast<Key>(static_cast<int>(Key::F1) + (int)(kc - 67));
	switch(kc)
	{
		case 78: return Key::ScrollLock; case 79: return Key::Kp7; case 80: return Key::Kp8; case 81: return Key::Kp9;
		case 82: return Key::KpSubtract; case 83: return Key::Kp4; case 84: return Key::Kp5; case 85: return Key::Kp6;
		case 86: return Key::KpAdd; case 87: return Key::Kp1; case 88: return Key::Kp2; case 89: return Key::Kp3;
		case 90: return Key::Kp0; case 91: return Key::KpDecimal; case 95: return Key::F11; case 96: return Key::F12;
		case 104: return Key::KpEnter; case 105: return Key::RCtrl; case 106: return Key::KpDivide; case 107: return Key::PrintScreen;
		case 108: return Key::RAlt; case 110: return Key::Home; case 111: return Key::Up; case 112: return Key::PageUp;
		case 113: return Key::Left; case 114: return Key::Right; case 115: return Key::End; case 116: return Key::Down;
		case 117: return Key::PageDown; case 118: return Key::Insert; case 119: return Key::Delete; case 127: return Key::Pause;
		case 133: return Key::LWin; case 134: return Key::RWin; case 135: return Key::Menu;
		default: return Key::None;
	}
}

static Key FromQtKey(int k, Qt::KeyboardModifiers mods)
{
	const bool kp = mods.testFlag(Qt::KeypadModifier);
	if(k >= Qt::Key_A && k <= Qt::Key_Z)
		return static_cast<Key>(static_cast<int>(Key::A) + (k - Qt::Key_A));
	if(k >= Qt::Key_0 && k <= Qt::Key_9)
		return static_cast<Key>(static_cast<int>(kp ? Key::Kp0 : Key::Num0) + (k - Qt::Key_0));
	if(k >= Qt::Key_F1 && k <= Qt::Key_F12)
		return static_cast<Key>(static_cast<int>(Key::F1) + (k - Qt::Key_F1));
	switch(k)
	{
		case Qt::Key_Shift: return Key::LShift;
		case Qt::Key_Control: return Key::LCtrl;
		case Qt::Key_Alt: return Key::LAlt;
		case Qt::Key_AltGr: return Key::RAlt;
		case Qt::Key_Meta: return Key::LWin;
		case Qt::Key_Escape: return Key::Escape;
		case Qt::Key_Tab: case Qt::Key_Backtab: return Key::Tab;
		case Qt::Key_CapsLock: return Key::CapsLock;
		case Qt::Key_Space: return Key::Space;
		case Qt::Key_Return: return Key::Enter;
		case Qt::Key_Enter: return Key::KpEnter;
		case Qt::Key_Backspace: return Key::Backspace;
		case Qt::Key_Insert: return Key::Insert;
		case Qt::Key_Delete: return Key::Delete;
		case Qt::Key_Home: return Key::Home;
		case Qt::Key_End: return Key::End;
		case Qt::Key_PageUp: return Key::PageUp;
		case Qt::Key_PageDown: return Key::PageDown;
		case Qt::Key_Up: return Key::Up;
		case Qt::Key_Down: return Key::Down;
		case Qt::Key_Left: return Key::Left;
		case Qt::Key_Right: return Key::Right;
		case Qt::Key_QuoteLeft: case Qt::Key_AsciiTilde: return Key::Grave;
		case Qt::Key_Minus: return kp ? Key::KpSubtract : Key::Minus;
		case Qt::Key_Plus: return Key::KpAdd;
		case Qt::Key_Asterisk: return Key::KpMultiply;
		case Qt::Key_Equal: return Key::Equals;
		case Qt::Key_BracketLeft: return Key::LBracket;
		case Qt::Key_BracketRight: return Key::RBracket;
		case Qt::Key_Backslash: return Key::Backslash;
		case Qt::Key_Semicolon: return Key::Semicolon;
		case Qt::Key_Apostrophe: return Key::Apostrophe;
		case Qt::Key_Comma: return Key::Comma;
		case Qt::Key_Period: return kp ? Key::KpDecimal : Key::Period;
		case Qt::Key_Slash: return kp ? Key::KpDivide : Key::Slash;
		case Qt::Key_Print: return Key::PrintScreen;
		case Qt::Key_ScrollLock: return Key::ScrollLock;
		case Qt::Key_Pause: return Key::Pause;
		case Qt::Key_Menu: return Key::Menu;
		default: return Key::None;
	}
}

rs::Key RsKeyFromQt(int qt_key, quint32 sc, Qt::KeyboardModifiers mods)
{
	Key k = Key::None;
	const QString platform = QGuiApplication::platformName();
#ifdef _WIN32
	(void)platform;
	k = FromWindowsScanCode(sc);
	// Some keyboards/remappers omit the extended bit; trust Qt for navigation keys then.
	if(k >= Key::Kp0 && k <= Key::KpDecimal && !mods.testFlag(Qt::KeypadModifier))
	{
		Key q = FromQtKey(qt_key, mods);
		if(q != Key::None)
			k = q;
	}
#else
	if(platform == QLatin1String("xcb") || platform == QLatin1String("wayland") || platform == QLatin1String("eglfs") || platform == QLatin1String("offscreen"))
		k = FromX11KeyCode(sc);
#endif
	if(k == Key::None)
		k = FromQtKey(qt_key, mods);
	return k;
}

rs::Key RsKeyFromMouseButton(Qt::MouseButton b)
{
	switch(b)
	{
		case Qt::LeftButton: return Key::MouseLeft;
		case Qt::RightButton: return Key::MouseRight;
		case Qt::MiddleButton: return Key::MouseMiddle;
		case Qt::BackButton: return Key::MouseX1;
		case Qt::ForwardButton: return Key::MouseX2;
		default: return Key::None;
	}
}

// ---------------------------------------------------------------- controller

RsInputController::RsInputController(QObject *parent) : QObject(parent)
{
	chiaki_controller_state_set_idle(&state);
	clock.start();
	tick_timer.setTimerType(Qt::PreciseTimer);
	connect(&tick_timer, &QTimer::timeout, this, &RsInputController::Tick);
	SetTickRate(250);
	tick_timer.start();
	raw = new RsRawMouse([this](int dx, int dy) { HandleRawDelta(dx, dy); });
}

RsInputController::~RsInputController()
{
	SetCaptured(false);
	delete raw;
}

void RsInputController::SetProfile(const rs::Profile &profile)
{
	engine.SetProfile(profile);
	emit ModeChanged(ActiveMode());
}

void RsInputController::SetTickRate(int hz)
{
	tick_hz = qBound(60, hz, 1000);
	tick_timer.setInterval(qMax(1, 1000 / tick_hz));
}

void RsInputController::AttachWindow(QWindow *w)
{
	if(window == w)
		return;
	if(window)
		disconnect(window, nullptr, this, nullptr);
	window = w;
	if(!w)
		return;
	auto reclip = [this] { if(raw) raw->Reclip(); };
	connect(w, &QWindow::xChanged, this, reclip);
	connect(w, &QWindow::yChanged, this, reclip);
	connect(w, &QWindow::widthChanged, this, reclip);
	connect(w, &QWindow::heightChanged, this, reclip);
	connect(w, &QWindow::activeChanged, this, [this] {
		if(window && !window->isActive())
			SetCaptured(false);
	});
}

bool RsInputController::UsingRawInput() const
{
	return raw && raw->Active();
}

bool RsInputController::SetMode(const QString &mode)
{
	bool ok = engine.SetMode(mode.toStdString());
	if(ok)
	{
		engine.TakeEvents();
		emit ModeChanged(mode);
	}
	return ok;
}

void RsInputController::SetCaptured(bool capture)
{
	if(capture == captured)
		return;
	if(capture && (!window || !window->isActive()))
		return;
	captured = capture;
	engine.ReleaseAll();
	wheel_remainder_v = wheel_remainder_h = 0;
	if(captured)
	{
		if(!raw->Start(window))
		{
			// Fallback: relative motion by re-centring the pointer (X11, macOS).
			ignore_next_warp = true;
			QCursor::setPos(window->mapToGlobal(QPoint(window->width() / 2, window->height() / 2)));
		}
		window->setCursor(Qt::BlankCursor);
	}
	else
	{
		raw->Stop();
		if(window)
			window->unsetCursor();
	}
	Tick();
	emit CaptureChanged(captured);
}

void RsInputController::ReleaseAll()
{
	engine.ReleaseAll();
	Tick();
}

bool RsInputController::HandleKey(QKeyEvent *event)
{
	const bool press = event->type() == QEvent::KeyPress;
	Key k = RsKeyFromQt(event->key(), event->nativeScanCode(), event->modifiers());
	if(k == Key::None)
		return captured;
	if(k == capture_key)
	{
		if(press && !event->isAutoRepeat())
			SetCaptured(!captured);
		return true;
	}
	if(!captured)
		return false;
	if(event->isAutoRepeat())
		return true;
	if(press)
		engine.KeyDown(k, NowUs());
	else
		engine.KeyUp(k, NowUs());
	Tick(); // send key changes immediately rather than on the next timer tick
	return true;
}

bool RsInputController::HandleMouseButton(QMouseEvent *event)
{
	const bool press = event->type() == QEvent::MouseButtonPress;
	if(!captured)
	{
		// Clicking into the stream captures the mouse, like a PC game.
		if(press && event->button() == Qt::LeftButton)
		{
			SetCaptured(true);
			return true;
		}
		return false;
	}
	Key k = RsKeyFromMouseButton(event->button());
	if(k == Key::None)
		return true;
	if(press)
		engine.KeyDown(k, NowUs());
	else
		engine.KeyUp(k, NowUs());
	Tick();
	return true;
}

bool RsInputController::HandleMouseMove(QMouseEvent *event)
{
	if(!captured)
		return false;
	if(UsingRawInput() || !window)
		return true; // motion comes from WM_INPUT
	const QPoint centre(window->width() / 2, window->height() / 2);
	const QPoint pos = event->position().toPoint();
	if(ignore_next_warp && pos == centre)
	{
		ignore_next_warp = false;
		return true;
	}
	const QPoint d = pos - centre;
	if(d.isNull())
		return true;
	const qreal dpr = window->devicePixelRatio();
	engine.MouseDelta(qRound(d.x() * dpr), qRound(d.y() * dpr), NowUs());
	ignore_next_warp = true;
	QCursor::setPos(window->mapToGlobal(centre));
	return true;
}

bool RsInputController::HandleWheel(QWheelEvent *event)
{
	if(!captured)
		return false;
	wheel_remainder_v += event->angleDelta().y();
	wheel_remainder_h += event->angleDelta().x();
	int v = wheel_remainder_v / 120, h = wheel_remainder_h / 120;
	wheel_remainder_v -= v * 120;
	wheel_remainder_h -= h * 120;
	if(v || h)
		engine.Wheel(v, h, NowUs());
	return true;
}

void RsInputController::HandleRawDelta(int dx, int dy)
{
	if(!captured)
		return;
	engine.MouseDelta(dx, dy, NowUs());
}

void RsInputController::Tick()
{
	const uint64_t now = NowUs();
	if(captured && window && !window->isActive())
		SetCaptured(false); // focus lost (Alt+Tab etc.): never leave keys stuck down
	rs::PadState pad = engine.Tick(now);

	for(const auto &ev : engine.TakeEvents())
	{
		if(ev.type == rs::EngineEvent::Type::ModeChanged)
			emit ModeChanged(QString::fromStdString(ev.mode));
		else if(ev.type == rs::EngineEvent::Type::CaptureToggleRequested)
			QMetaObject::invokeMethod(this, [this] { SetCaptured(!captured); }, Qt::QueuedConnection);
	}

	rate_ticks++;
	if(now - rate_window_start >= 1000000)
	{
		measured_tick_hz = rate_ticks * 1e6 / (double)(now - rate_window_start);
		rate_ticks = 0;
		rate_window_start = now;
	}

	if(pad == last_pad)
		return;
	last_pad = pad;
	static_assert(CHIAKI_CONTROLLER_BUTTON_CROSS == (uint32_t)rs::PadButton::Cross, "button bits");
	static_assert(CHIAKI_CONTROLLER_BUTTON_PS == (uint32_t)rs::PadButton::PS, "button bits");
	state.buttons = pad.buttons;
	state.l2_state = pad.l2;
	state.r2_state = pad.r2;
	state.left_x = pad.lx;
	state.left_y = pad.ly;
	state.right_x = pad.rx;
	state.right_y = pad.ry;
	emit StateChanged();
}
