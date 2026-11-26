// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// RemoteShift: drives the keyboard & mouse engine for a stream session.
#pragma once

#include <chiaki/controller.h>

#include <rsinput/engine.h>

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QWindow>

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class RsRawMouse;

// Physical key from a Qt key event. Uses the native scan code so bindings stay on the
// same physical keys regardless of keyboard layout and Shift state (Shift+1 is still "1").
rs::Key RsKeyFromQt(int qt_key, quint32 native_scan_code, Qt::KeyboardModifiers mods);
rs::Key RsKeyFromMouseButton(Qt::MouseButton b);

class RsInputController : public QObject
{
	Q_OBJECT

public:
	explicit RsInputController(QObject *parent = nullptr);
	~RsInputController() override;

	void SetProfile(const rs::Profile &profile);
	void SetCaptureKey(rs::Key key) { capture_key = key; }
	void SetTickRate(int hz);
	int TickRate() const { return tick_hz; }
	void AttachWindow(QWindow *window);

	// Event entry points from the stream window. Each returns true when consumed.
	bool HandleKey(QKeyEvent *event);
	bool HandleMouseButton(QMouseEvent *event);
	bool HandleMouseMove(QMouseEvent *event);
	bool HandleWheel(QWheelEvent *event);
	void HandleRawDelta(int dx, int dy);

	void ReleaseAll();
	bool Captured() const { return captured; }
	void SetCaptured(bool capture);
	bool UsingRawInput() const;

	ChiakiControllerState State() const { return state; }
	const rs::Engine &Engine() const { return engine; }
	bool SetMode(const QString &mode);
	QString ActiveMode() const { return QString::fromStdString(engine.ActiveMode()); }
	double MeasuredTickHz() const { return measured_tick_hz; }

signals:
	void StateChanged();
	void CaptureChanged(bool captured);
	void ModeChanged(const QString &mode);

private:
	void Tick();
	uint64_t NowUs() const { return (uint64_t)(clock.nsecsElapsed() / 1000); }
	void ApplyClip();

	rs::Engine engine;
	ChiakiControllerState state;
	rs::PadState last_pad;
	QTimer tick_timer;
	QElapsedTimer clock;
	int tick_hz = 250;
	rs::Key capture_key = rs::Key::F8;
	bool captured = false;
	QPointer<QWindow> window;
	RsRawMouse *raw = nullptr;
	int wheel_remainder_v = 0, wheel_remainder_h = 0;
	bool ignore_next_warp = false;
	// Tick-rate measurement for diagnostics.
	uint64_t rate_window_start = 0;
	int rate_ticks = 0;
	double measured_tick_hz = 0.0;
};
