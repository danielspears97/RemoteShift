// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// Developer tool: renders RemoteShift QML pages offscreen to PNG for visual review.
// Usage: rsuiharness <qml-file-in-qrc> <page> <out.png> [width height] [scenario]
#include "qmlsvgprovider.h"
#include "rsprofilestore.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTimer>
#include <QTemporaryDir>
#include <QStandardPaths>

class HarnessBackend : public QObject
{
	Q_OBJECT
	Q_PROPERTY(RsProfileStore *remoteShift READ remoteShift CONSTANT)
	Q_PROPERTY(QObject *session READ session CONSTANT)
public:
	explicit HarnessBackend(QObject *p) : QObject(p), store(new RsProfileStore(this)) {}
	RsProfileStore *remoteShift() const { return store; }
	QObject *session() const { return nullptr; }
	RsProfileStore *store;
};

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QStandardPaths::setTestModeEnabled(true);
	QGuiApplication app(argc, argv);
	QGuiApplication::setOrganizationName("RemoteShiftHarness");
	QGuiApplication::setApplicationName("RemoteShiftHarness");
	const QStringList args = app.arguments();
	const QString qml = args.value(1, "RsInputView.qml");
	const int page = args.value(2, "0").toInt();
	const QString out = args.value(3, "out.png");
	const int w = args.value(4, "1600").toInt(), h = args.value(5, "900").toInt();
	const QString scenario = args.value(6);

	auto *backend = new HarnessBackend(&app);
	qmlRegisterSingletonInstance("org.streetpea.chiaking", 1, 0, "Chiaki", backend);
	qmlRegisterUncreatableType<RsProfileStore>("org.streetpea.chiaking", 1, 0, "RemoteShiftStore", {});

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
