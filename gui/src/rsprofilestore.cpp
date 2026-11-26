// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "rsprofilestore.h"
#include "rsinputcontroller.h"
#include "rsportable.h"

#include <rsinput/mousestick.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <cmath>

static RsProfileStore *g_instance = nullptr;
static const char *const kBundled[] = {"gta6.json", "default.json"};

RsProfileStore *RsProfileStore::Instance() { return g_instance; }

RsProfileStore::RsProfileStore(QObject *parent) : QObject(parent)
{
	g_instance = this;
	dir = RsDataDir() + "/profiles";
	QDir().mkpath(dir);
	settings.beginGroup("remoteshift");
	LoadAll();
	if(profiles.isEmpty())
	{
		SeedBundled(false);
		LoadAll();
	}
	active = settings.value("active_profile").toString();
	if(!profiles.contains(active))
		active = profiles.contains("GTA 6") ? "GTA 6" : (profiles.isEmpty() ? QString() : profiles.firstKey());

	live_timer.setInterval(33);
	connect(&live_timer, &QTimer::timeout, this, &RsProfileStore::UpdateLive);

	if(UpdateCheckAvailable() && UpdateCheckEnabled())
		QTimer::singleShot(4000, this, &RsProfileStore::checkForUpdates);

	preview_clock.start();
	preview_timer.setTimerType(Qt::PreciseTimer);
	preview_timer.setInterval(4);
	connect(&preview_timer, &QTimer::timeout, this, [this] {
		preview_pad = preview.Tick((uint64_t)(preview_clock.nsecsElapsed() / 1000));
		preview.TakeEvents();
	});
}

RsProfileStore::~RsProfileStore()
{
	if(g_instance == this)
		g_instance = nullptr;
}

// ---------------------------------------------------------------- settings

bool RsProfileStore::Enabled() const { return settings.value("enabled", true).toBool(); }
void RsProfileStore::SetEnabled(bool e) { settings.setValue("enabled", e); emit settingsChanged(); }
QString RsProfileStore::CaptureKey() const { return settings.value("capture_key", "F8").toString(); }
void RsProfileStore::SetCaptureKey(const QString &k)
{
	if(rs::KeyFromName(k.toStdString()) == rs::Key::None)
		return;
	settings.setValue("capture_key", k);
	if(controller)
		controller->SetCaptureKey(rs::KeyFromName(k.toStdString()));
	emit settingsChanged();
}
int RsProfileStore::TickRate() const { return settings.value("tick_rate", 250).toInt(); }
void RsProfileStore::SetTickRate(int hz)
{
	settings.setValue("tick_rate", qBound(60, hz, 1000));
	if(controller)
		controller->SetTickRate(TickRate());
	emit settingsChanged();
}
bool RsProfileStore::OverlayEnabled() const { return settings.value("overlay", true).toBool(); }
void RsProfileStore::SetOverlayEnabled(bool e) { settings.setValue("overlay", e); emit settingsChanged(); }

QStringList RsProfileStore::KeyNames() const
{
	QStringList out;
	for(int i = 1; i < rs::kKeyCount; i++)
		out << rs::KeyName(static_cast<rs::Key>(i));
	return out;
}

QStringList RsProfileStore::ButtonNames() const
{
	return {"Cross", "Circle", "Square", "Triangle", "DpadUp", "DpadDown", "DpadLeft", "DpadRight",
		"L1", "R1", "L3", "R3", "Options", "Create", "Touchpad", "PS"};
}

QStringList RsProfileStore::ActionTypes() const
{
	return {"Button", "L2", "R2", "LeftStickUp", "LeftStickDown", "LeftStickLeft", "LeftStickRight",
		"RightStickUp", "RightStickDown", "RightStickLeft", "RightStickRight",
		"SetMode", "HoldMode", "CycleMode", "AdsHold", "SensitivityScale", "ToggleCapture"};
}

// ---------------------------------------------------------------- updates

#ifndef REMOTESHIFT_UPDATE_REPO
#define REMOTESHIFT_UPDATE_REPO ""
#endif

QString RsProfileStore::AppVersion() const { return QCoreApplication::applicationVersion(); }
bool RsProfileStore::UpdateCheckAvailable() const { return QString(REMOTESHIFT_UPDATE_REPO).contains('/'); }
bool RsProfileStore::UpdateCheckEnabled() const { return settings.value("check_updates", true).toBool(); }
void RsProfileStore::SetUpdateCheckEnabled(bool e) { settings.setValue("check_updates", e); emit settingsChanged(); }

int RsProfileStore::CompareVersions(const QString &a_in, const QString &b_in)
{
	auto parts = [](QString v) {
		v = v.trimmed();
		if(v.startsWith('v') || v.startsWith('V'))
			v.remove(0, 1);
		v = v.section('-', 0, 0); // ignore pre-release suffix for ordering
		QList<int> out;
		for(const QString &p : v.split('.'))
			out << p.toInt();
		while(out.size() < 3)
			out << 0;
		return out;
	};
	const QList<int> a = parts(a_in), b = parts(b_in);
	for(int i = 0; i < qMax(a.size(), b.size()); i++)
	{
		int x = a.value(i), y = b.value(i);
		if(x != y)
			return x < y ? -1 : 1;
	}
	return 0;
}

void RsProfileStore::checkForUpdates()
{
	if(!UpdateCheckAvailable() || update_in_flight)
		return;
	update_in_flight = true;
	update_status = tr("Checking for updates…");
	emit updateChanged();
	// Only the latest release's public metadata is requested; nothing about this install is sent.
	auto *nam = new QNetworkAccessManager(this);
	QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(REMOTESHIFT_UPDATE_REPO)));
	req.setRawHeader("Accept", "application/vnd.github+json");
	req.setRawHeader("User-Agent", "RemoteShift");
	req.setTransferTimeout(10000);
	QNetworkReply *reply = nam->get(req);
	connect(reply, &QNetworkReply::finished, this, [this, reply, nam] {
		update_in_flight = false;
		reply->deleteLater();
		nam->deleteLater();
		if(reply->error() != QNetworkReply::NoError)
		{
			update_status = tr("Update check failed: %1").arg(reply->errorString());
			emit updateChanged();
			return;
		}
		const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
		const QString tag = o.value("tag_name").toString();
		const QString url = o.value("html_url").toString();
		if(tag.isEmpty())
		{
			update_status = tr("No releases published yet.");
		}
		else if(CompareVersions(tag, AppVersion()) > 0)
		{
			update_version = tag;
			update_url = url;
			update_status = tr("RemoteShift %1 is available.").arg(tag);
		}
		else
		{
			update_version.clear();
			update_url.clear();
			update_status = tr("You're on the latest version (%1).").arg(AppVersion());
		}
		emit updateChanged();
	});
}

// ---------------------------------------------------------------- storage

QString RsProfileStore::PathFor(const QString &name) const
{
	QString safe = name;
	safe.replace(QRegularExpression("[^A-Za-z0-9 _\\-\\.]"), "_");
	return dir + "/" + safe.trimmed() + ".json";
}

void RsProfileStore::LoadAll()
{
	profiles.clear();
	const auto files = QDir(dir).entryInfoList({"*.json"}, QDir::Files, QDir::Name);
	for(const auto &fi : files)
	{
		QFile f(fi.absoluteFilePath());
		if(!f.open(QIODevice::ReadOnly))
			continue;
		try
		{
			rs::Profile p = rs::ProfileFromJson(f.readAll().toStdString());
			QString name = QString::fromStdString(p.name);
			if(name.isEmpty() || profiles.contains(name))
				continue;
			profiles.insert(name, p);
		}
		catch(const std::exception &ex)
		{
			qWarning("RemoteShift: skipping profile %s: %s", qPrintable(fi.fileName()), ex.what());
		}
	}
	emit profilesChanged();
}

void RsProfileStore::SeedBundled(bool overwrite)
{
	for(const char *name : kBundled)
	{
		QFile res(QString(":/remoteshift/presets/") + name);
		if(!res.open(QIODevice::ReadOnly))
			continue;
		const QByteArray data = res.readAll();
		try
		{
			rs::Profile p = rs::ProfileFromJson(data.toStdString());
			QString path = PathFor(QString::fromStdString(p.name));
			if(!overwrite && QFile::exists(path))
				continue;
			QFile out(path);
			if(out.open(QIODevice::WriteOnly | QIODevice::Truncate))
				out.write(data);
		}
		catch(const std::exception &ex)
		{
			qWarning("RemoteShift: bundled profile %s invalid: %s", name, ex.what());
		}
	}
}

QString RsProfileStore::SaveProfile(const rs::Profile &p)
{
	QFile f(PathFor(QString::fromStdString(p.name)));
	if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return tr("Could not write %1: %2").arg(f.fileName(), f.errorString());
	f.write(QByteArray::fromStdString(rs::ProfileToJson(p)));
	return {};
}

rs::Profile *RsProfileStore::Active()
{
	auto it = profiles.find(active);
	return it == profiles.end() ? nullptr : &it.value();
}

const rs::Profile *RsProfileStore::Active() const
{
	auto it = profiles.constFind(active);
	return it == profiles.constEnd() ? nullptr : &it.value();
}

rs::Profile RsProfileStore::ActiveProfile() const
{
	const rs::Profile *p = Active();
	return p ? *p : rs::Profile();
}

void RsProfileStore::SetActiveProfile(const QString &name)
{
	if(!profiles.contains(name) || name == active)
		return;
	active = name;
	settings.setValue("active_profile", name);
	QStringList recent = settings.value("recent_profiles").toStringList();
	recent.removeAll(name);
	recent.prepend(name);
	while(recent.size() > 5)
		recent.removeLast();
	settings.setValue("recent_profiles", recent);
	if(controller)
		controller->SetProfile(ActiveProfile());
	preview.SetProfile(ActiveProfile());
	revision++;
	emit activeProfileChanged();
	emit profileEdited();
}

QStringList RsProfileStore::RecentProfiles() const
{
	QStringList out;
	for(const QString &n : settings.value("recent_profiles").toStringList())
		if(profiles.contains(n))
			out << n;
	if(out.isEmpty() && !active.isEmpty())
		out << active;
	return out;
}

void RsProfileStore::Edited()
{
	if(rs::Profile *p = Active())
	{
		QString err = SaveProfile(*p);
		if(!err.isEmpty())
			qWarning("RemoteShift: %s", qPrintable(err));
		if(controller)
			controller->SetProfile(*p);
		preview.SetProfile(*p);
	}
	revision++;
	emit profileEdited();
}

// ---------------------------------------------------------------- profile management

QString RsProfileStore::createProfile(const QString &name_in, const QString &copy_from)
{
	const QString name = name_in.trimmed();
	if(name.isEmpty())
		return tr("Enter a profile name.");
	if(profiles.contains(name))
		return tr("A profile named \"%1\" already exists.").arg(name);
	rs::Profile p;
	if(profiles.contains(copy_from))
		p = profiles.value(copy_from);
	else
	{
		rs::Mode m;
		m.name = "Gameplay";
		p.modes.push_back(m);
		p.default_mode = "Gameplay";
	}
	p.name = name.toStdString();
	QString err = SaveProfile(p);
	if(!err.isEmpty())
		return err;
	profiles.insert(name, p);
	emit profilesChanged();
	SetActiveProfile(name);
	return {};
}

QString RsProfileStore::renameProfile(const QString &from, const QString &to_in)
{
	const QString to = to_in.trimmed();
	if(!profiles.contains(from))
		return tr("Profile not found.");
	if(to.isEmpty())
		return tr("Enter a profile name.");
	if(to == from)
		return {};
	if(profiles.contains(to))
		return tr("A profile named \"%1\" already exists.").arg(to);
	rs::Profile p = profiles.take(from);
	p.name = to.toStdString();
	QString err = SaveProfile(p);
	if(!err.isEmpty())
	{
		p.name = from.toStdString();
		profiles.insert(from, p);
		return err;
	}
	QFile::remove(PathFor(from));
	profiles.insert(to, p);
	if(active == from)
	{
		active = to;
		settings.setValue("active_profile", to);
	}
	emit profilesChanged();
	emit activeProfileChanged();
	return {};
}

QString RsProfileStore::deleteProfile(const QString &name)
{
	if(!profiles.contains(name))
		return tr("Profile not found.");
	if(profiles.size() == 1)
		return tr("Keep at least one profile.");
	if(!QFile::remove(PathFor(name)) && QFile::exists(PathFor(name)))
		return tr("Could not delete the profile file.");
	profiles.remove(name);
	emit profilesChanged();
	if(active == name)
	{
		active.clear();
		SetActiveProfile(profiles.firstKey());
	}
	return {};
}

QString RsProfileStore::importProfile(const QUrl &url)
{
	QFile f(url.isLocalFile() ? url.toLocalFile() : url.toString());
	if(!f.open(QIODevice::ReadOnly))
		return tr("Could not open file: %1").arg(f.errorString());
	if(f.size() > 4 * 1024 * 1024)
		return tr("File is too large to be a profile.");
	rs::Profile p;
	try { p = rs::ProfileFromJson(f.readAll().toStdString()); }
	catch(const std::exception &ex) { return tr("Not a valid profile: %1").arg(QString::fromUtf8(ex.what())); }
	auto problems = rs::ValidateProfile(p);
	if(!problems.empty())
		return tr("Profile has problems: %1").arg(QString::fromStdString(problems.front()));
	QString base = QString::fromStdString(p.name).trimmed();
	if(base.isEmpty())
		base = tr("Imported");
	QString name = base;
	for(int i = 2; profiles.contains(name); i++)
		name = QString("%1 (%2)").arg(base).arg(i);
	p.name = name.toStdString();
	QString err = SaveProfile(p);
	if(!err.isEmpty())
		return err;
	profiles.insert(name, p);
	emit profilesChanged();
	SetActiveProfile(name);
	return {};
}

QString RsProfileStore::exportProfile(const QString &name, const QUrl &url)
{
	if(!profiles.contains(name))
		return tr("Profile not found.");
	QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
	if(!path.endsWith(".json", Qt::CaseInsensitive))
		path += ".json";
	QFile f(path);
	if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return tr("Could not write file: %1").arg(f.errorString());
	f.write(QByteArray::fromStdString(rs::ProfileToJson(profiles.value(name))));
	return {};
}

QString RsProfileStore::restoreBundledProfiles()
{
	SeedBundled(true);
	QString keep = active;
	LoadAll();
	active.clear();
	SetActiveProfile(profiles.contains(keep) ? keep : profiles.firstKey());
	revision++;
	emit profileEdited();
	return {};
}

QVariantMap RsProfileStore::profileInfo(const QString &name) const
{
	QVariantMap m;
	auto it = profiles.constFind(name);
	if(it == profiles.constEnd())
		return m;
	m["name"] = name;
	m["game"] = QString::fromStdString(it->game);
	m["description"] = QString::fromStdString(it->description);
	m["modes"] = (int)it->modes.size();
	int binds = 0;
	for(const auto &md : it->modes)
		binds += (int)md.bindings.size();
	m["bindings"] = binds;
	return m;
}

void RsProfileStore::setProfileInfo(const QString &field, const QString &value)
{
	rs::Profile *p = Active();
	if(!p)
		return;
	if(field == "game")
		p->game = value.toStdString();
	else if(field == "description")
		p->description = value.toStdString();
	else
		return;
	Edited();
}

QStringList RsProfileStore::validate() const
{
	QStringList out;
	if(const rs::Profile *p = Active())
		for(const auto &s : rs::ValidateProfile(*p))
			out << QString::fromStdString(s);
	return out;
}

// ---------------------------------------------------------------- modes

rs::Mode *RsProfileStore::FindMode(const QString &mode)
{
	rs::Profile *p = Active();
	if(!p)
		return nullptr;
	for(auto &m : p->modes)
		if(m.name == mode.toStdString())
			return &m;
	return nullptr;
}

const rs::Mode *RsProfileStore::FindMode(const QString &mode) const
{
	const rs::Profile *p = Active();
	if(!p)
		return nullptr;
	for(const auto &m : p->modes)
		if(m.name == mode.toStdString())
			return &m;
	return nullptr;
}

QStringList RsProfileStore::modes() const
{
	QStringList out;
	if(const rs::Profile *p = Active())
		for(const auto &m : p->modes)
			out << QString::fromStdString(m.name);
	return out;
}

QString RsProfileStore::defaultMode() const
{
	const rs::Profile *p = Active();
	return p ? QString::fromStdString(p->default_mode) : QString();
}

void RsProfileStore::setDefaultMode(const QString &mode)
{
	rs::Profile *p = Active();
	if(!p || !FindMode(mode))
		return;
	p->default_mode = mode.toStdString();
	Edited();
}

QString RsProfileStore::addMode(const QString &name_in, const QString &copy_from)
{
	const QString name = name_in.trimmed();
	rs::Profile *p = Active();
	if(!p)
		return tr("No active profile.");
	if(name.isEmpty())
		return tr("Enter a mode name.");
	if(FindMode(name))
		return tr("A mode named \"%1\" already exists.").arg(name);
	rs::Mode m;
	if(const rs::Mode *src = FindMode(copy_from))
		m = *src;
	m.name = name.toStdString();
	p->modes.push_back(m);
	Edited();
	return {};
}

QString RsProfileStore::renameMode(const QString &from, const QString &to_in)
{
	const QString to = to_in.trimmed();
	rs::Profile *p = Active();
	rs::Mode *m = FindMode(from);
	if(!p || !m)
		return tr("Mode not found.");
	if(to.isEmpty())
		return tr("Enter a mode name.");
	if(to == from)
		return {};
	if(FindMode(to))
		return tr("A mode named \"%1\" already exists.").arg(to);
	m->name = to.toStdString();
	// Keep mode-switch bindings pointing at the renamed mode.
	for(auto &md : p->modes)
		for(auto &b : md.bindings)
			if(b.action.mode == from.toStdString())
				b.action.mode = to.toStdString();
	if(p->default_mode == from.toStdString())
		p->default_mode = to.toStdString();
	Edited();
	return {};
}

QString RsProfileStore::removeMode(const QString &mode)
{
	rs::Profile *p = Active();
	if(!p || !FindMode(mode))
		return tr("Mode not found.");
	if(p->modes.size() == 1)
		return tr("A profile needs at least one mode.");
	for(auto it = p->modes.begin(); it != p->modes.end(); ++it)
		if(it->name == mode.toStdString())
		{
			p->modes.erase(it);
			break;
		}
	if(p->default_mode == mode.toStdString())
		p->default_mode = p->modes.front().name;
	Edited();
	return {};
}

bool RsProfileStore::switchSessionMode(const QString &mode)
{
	if(preview_active)
		preview.SetMode(mode.toStdString());
	return controller ? controller->SetMode(mode) : preview_active;
}

// ---------------------------------------------------------------- bindings

QVariantMap RsProfileStore::BindingToMap(const rs::Binding &b, int index)
{
	QVariantMap m;
	m["index"] = index;
	m["key"] = QString::fromUtf8(rs::KeyName(b.key));
	QStringList mods;
	for(rs::Key k : b.modifiers)
		mods << QString::fromUtf8(rs::KeyName(k));
	m["modifiers"] = mods;
	m["action"] = QString::fromUtf8(rs::ActionTypeName(b.action.type));
	m["button"] = QString::fromUtf8(rs::PadButtonName(b.action.button));
	m["value"] = b.action.value;
	m["mode"] = QString::fromStdString(b.action.mode);
	m["bindMode"] = b.bind_mode == rs::BindMode::Toggle ? "toggle" : (b.bind_mode == rs::BindMode::Turbo ? "turbo" : "hold");
	m["turboHz"] = b.turbo_hz;
	m["chord"] = QString::fromStdString(rs::DescribeBinding(b));
	m["actionText"] = QString::fromStdString(rs::DescribeAction(b.action));
	return m;
}

bool RsProfileStore::BindingFromMap(const QVariantMap &m, rs::Binding *out, QString *err)
{
	rs::Binding b;
	b.key = rs::KeyFromName(m.value("key").toString().toStdString());
	if(b.key == rs::Key::None)
	{
		*err = tr("Choose a key for this binding.");
		return false;
	}
	for(const QVariant &v : m.value("modifiers").toList())
	{
		rs::Key k = rs::KeyFromName(v.toString().toStdString());
		if(k == rs::Key::None || k == b.key)
			continue;
		if(std::find(b.modifiers.begin(), b.modifiers.end(), k) == b.modifiers.end())
			b.modifiers.push_back(k);
	}
	if(!rs::ActionTypeFromName(m.value("action", "Button").toString().toStdString(), &b.action.type))
	{
		*err = tr("Unknown action.");
		return false;
	}
	if(b.action.type == rs::ActionType::Button && !rs::PadButtonFromName(m.value("button").toString().toStdString(), &b.action.button))
	{
		*err = tr("Choose a controller button.");
		return false;
	}
	b.action.value = (float)m.value("value", 1.0).toDouble();
	b.action.mode = m.value("mode").toString().toStdString();
	if((b.action.type == rs::ActionType::SetMode || b.action.type == rs::ActionType::HoldMode) && b.action.mode.empty())
	{
		*err = tr("Choose the mode to switch to.");
		return false;
	}
	const QString bm = m.value("bindMode", "hold").toString();
	b.bind_mode = bm == "toggle" ? rs::BindMode::Toggle : (bm == "turbo" ? rs::BindMode::Turbo : rs::BindMode::Hold);
	b.turbo_hz = (float)qBound(1.0, m.value("turboHz", 10.0).toDouble(), 30.0);
	*out = b;
	return true;
}

QVariantList RsProfileStore::bindings(const QString &mode) const
{
	QVariantList out;
	if(const rs::Mode *m = FindMode(mode))
		for(size_t i = 0; i < m->bindings.size(); i++)
			out << BindingToMap(m->bindings[i], (int)i);
	return out;
}

QString RsProfileStore::setBinding(const QString &mode, int index, const QVariantMap &map)
{
	rs::Mode *m = FindMode(mode);
	if(!m || index < 0 || index >= (int)m->bindings.size())
		return tr("Binding not found.");
	rs::Binding b;
	QString err;
	if(!BindingFromMap(map, &b, &err))
		return err;
	m->bindings[index] = b;
	Edited();
	return {};
}

QString RsProfileStore::addBinding(const QString &mode, const QVariantMap &map)
{
	rs::Mode *m = FindMode(mode);
	if(!m)
		return tr("Mode not found.");
	rs::Binding b;
	QString err;
	if(!BindingFromMap(map, &b, &err))
		return err;
	m->bindings.push_back(b);
	Edited();
	return {};
}

void RsProfileStore::removeBinding(const QString &mode, int index)
{
	rs::Mode *m = FindMode(mode);
	if(!m || index < 0 || index >= (int)m->bindings.size())
		return;
	m->bindings.erase(m->bindings.begin() + index);
	Edited();
}

QVariantMap RsProfileStore::assignments(const QString &mode) const
{
	QMap<QString, QStringList> acc;
	if(const rs::Mode *m = FindMode(mode))
	{
		for(const auto &b : m->bindings)
		{
			QString id;
			switch(b.action.type)
			{
				case rs::ActionType::Button: id = QString::fromUtf8(rs::PadButtonName(b.action.button)); break;
				case rs::ActionType::SetMode: case rs::ActionType::HoldMode: case rs::ActionType::CycleMode:
				case rs::ActionType::AdsHold: case rs::ActionType::SensitivityScale: case rs::ActionType::ToggleCapture:
				case rs::ActionType::None:
					id = "special"; break;
				default: id = QString::fromUtf8(rs::ActionTypeName(b.action.type)); break;
			}
			QString chord = QString::fromStdString(rs::DescribeBinding(b));
			if(id == "special")
				chord += " → " + QString::fromStdString(rs::DescribeAction(b.action));
			if(!acc[id].contains(chord))
				acc[id] << chord;
		}
		if(m->mouse.target != rs::StickTarget::None)
			acc[m->mouse.target == rs::StickTarget::LeftStick ? "LeftStickMouse" : "RightStickMouse"] << tr("Mouse");
	}
	QVariantMap out;
	for(auto it = acc.cbegin(); it != acc.cend(); ++it)
		out[it.key()] = it.value();
	return out;
}

QString RsProfileStore::keyFromEvent(int qt_key, int sc, int modifiers) const
{
	return QString::fromUtf8(rs::KeyName(RsKeyFromQt(qt_key, (quint32)sc, Qt::KeyboardModifiers(modifiers))));
}

QString RsProfileStore::keyFromMouseButton(int qt_button) const
{
	return QString::fromUtf8(rs::KeyName(RsKeyFromMouseButton((Qt::MouseButton)qt_button)));
}

QString RsProfileStore::describeAction(const QVariantMap &binding) const
{
	rs::Binding b;
	QString err;
	QVariantMap m = binding;
	if(!m.contains("key") || m.value("key").toString().isEmpty() || m.value("key").toString() == "None")
		m["key"] = "A";
	if(!BindingFromMap(m, &b, &err))
		return err;
	return QString::fromStdString(rs::DescribeAction(b.action));
}

// ---------------------------------------------------------------- tuning

QVariantMap RsProfileStore::mouseConfig(const QString &mode) const
{
	QVariantMap v;
	const rs::Mode *m = FindMode(mode);
	if(!m)
		return v;
	const rs::MouseConfig &c = m->mouse;
	v["target"] = c.target == rs::StickTarget::LeftStick ? "left_stick" : (c.target == rs::StickTarget::RightStick ? "right_stick" : "none");
	v["sensitivity"] = c.sensitivity;
	v["sens_x"] = c.sens_x;
	v["sens_y"] = c.sens_y;
	v["invert_y"] = c.invert_y;
	v["ads_multiplier"] = c.ads_multiplier;
	v["ads_follows_l2"] = c.ads_follows_l2;
	v["sample_window_ms"] = c.sample_window_ms;
	v["smoothing"] = c.smoothing;
	v["acceleration"] = c.acceleration;
	v["accel_cap"] = c.accel_cap;
	v["deadzone"] = c.deadzone;
	v["curve"] = c.curve == rs::CurveType::Linear ? "linear" : (c.curve == rs::CurveType::Power ? "power" : "custom");
	v["curve_exponent"] = c.curve_exponent;
	QVariantList pts;
	for(const auto &p : c.curve_points)
		pts << QVariantList{p.in, p.out};
	v["curve_points"] = pts;
	v["anti_deadzone"] = c.anti_deadzone;
	v["anti_deadzone_shape"] = c.anti_deadzone_shape == rs::DeadzoneShape::Axial ? "axial" : "radial";
	v["outer_limit"] = c.outer_limit;
	v["square_saturation"] = c.square_saturation;
	return v;
}

void RsProfileStore::setMouseValue(const QString &mode, const QString &f, const QVariant &value)
{
	rs::Mode *m = FindMode(mode);
	if(!m)
		return;
	rs::MouseConfig &c = m->mouse;
	const float d = (float)value.toDouble();
	if(f == "target")
	{
		QString t = value.toString();
		c.target = t == "left_stick" ? rs::StickTarget::LeftStick : (t == "none" ? rs::StickTarget::None : rs::StickTarget::RightStick);
	}
	else if(f == "sensitivity") c.sensitivity = qBound(0.01f, d, 50.0f);
	else if(f == "sens_x") c.sens_x = qBound(0.05f, d, 10.0f);
	else if(f == "sens_y") c.sens_y = qBound(0.05f, d, 10.0f);
	else if(f == "invert_y") c.invert_y = value.toBool();
	else if(f == "ads_multiplier") c.ads_multiplier = qBound(0.05f, d, 3.0f);
	else if(f == "ads_follows_l2") c.ads_follows_l2 = value.toBool();
	else if(f == "sample_window_ms") c.sample_window_ms = qBound(1.0f, d, 100.0f);
	else if(f == "smoothing") c.smoothing = qBound(0.0f, d, 1.0f);
	else if(f == "acceleration") c.acceleration = qBound(0.0f, d, 5.0f);
	else if(f == "accel_cap") c.accel_cap = qBound(0.0f, d, 5.0f);
	else if(f == "deadzone") c.deadzone = qBound(0.0f, d, 0.5f);
	else if(f == "curve")
	{
		QString t = value.toString();
		c.curve = t == "linear" ? rs::CurveType::Linear : (t == "custom" ? rs::CurveType::Custom : rs::CurveType::Power);
		if(c.curve == rs::CurveType::Custom && c.curve_points.empty())
			c.curve_points = {{0.25f, 0.35f}, {0.5f, 0.6f}, {0.75f, 0.82f}};
	}
	else if(f == "curve_exponent") c.curve_exponent = qBound(0.1f, d, 5.0f);
	else if(f == "curve_points")
	{
		c.curve_points.clear();
		for(const QVariant &pv : value.toList())
		{
			QVariantList pair = pv.toList();
			if(pair.size() == 2)
				c.curve_points.push_back({qBound(0.0f, (float)pair[0].toDouble(), 1.0f), qBound(0.0f, (float)pair[1].toDouble(), 1.0f)});
		}
		std::sort(c.curve_points.begin(), c.curve_points.end(), [](const rs::CurvePoint &a, const rs::CurvePoint &b) { return a.in < b.in; });
	}
	else if(f == "anti_deadzone") c.anti_deadzone = qBound(0.0f, d, 0.9f);
	else if(f == "anti_deadzone_shape") c.anti_deadzone_shape = value.toString() == "axial" ? rs::DeadzoneShape::Axial : rs::DeadzoneShape::Radial;
	else if(f == "outer_limit") c.outer_limit = qBound(0.1f, d, 1.0f);
	else if(f == "square_saturation") c.square_saturation = qBound(0.0f, d, 1.0f);
	else
		return;
	Edited();
}

QVariantMap RsProfileStore::keyStickConfig(const QString &mode, const QString &which) const
{
	QVariantMap v;
	const rs::Mode *m = FindMode(mode);
	if(!m)
		return v;
	const rs::KeyStickConfig &k = which == "right" ? m->right_keys : m->left_keys;
	v["ramp_up_ms"] = k.ramp_up_ms;
	v["ramp_down_ms"] = k.ramp_down_ms;
	v["socd"] = k.socd == rs::Socd::Neutral ? "neutral" : (k.socd == rs::Socd::FirstWins ? "first_wins" : "last_wins");
	v["circular_diagonals"] = k.circular_diagonals;
	return v;
}

void RsProfileStore::setKeyStickValue(const QString &mode, const QString &which, const QString &f, const QVariant &value)
{
	rs::Mode *m = FindMode(mode);
	if(!m)
		return;
	rs::KeyStickConfig &k = which == "right" ? m->right_keys : m->left_keys;
	if(f == "ramp_up_ms") k.ramp_up_ms = qBound(0.0f, (float)value.toDouble(), 2000.0f);
	else if(f == "ramp_down_ms") k.ramp_down_ms = qBound(0.0f, (float)value.toDouble(), 2000.0f);
	else if(f == "socd")
	{
		QString s = value.toString();
		k.socd = s == "neutral" ? rs::Socd::Neutral : (s == "first_wins" ? rs::Socd::FirstWins : rs::Socd::LastWins);
	}
	else if(f == "circular_diagonals") k.circular_diagonals = value.toBool();
	else
		return;
	Edited();
}

double RsProfileStore::triggerRamp(const QString &mode) const
{
	const rs::Mode *m = FindMode(mode);
	return m ? m->trigger_ramp_ms : 0.0;
}

void RsProfileStore::setTriggerRamp(const QString &mode, double ms)
{
	rs::Mode *m = FindMode(mode);
	if(!m)
		return;
	m->trigger_ramp_ms = (float)qBound(0.0, ms, 2000.0);
	Edited();
}

QVariantList RsProfileStore::curvePreview(const QString &mode, int steps) const
{
	QVariantList out;
	const rs::Mode *m = FindMode(mode);
	if(!m)
		return out;
	steps = qBound(2, steps, 400);
	for(int i = 0; i <= steps; i++)
	{
		float in = (float)i / steps;
		rs::Vec2 o = rs::MouseStick::Shape(m->mouse, {in, 0.0f});
		out << QVariant(QPointF(in, o.x));
	}
	return out;
}

// ---------------------------------------------------------------- live telemetry & preview

void RsProfileStore::AttachController(RsInputController *c)
{
	controller = c;
	if(c)
	{
		c->SetProfile(ActiveProfile());
		c->SetCaptureKey(rs::KeyFromName(CaptureKey().toStdString()));
		c->SetTickRate(TickRate());
		connect(c, &QObject::destroyed, this, [this] { emit sessionActiveChanged(); UpdateLive(); });
	}
	if(!live_timer.isActive())
		live_timer.start();
	emit sessionActiveChanged();
}

void RsProfileStore::DetachController(RsInputController *c)
{
	if(controller == c)
		controller = nullptr;
	emit sessionActiveChanged();
	UpdateLive();
}

void RsProfileStore::startPreview()
{
	if(preview_active)
		return;
	preview.SetProfile(ActiveProfile());
	preview.ReleaseAll();
	preview_active = true;
	preview_timer.start();
	live_timer.start();
	emit previewActiveChanged();
}

void RsProfileStore::stopPreview()
{
	if(!preview_active)
		return;
	preview.ReleaseAll();
	preview_timer.stop();
	preview_active = false;
	preview_pad = rs::PadState();
	emit previewActiveChanged();
	UpdateLive();
}

void RsProfileStore::previewKey(int qt_key, int sc, int modifiers, bool pressed)
{
	previewKeyName(keyFromEvent(qt_key, sc, modifiers), pressed);
}

void RsProfileStore::previewKeyName(const QString &name, bool pressed)
{
	if(!preview_active)
		return;
	rs::Key k = rs::KeyFromName(name.toStdString());
	if(k == rs::Key::None)
		return;
	uint64_t now = (uint64_t)(preview_clock.nsecsElapsed() / 1000);
	if(pressed)
		preview.KeyDown(k, now);
	else
		preview.KeyUp(k, now);
}

void RsProfileStore::previewMouseDelta(double dx, double dy)
{
	if(!preview_active)
		return;
	preview_dx_rem += dx;
	preview_dy_rem += dy;
	int ix = (int)preview_dx_rem, iy = (int)preview_dy_rem;
	preview_dx_rem -= ix;
	preview_dy_rem -= iy;
	if(ix || iy)
		preview.MouseDelta(ix, iy, (uint64_t)(preview_clock.nsecsElapsed() / 1000));
}

void RsProfileStore::previewWheel(int angle_delta_y)
{
	if(!preview_active)
		return;
	preview_wheel_rem += angle_delta_y;
	int n = (int)(preview_wheel_rem / 120);
	preview_wheel_rem -= n * 120;
	if(n)
		preview.Wheel(n, 0, (uint64_t)(preview_clock.nsecsElapsed() / 1000));
}

void RsProfileStore::UpdateLive()
{
	QVariantMap m;
	rs::PadState pad;
	const rs::Engine *eng = nullptr;
	if(controller)
	{
		ChiakiControllerState s = controller->State();
		pad.buttons = s.buttons;
		pad.l2 = s.l2_state;
		pad.r2 = s.r2_state;
		pad.lx = s.left_x;
		pad.ly = s.left_y;
		pad.rx = s.right_x;
		pad.ry = s.right_y;
		eng = &controller->Engine();
		m["source"] = "session";
		m["captured"] = controller->Captured();
		m["rawInput"] = controller->UsingRawInput();
		m["tickHz"] = controller->MeasuredTickHz();
	}
	else if(preview_active)
	{
		pad = preview_pad;
		eng = &preview;
		m["source"] = "preview";
		m["captured"] = true;
		m["rawInput"] = false;
		m["tickHz"] = 250.0;
	}
	else
	{
		m["source"] = "none";
		m["captured"] = false;
		if(live_timer.isActive())
			live_timer.stop();
	}
	m["buttons"] = (qint64)pad.buttons;
	m["l2"] = pad.l2 / 255.0;
	m["r2"] = pad.r2 / 255.0;
	m["lx"] = pad.lx / 32767.0;
	m["ly"] = pad.ly / 32767.0;
	m["rx"] = pad.rx / 32767.0;
	m["ry"] = pad.ry / 32767.0;
	if(eng)
	{
		const rs::EngineTelemetry &t = eng->Telemetry();
		m["mode"] = QString::fromStdString(eng->ActiveMode());
		m["mouseVx"] = t.mouse_velocity.x;
		m["mouseVy"] = t.mouse_velocity.y;
		m["mouseInput"] = t.mouse_input_magnitude;
		m["ads"] = t.ads_active;
		m["sensScale"] = t.sens_scale;
		m["heldKeys"] = t.held_keys;
	}
	m["profile"] = active;
	if(m != live)
	{
		live = m;
		emit liveChanged();
	}
}
