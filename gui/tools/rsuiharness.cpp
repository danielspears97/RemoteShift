// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Developer tool: renders RemoteShift QML pages offscreen to PNG for visual review.
// Usage: rsuiharness <qml-file-in-qrc> <page> <out.png> [width height] [scenario]
#include "qmlsvgprovider.h"
#include "rsprofilestore.h"
#include "rsdiagnostics.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTimer>
#include <QtMath>
#include <QTemporaryDir>
#include <QStandardPaths>

// Stand-ins for the parts of the real backend the pages read. Sample data only; never shipped.
class HarnessSettings : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool streamerMode MEMBER streamerMode CONSTANT)
	Q_PROPERTY(int resolutionLocalPS5 MEMBER resolution CONSTANT)
	Q_PROPERTY(int fpsLocalPS5 MEMBER fps CONSTANT)
	Q_PROPERTY(int codecLocalPS5 MEMBER codec CONSTANT)
	Q_PROPERTY(bool showStreamStats MEMBER showStats NOTIFY changed)
	Q_PROPERTY(QString psnAuthToken MEMBER psn CONSTANT)
public:
	using QObject::QObject;
	bool streamerMode = false, showStats = true;
	int resolution = 4, fps = 60, codec = 1;
	QString psn;
signals:
	void changed();
};

class HarnessBackend : public QObject
{
	Q_OBJECT
	Q_PROPERTY(RsProfileStore *remoteShift READ remoteShift CONSTANT)
	Q_PROPERTY(RsDiagnostics *diagnostics MEMBER diag CONSTANT)
	Q_PROPERTY(HarnessSettings *settings MEMBER settings CONSTANT)
	Q_PROPERTY(QObject *session READ session CONSTANT)
	Q_PROPERTY(QVariantList hosts MEMBER hosts NOTIFY hostsChanged)
	Q_PROPERTY(bool discoveryEnabled MEMBER discovery NOTIFY hostsChanged)
public:
	explicit HarnessBackend(QObject *p) : QObject(p), store(new RsProfileStore(this)), diag(new RsDiagnostics(this)), settings(new HarnessSettings(this)) {}
	RsProfileStore *remoteShift() const { return store; }
	QObject *session() const { return nullptr; }
	Q_INVOKABLE void connectToHost(int, QString = QString()) {}
	Q_INVOKABLE void wakeUpHost(int) {}
	Q_INVOKABLE void refreshPsnToken() {}
	RsProfileStore *store;
	RsDiagnostics *diag;
	HarnessSettings *settings;
	QVariantList hosts;
	bool discovery = true;
signals:
	void hostsChanged();
};

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QStandardPaths::setTestModeEnabled(true);
	QGuiApplication app(argc, argv);
	QGuiApplication::setOrganizationName("RemoteShiftHarness");
	QGuiApplication::setApplicationName("RemoteShiftHarness");
	const QStringList args = app.arguments();
	const QString qml = args.value(1, "RsShell.qml");
	const int page = args.value(2, "0").toInt();
	const QString out = args.value(3, "out.png");
	const int w = args.value(4, "1600").toInt(), h = args.value(5, "900").toInt();
	const QString scenario = args.value(6);

	auto *backend = new HarnessBackend(&app);
	qmlRegisterSingletonInstance("org.streetpea.chiaking", 1, 0, "Chiaki", backend);
	qmlRegisterUncreatableType<RsProfileStore>("org.streetpea.chiaking", 1, 0, "RemoteShiftStore", {});
	qmlRegisterUncreatableType<RsDiagnostics>("org.streetpea.chiaking", 1, 0, "RemoteShiftDiagnostics", {});
	qmlRegisterUncreatableType<HarnessSettings>("org.streetpea.chiaking", 1, 0, "HarnessSettings", {});
	if(!scenario.contains("empty"))
	{
		auto host = [](const QString &name, const QString &state, bool registered, bool discovered, const QString &app) {
			return QVariantMap{{"name", name}, {"state", state}, {"registered", registered}, {"discovered", discovered}, {"manual", !discovered},
				{"display", true}, {"ps5", true}, {"mac", name + "-mac"}, {"duid", ""}, {"address", discovered ? "192.168.1.42" : "192.168.1.77"}, {"app", app}};
		};
		backend->hosts = {host("Living Room PS5", "ready", true, true, "Grand Theft Auto VI"), host("Bedroom PS5", "standby", true, false, "")};
	}
	if(scenario.contains("diag"))
	{
		// Synthetic session with a Wi-Fi hiccup in the middle, to review the charts and advice.
		backend->diag->BeginSession(1920, 1080, 60, 15000, "H.265", "d3d11va");
		for(int i = 0; i < 240; i++)
		{
			const bool bad = i > 150 && i < 185;
			for(int f = 0; f < (bad ? 47 : 60); f++)
				backend->diag->NoteFrame();
			RsDiagSample smp;
			smp.bitrate_mbps = 12.5 + 2.5 * qSin(i / 9.0) - (bad ? 5 : 0);
			smp.packet_loss = bad ? 0.031 : (i > 205 ? 0.012 : 0.0);
			smp.frames_lost_total = bad ? (i - 150) * 2 : (i >= 185 ? 70 + (i > 205 ? (i - 205) / 3 : 0) : 0);
			smp.dropped_frames_total = i / 40;
			smp.queue_depth = 1.1 + (bad ? 0.9 : 0);
			smp.frame_age_ms = 9 + 3 * qSin(i / 5.0) + (bad ? 14 : 0);
			smp.rtt_ms = 1.8;
			smp.input_hz = 250;
			smp.input_mode = "On Foot";
			smp.input_profile = "GTA 6";
			backend->diag->AddSample(smp, 1.0);
		}
	}

	QQuickView view;
	view.engine()->addImageProvider("svg", new QmlSvgProvider);
	view.setResizeMode(QQuickView::SizeRootObjectToView);
	view.resize(w, h);
	view.setColor(QColor("#121317"));
	int errors = 0;
	QObject::connect(view.engine(), &QQmlEngine::warnings, [&](const QList<QQmlError> &ws) {
		for(const auto &e : ws) { qWarning("QML: %s", qPrintable(e.toString())); errors++; }
	});
	view.setSource(qml.startsWith("/") ? QUrl::fromLocalFile(qml) : QUrl("qrc:/" + qml));
	if(view.status() != QQuickView::Ready)
	{
		for(const auto &e : view.errors())
			qWarning("QML error: %s", qPrintable(e.toString()));
		return 2;
	}
	view.rootObject()->setProperty("page", page);
	view.show();

	RsProfileStore *s = backend->store;
	if(scenario == "drive")
	{
		s->startPreview();
		s->previewKeyName("W", true);
		s->previewKeyName("D", true);
		s->previewKeyName("MouseRight", true);
		QTimer *t = new QTimer(&app);
		QObject::connect(t, &QTimer::timeout, [s] { s->previewMouseDelta(9, -3); });
		t->start(2);
	}
	else if(scenario == "editor")
	{
		if(QObject *ed = view.rootObject()->findChild<QObject *>("bindingEditorForHarness"))
		{
			QVariantMap b;
			b["index"] = 4; b["key"] = "W"; b["modifiers"] = QStringList{"LAlt"}; b["action"] = "LeftStickUp";
			b["button"] = "Cross"; b["value"] = 0.45; b["mode"] = ""; b["bindMode"] = "hold"; b["turboHz"] = 10;
			QMetaObject::invokeMethod(ed, "openExisting", Q_ARG(QVariant, QVariant(b)));
		}
	}
	else if(scenario == "select")
	{
		if(QObject *d = view.rootObject()->findChild<QObject *>("diagramForHarness"))
			d->setProperty("selected", "R2");
	}
	QTimer::singleShot(900, [&] {
		QImage img = view.grabWindow();
		img.save(out);
		qInfo("saved %s (%dx%d), %d QML warnings", qPrintable(out), img.width(), img.height(), errors);
		app.exit(errors ? 3 : 0);
	});
	return app.exec();
}

#include "rsuiharness.moc"
