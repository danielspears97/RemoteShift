// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// RemoteShift: input profiles on disk, editing API for QML, and live input telemetry.
#pragma once

#include <rsinput/engine.h>
#include <rsinput/profile.h>

#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <QVariant>

class RsInputController;

class RsProfileStore : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool enabled READ Enabled WRITE SetEnabled NOTIFY settingsChanged)
	Q_PROPERTY(QString captureKey READ CaptureKey WRITE SetCaptureKey NOTIFY settingsChanged)
	Q_PROPERTY(int tickRate READ TickRate WRITE SetTickRate NOTIFY settingsChanged)
	Q_PROPERTY(bool overlayEnabled READ OverlayEnabled WRITE SetOverlayEnabled NOTIFY settingsChanged)
	Q_PROPERTY(QStringList profiles READ Profiles NOTIFY profilesChanged)
	Q_PROPERTY(QString activeProfile READ ActiveProfileName WRITE SetActiveProfile NOTIFY activeProfileChanged)
	Q_PROPERTY(QStringList recentProfiles READ RecentProfiles NOTIFY activeProfileChanged)
	Q_PROPERTY(int revision READ Revision NOTIFY profileEdited)
	Q_PROPERTY(QVariantMap live READ Live NOTIFY liveChanged)
	Q_PROPERTY(bool sessionActive READ SessionActive NOTIFY sessionActiveChanged)
	Q_PROPERTY(bool previewActive READ PreviewActive NOTIFY previewActiveChanged)
	Q_PROPERTY(QStringList keyNames READ KeyNames CONSTANT)
	Q_PROPERTY(QStringList buttonNames READ ButtonNames CONSTANT)
	Q_PROPERTY(QStringList actionTypes READ ActionTypes CONSTANT)
	Q_PROPERTY(QString profileDirectory READ ProfileDirectory CONSTANT)

public:
	explicit RsProfileStore(QObject *parent = nullptr);
	~RsProfileStore() override;
	static RsProfileStore *Instance();

	bool Enabled() const;
	void SetEnabled(bool e);
	QString CaptureKey() const;
	void SetCaptureKey(const QString &k);
	int TickRate() const;
	void SetTickRate(int hz);
	bool OverlayEnabled() const;
	void SetOverlayEnabled(bool e);

	QStringList Profiles() const { return profiles.keys(); }
	QString ActiveProfileName() const { return active; }
	void SetActiveProfile(const QString &name);
	QStringList RecentProfiles() const;
	rs::Profile ActiveProfile() const;
	int Revision() const { return revision; }
	QString ProfileDirectory() const { return dir; }

	QVariantMap Live() const { return live; }
	bool SessionActive() const { return !controller.isNull(); }
	bool PreviewActive() const { return preview_active; }

	QStringList KeyNames() const;
	QStringList ButtonNames() const;
	QStringList ActionTypes() const;

	// Session wiring
	void AttachController(RsInputController *c);
	void DetachController(RsInputController *c);

	// ---- profile management (QML)
	Q_INVOKABLE QString createProfile(const QString &name, const QString &copy_from);
	Q_INVOKABLE QString renameProfile(const QString &from, const QString &to);
	Q_INVOKABLE QString deleteProfile(const QString &name);
	Q_INVOKABLE QString importProfile(const QUrl &file);
	Q_INVOKABLE QString exportProfile(const QString &name, const QUrl &file);
	Q_INVOKABLE QString restoreBundledProfiles();
	Q_INVOKABLE QVariantMap profileInfo(const QString &name) const;
	Q_INVOKABLE void setProfileInfo(const QString &field, const QString &value);
	Q_INVOKABLE QStringList validate() const;

	// ---- modes (active profile)
	Q_INVOKABLE QStringList modes() const;
	Q_INVOKABLE QString defaultMode() const;
	Q_INVOKABLE void setDefaultMode(const QString &mode);
	Q_INVOKABLE QString addMode(const QString &name, const QString &copy_from);
	Q_INVOKABLE QString renameMode(const QString &from, const QString &to);
	Q_INVOKABLE QString removeMode(const QString &mode);
	Q_INVOKABLE bool switchSessionMode(const QString &mode);

	// ---- bindings (active profile)
	Q_INVOKABLE QVariantList bindings(const QString &mode) const;
	Q_INVOKABLE QString setBinding(const QString &mode, int index, const QVariantMap &binding);
	Q_INVOKABLE QString addBinding(const QString &mode, const QVariantMap &binding);
	Q_INVOKABLE void removeBinding(const QString &mode, int index);
	// Output id ("Cross", "L2", "LeftStickUp", ...) -> list of chord strings, for the controller diagram.
	Q_INVOKABLE QVariantMap assignments(const QString &mode) const;
	Q_INVOKABLE QString keyFromEvent(int qt_key, int native_scan_code, int modifiers) const;
	Q_INVOKABLE QString keyFromMouseButton(int qt_button) const;
	Q_INVOKABLE QString describeAction(const QVariantMap &binding) const;

	// ---- tuning (active profile)
	Q_INVOKABLE QVariantMap mouseConfig(const QString &mode) const;
	Q_INVOKABLE void setMouseValue(const QString &mode, const QString &field, const QVariant &value);
	Q_INVOKABLE QVariantMap keyStickConfig(const QString &mode, const QString &which) const;
	Q_INVOKABLE void setKeyStickValue(const QString &mode, const QString &which, const QString &field, const QVariant &value);
	Q_INVOKABLE double triggerRamp(const QString &mode) const;
	Q_INVOKABLE void setTriggerRamp(const QString &mode, double ms);
	Q_INVOKABLE QVariantList curvePreview(const QString &mode, int steps) const;

	// ---- offline preview: test a profile without a console
	Q_INVOKABLE void startPreview();
	Q_INVOKABLE void stopPreview();
	Q_INVOKABLE void previewKey(int qt_key, int native_scan_code, int modifiers, bool pressed);
	Q_INVOKABLE void previewKeyName(const QString &key, bool pressed);
	Q_INVOKABLE void previewMouseDelta(double dx, double dy);
	Q_INVOKABLE void previewWheel(int angle_delta_y);

signals:
	void settingsChanged();
	void profilesChanged();
	void activeProfileChanged();
	void profileEdited();
	void liveChanged();
	void sessionActiveChanged();
	void previewActiveChanged();

private:
	void LoadAll();
	void SeedBundled(bool overwrite);
	QString SaveProfile(const rs::Profile &p);
	QString PathFor(const QString &name) const;
	rs::Profile *Active();
	const rs::Profile *Active() const;
	rs::Mode *FindMode(const QString &mode);
	const rs::Mode *FindMode(const QString &mode) const;
	void Edited();
	void UpdateLive();
	static bool BindingFromMap(const QVariantMap &m, rs::Binding *out, QString *err);
	static QVariantMap BindingToMap(const rs::Binding &b, int index);

	QSettings settings;
	QString dir;
	QMap<QString, rs::Profile> profiles;
	QString active;
	int revision = 0;

	QPointer<RsInputController> controller;
	QTimer live_timer;
	QVariantMap live;

	bool preview_active = false;
	rs::Engine preview;
	QTimer preview_timer;
	QElapsedTimer preview_clock;
	rs::PadState preview_pad;
	double preview_wheel_rem = 0;
	double preview_dx_rem = 0, preview_dy_rem = 0;
};
