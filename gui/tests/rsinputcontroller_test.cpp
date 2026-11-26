// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Integration test: Qt window events -> RsInputController -> ChiakiControllerState.
#include "rsinputcontroller.h"

#include <rsinput/profile.h>

#include <QEventLoop>
#include <QFile>
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>
#include <QWindow>

class RsInputControllerTest : public QObject
{
	Q_OBJECT

	QWindow *window = nullptr;
	rs::Profile gta;

	// Native scan codes as the platform delivers them.
	static quint32 Sc(rs::Key k)
	{
#ifdef _WIN32
		switch(k) { case rs::Key::W: return 0x11; case rs::Key::D: return 0x20; case rs::Key::F: return 0x21; case rs::Key::Num1: return 0x02;
			case rs::Key::F8: return 0x42; case rs::Key::LShift: return 0x2A; case rs::Key::RShift: return 0x36; case rs::Key::LAlt: return 0x38; default: return 0; }
#else
		switch(k) { case rs::Key::W: return 25; case rs::Key::D: return 40; case rs::Key::F: return 41; case rs::Key::Num1: return 10;
			case rs::Key::F8: return 74; case rs::Key::LShift: return 50; case rs::Key::RShift: return 62; case rs::Key::LAlt: return 64; default: return 0; }
#endif
	}
	static bool Key(RsInputController &c, int qt_key, rs::Key k, bool press, Qt::KeyboardModifiers mods = Qt::NoModifier)
	{
		QKeyEvent ev(press ? QEvent::KeyPress : QEvent::KeyRelease, qt_key, mods, Sc(k), 0, 0);
		return c.HandleKey(&ev);
	}
	bool Click(RsInputController &c, Qt::MouseButton b, bool press)
	{
		QPointF p(window->width() / 2.0, window->height() / 2.0);
		QMouseEvent ev(press ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease, p, window->mapToGlobal(p),
			b, press ? b : Qt::NoButton, Qt::NoModifier);
		return c.HandleMouseButton(&ev);
	}

private slots:
	void initTestCase()
	{
		QFile f(QStringLiteral(RSINPUT_PRESET_DIR "/gta6.json"));
		QVERIFY(f.open(QIODevice::ReadOnly));
		gta = rs::ProfileFromJson(f.readAll().toStdString());
		window = new QWindow;
		window->resize(800, 450);
		window->show();
		window->requestActivate();
		QVERIFY(QTest::qWaitForWindowActive(window));
	}
	void cleanupTestCase() { delete window; }

	void scanCodesArePositional()
	{
		// Shift+1 produces Qt::Key_Exclam but must still be the physical "1" key.
		QCOMPARE(RsKeyFromQt(Qt::Key_Exclam, Sc(rs::Key::Num1), Qt::ShiftModifier), rs::Key::Num1);
		QCOMPARE(RsKeyFromQt(Qt::Key_Shift, Sc(rs::Key::RShift), Qt::ShiftModifier), rs::Key::RShift);
		QCOMPARE(RsKeyFromQt(Qt::Key_Shift, Sc(rs::Key::LShift), Qt::ShiftModifier), rs::Key::LShift);
		// Unknown scan code falls back to the Qt key.
		QCOMPARE(RsKeyFromQt(Qt::Key_F, 0, Qt::NoModifier), rs::Key::F);
		QCOMPARE(RsKeyFromMouseButton(Qt::BackButton), rs::Key::MouseX1);
	}

	void keysIgnoredUntilCaptured()
	{
		RsInputController c;
		c.AttachWindow(window);
		c.SetProfile(gta);
		QVERIFY(!Key(c, Qt::Key_W, rs::Key::W, true));
		QCOMPARE(c.State().left_y, (int16_t)0);
		QVERIFY(Click(c, Qt::LeftButton, true)); // click captures
		QVERIFY(c.Captured());
		Click(c, Qt::LeftButton, false);
	}

	void movementButtonsAndModes()
	{
		RsInputController c;
		c.AttachWindow(window);
		c.SetProfile(gta);
		QSignalSpy states(&c, &RsInputController::StateChanged);
		QSignalSpy modes(&c, &RsInputController::ModeChanged);
		c.SetCaptured(true);
		QVERIFY(c.Captured());
		QVERIFY(Key(c, Qt::Key_W, rs::Key::W, true));
		QCOMPARE(c.State().left_y, (int16_t)-32767); // sent immediately, not on the next timer tick
		QVERIFY(states.count() >= 1);
		QVERIFY(Key(c, Qt::Key_D, rs::Key::D, true));
		QVERIFY(c.State().left_x > 23000);
		// Auto-repeat must not re-trigger anything.
		QKeyEvent rep(QEvent::KeyPress, Qt::Key_W, Qt::NoModifier, Sc(rs::Key::W), 0, 0, QString(), true);
		QVERIFY(c.HandleKey(&rep));
		Key(c, Qt::Key_W, rs::Key::W, false);
		Key(c, Qt::Key_D, rs::Key::D, false);
		QCOMPARE(c.State().left_x, (int16_t)0);
		// Right mouse = L2 (aim), left mouse = R2 (fire)
		QVERIFY(Click(c, Qt::RightButton, true));
		QCOMPARE(c.State().l2_state, (uint8_t)255);
		Click(c, Qt::RightButton, false);
		QCOMPARE(c.State().l2_state, (uint8_t)0);
		// F: Triangle + switch to Driving
		Key(c, Qt::Key_F, rs::Key::F, true);
		QCOMPARE(c.ActiveMode(), QStringLiteral("Driving"));
		QVERIFY(c.State().buttons & CHIAKI_CONTROLLER_BUTTON_PYRAMID);
		Key(c, Qt::Key_F, rs::Key::F, false);
		QVERIFY(modes.count() >= 1);
		// In Driving, W is throttle (with a 60 ms ramp)
		Key(c, Qt::Key_W, rs::Key::W, true);
		QTRY_COMPARE_WITH_TIMEOUT(c.State().r2_state, (uint8_t)255, 1000);
		Key(c, Qt::Key_W, rs::Key::W, false);
		QVERIFY(c.SetMode("On Foot"));
	}

	void wheelPulses()
	{
		RsInputController c;
		c.AttachWindow(window);
		c.SetProfile(gta);
		c.SetCaptured(true);
		QWheelEvent ev(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
		QVERIFY(c.HandleWheel(&ev));
		// WheelDown -> DpadRight pulse in On Foot
		QTRY_VERIFY_WITH_TIMEOUT(c.State().buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT, 500);
		QTRY_VERIFY_WITH_TIMEOUT(!(c.State().buttons & CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT), 500);
	}

	void mouseMotionMovesRightStick()
	{
		RsInputController c;
		c.AttachWindow(window);
		c.SetProfile(gta);
		c.SetCaptured(true);
		if(c.UsingRawInput())
			QSKIP("Raw Input active: motion comes from WM_INPUT, not window events");
		const QPointF centre(window->width() / 2.0, window->height() / 2.0);
		bool moved = false;
		for(int i = 0; i < 40 && !moved; i++)
		{
			QPointF p = centre + QPointF(6, 0);
			QMouseEvent ev(QEvent::MouseMove, p, window->mapToGlobal(p), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
			QVERIFY(c.HandleMouseMove(&ev));
			QTest::qWait(2);
			moved = c.State().right_x > 3000;
		}
		QVERIFY(moved);
		QTRY_COMPARE_WITH_TIMEOUT(c.State().right_x, (int16_t)0, 500); // recentres when motion stops
	}

	void captureKeyReleasesAndClearsState()
	{
		RsInputController c;
		c.AttachWindow(window);
		c.SetProfile(gta);
		c.SetCaptured(true);
		Key(c, Qt::Key_W, rs::Key::W, true);
		QCOMPARE(c.State().left_y, (int16_t)-32767);
		QVERIFY(Key(c, Qt::Key_F8, rs::Key::F8, true));
		QVERIFY(!c.Captured());
		QCOMPARE(c.State().left_y, (int16_t)0); // nothing stays held after release
		Key(c, Qt::Key_F8, rs::Key::F8, false);
		QVERIFY(Key(c, Qt::Key_F8, rs::Key::F8, true));
		QVERIFY(c.Captured());
	}

	void tickRateIsHonoured()
	{
		RsInputController c;
		c.SetTickRate(500);
		QCOMPARE(c.TickRate(), 500);
		c.SetTickRate(5000);
		QCOMPARE(c.TickRate(), 1000);
		c.SetTickRate(250);
		// A real event loop (QTest::qWait sleeps 10 ms between event batches, capping timers at ~100 Hz).
		QEventLoop loop;
		QTimer::singleShot(2300, &loop, &QEventLoop::quit);
		loop.exec();
		QVERIFY2(c.MeasuredTickHz() > 150 && c.MeasuredTickHz() < 300, qPrintable(QString::number(c.MeasuredTickHz())));
	}
};

QTEST_MAIN(RsInputControllerTest)
#include "rsinputcontroller_test.moc"
