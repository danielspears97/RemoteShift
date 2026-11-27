// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "rsdiagnostics.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>

#include <algorithm>
#include <numeric>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#endif

static RsDiagnostics *g_diag = nullptr;

RsDiagnostics *RsDiagnostics::Instance() { return g_diag; }

RsDiagnostics::RsDiagnostics(QObject *parent) : QObject(parent)
{
	g_diag = this;
	connect(this, &QObject::destroyed, [] { g_diag = nullptr; });
}

void RsDiagnostics::Push(QVector<double> &v, double x)
{
	v.append(x);
	if(v.size() > kHistory)
		v.remove(0, v.size() - kHistory);
}

static qint64 ProcessCpuTimeUs()
{
#ifdef _WIN32
	FILETIME creation, exit, kernel, user;
	if(!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
		return -1;
	auto to64 = [](const FILETIME &f) { return ((quint64)f.dwHighDateTime << 32) | f.dwLowDateTime; };
	return (qint64)((to64(kernel) + to64(user)) / 10); // 100 ns units -> us
#else
	struct rusage ru;
	if(getrusage(RUSAGE_SELF, &ru) != 0)
		return -1;
	return (qint64)ru.ru_utime.tv_sec * 1000000 + ru.ru_utime.tv_usec + (qint64)ru.ru_stime.tv_sec * 1000000 + ru.ru_stime.tv_usec;
#endif
}

// CPU used by this process since the previous call, as a share of all logical cores.
double RsDiagnostics::ProcessCpuPercent()
{
	const qint64 now = ProcessCpuTimeUs();
	if(now < 0)
		return -1;
	if(last_cpu_time_us >= 0 && cpu_clock.isValid())
	{
		const qint64 wall = cpu_clock.nsecsElapsed() / 1000;
		if(wall < 200000)
			return last_cpu_pct; // too short an interval to be meaningful
		last_cpu_pct = qBound(0.0, 100.0 * (double)(now - last_cpu_time_us) / (double)wall / qMax(1, QThread::idealThreadCount()), 100.0);
	}
	last_cpu_time_us = now;
	cpu_clock.restart();
	return last_cpu_pct;
}

void RsDiagnostics::BeginSession(int width, int height, int fps_target, int target_bitrate_kbps, const QString &codec, const QString &decoder)
{
	fps.clear(); bitrate.clear(); loss.clear(); dropped.clear(); lost.clear();
	frame_age.clear(); queue.clear(); cpu.clear(); input_hz.clear();
	tips.clear();
	current.clear();
	seconds = 0;
	last_dropped = last_lost = 0;
	rtt_ms = -1;
	target_fps = fps_target;
	frame_counter.storeRelaxed(0);
	sample_clock.restart();
	last_cpu_time_us = -1;
	last_cpu_pct = -1;
	ProcessCpuPercent();
	session.clear();
	session["width"] = width;
	session["height"] = height;
	session["targetFps"] = fps_target;
	session["targetBitrateMbps"] = target_bitrate_kbps / 1000.0;
	session["codec"] = codec;
	session["decoder"] = decoder.isEmpty() ? tr("software") : decoder;
	session["started"] = QDateTime::currentDateTime().toString(Qt::ISODate);
	streaming = true;
	revision++;
	emit streamingChanged();
	emit sampled();
}

void RsDiagnostics::EndSession()
{
	if(!streaming)
		return;
	streaming = false;
	emit streamingChanged();
}

void RsDiagnostics::AddSample(const RsDiagSample &s, double interval_s)
{
	if(!streaming)
		return;
	const double dt = interval_s > 0 ? interval_s : (sample_clock.isValid() ? sample_clock.nsecsElapsed() / 1e9 : 1.0);
	sample_clock.restart();
	const int frames = frame_counter.fetchAndStoreRelaxed(0);
	const double fps_now = dt > 0.05 ? frames / dt : 0.0;
	const int d_dropped = qMax(0, s.dropped_frames_total - last_dropped);
	const int d_lost = qMax(0, s.frames_lost_total - last_lost);
	last_dropped = s.dropped_frames_total;
	last_lost = s.frames_lost_total;
	const double cpu_now = ProcessCpuPercent();
	if(s.rtt_ms > 0)
		rtt_ms = s.rtt_ms;

	Push(fps, fps_now);
	Push(bitrate, s.bitrate_mbps);
	Push(loss, s.packet_loss * 100.0);
	Push(dropped, d_dropped);
	Push(lost, d_lost);
	Push(frame_age, s.frame_age_ms);
	Push(queue, s.queue_depth);
	Push(cpu, cpu_now < 0 ? 0 : cpu_now);
	Push(input_hz, s.input_hz < 0 ? 0 : s.input_hz);
	seconds++;

	QVariantMap c;
	c["fps"] = fps_now;
	c["bitrate"] = s.bitrate_mbps;
	c["loss"] = s.packet_loss * 100.0;
	c["droppedTotal"] = s.dropped_frames_total;
	c["lostTotal"] = s.frames_lost_total;
	c["frameAge"] = s.frame_age_ms;
	c["queue"] = s.queue_depth;
	c["cpu"] = cpu_now;        // -1 = unavailable
	c["rtt"] = rtt_ms;         // -1 = not measured for this connection
	c["inputHz"] = s.input_hz; // -1 = engine off
	c["inputMode"] = s.input_mode;
	c["inputProfile"] = s.input_profile;
	current = c;
	UpdateTips();
	revision++;
	emit sampled();
}

const QVector<double> *RsDiagnostics::Find(const QString &name) const
{
	if(name == "fps") return &fps;
	if(name == "bitrate") return &bitrate;
	if(name == "loss") return &loss;
	if(name == "dropped") return &dropped;
	if(name == "lost") return &lost;
	if(name == "frameAge") return &frame_age;
	if(name == "queue") return &queue;
	if(name == "cpu") return &cpu;
	if(name == "inputHz") return &input_hz;
	return nullptr;
}

QVariantList RsDiagnostics::series(const QString &name) const
{
	QVariantList out;
	if(const QVector<double> *v = Find(name))
	{
		out.reserve(v->size());
		for(double x : *v)
			out << x;
	}
	return out;
}

static double TailMean(const QVector<double> &v, int n)
{
	if(v.isEmpty())
		return 0;
	const int from = qMax(0, v.size() - n);
	double s = 0;
	for(int i = from; i < v.size(); i++)
		s += v[i];
	return s / (v.size() - from);
}

static double TailSum(const QVector<double> &v, int n)
{
	return TailMean(v, n) * qMin(n, v.size());
}

void RsDiagnostics::UpdateTips()
{
	tips.clear();
	if(seconds < 5)
		return;
	auto add = [this](const char *level, const QString &title, const QString &text) {
		tips << QVariantMap{{"level", level}, {"title", title}, {"text", text}};
	};
	const int w = 30;
	const double loss_avg = TailMean(loss, w);
	const double fps_avg = TailMean(fps, w);
	const double age_avg = TailMean(frame_age, w);
	const double cpu_avg = TailMean(cpu, w);
	const double lost_sum = TailSum(lost, w);
	const double dropped_sum = TailSum(dropped, w);
	const double bitrate_avg = TailMean(bitrate, w);
	const double target_mbps = session.value("targetBitrateMbps").toDouble();

	if(loss_avg >= 1.0 || lost_sum >= 5)
		add("bad", tr("Network is losing packets (%1% loss, %2 frames lost in the last 30 s)").arg(loss_avg, 0, 'f', 1).arg((int)lost_sum),
			tr("Wire the PS5 and this PC to the router if either is on Wi-Fi. If you must use Wi-Fi, use 5 GHz and lower the bitrate in Settings → Video."));
	else if(loss_avg >= 0.2)
		add("warn", tr("Some packet loss (%1%)").arg(loss_avg, 0, 'f', 1),
			tr("Occasional artifacts are likely. A wired connection for the PS5 usually fixes this."));

	if(rtt_ms >= 25)
		add("warn", tr("High network round trip (%1 ms when connecting)").arg(rtt_ms, 0, 'f', 0),
			tr("This delay is added to every input. On a home network it should be under 5 ms: check for Wi-Fi extenders, powerline adapters or a VPN in the path."));

	if(target_fps > 0 && fps_avg > 1 && fps_avg < target_fps * 0.9)
		add(loss_avg >= 0.2 ? "warn" : "bad", tr("Receiving %1 fps of a %2 fps stream").arg(fps_avg, 0, 'f', 0).arg(target_fps),
			loss_avg >= 0.2 ? tr("Frames are being lost on the network; fixing the packet loss above should restore the frame rate.")
			                : tr("The console is sending fewer frames than requested or decoding is falling behind. Try another hardware decoder in Settings → Video, or 720p."));

	if(age_avg >= 40)
		add("warn", tr("Frames wait %1 ms before being shown").arg(age_avg, 0, 'f', 0),
			tr("Decoded frames are queuing up, which adds latency. Use the Default render preset instead of High Quality, and make sure a hardware decoder is selected."));

	if(dropped_sum >= 30)
		add("warn", tr("%1 frames dropped by the renderer in the last 30 s").arg((int)dropped_sum),
			tr("Your display refresh rate may not match the stream. Use a 60/120/240 Hz mode, or lower the render preset."));

	if(cpu_avg >= 60)
		add("warn", tr("RemoteShift is using %1% of the CPU").arg(cpu_avg, 0, 'f', 0),
			tr("That usually means software decoding. Pick a hardware decoder in Settings → Video."));

	if(session.value("decoder").toString() == tr("software"))
		add("warn", tr("Software video decoding is active"), tr("Hardware decoding lowers latency and CPU load. Select one in Settings → Video (on NVIDIA cards choose cuda or d3d11va)."));

	if(target_mbps > 0 && bitrate_avg > 0.5 && bitrate_avg < target_mbps * 0.35 && seconds > 20)
		add("info", tr("Bitrate is well below the configured %1 Mbps").arg(target_mbps, 0, 'f', 0),
			tr("That is normal for calm scenes. If the picture looks blocky in motion, the console is limiting the bitrate because of network conditions."));

	const double hz = current.value("inputHz").toDouble();
	if(hz > 0 && hz < 100)
		add("warn", tr("Keyboard & mouse engine is updating at only %1 Hz").arg(hz, 0, 'f', 0),
			tr("The system is too busy to keep the configured input rate. Close heavy background apps."));

	if(tips.isEmpty())
		add("good", tr("Stream looks healthy"), tr("No packet loss, frame rate on target and no decode backlog over the last 30 seconds."));
}

QString RsDiagnostics::Report() const
{
	// Deliberately contains no host names, addresses, account IDs or keys.
	QString r;
	QTextStream out(&r);
	out << "RemoteShift diagnostics report\n";
	out << "Version: " << QCoreApplication::applicationVersion() << "\n";
	out << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
	out << "System: " << QSysInfo::prettyProductName() << " (" << QSysInfo::currentCpuArchitecture() << "), "
	    << QThread::idealThreadCount() << " logical cores\n\n";
	if(session.isEmpty())
	{
		out << "No stream session has been recorded since the app started.\n";
		return r;
	}
	out << "Session: " << session.value("width").toInt() << "x" << session.value("height").toInt() << " @ "
	    << session.value("targetFps").toInt() << " fps, " << session.value("codec").toString() << ", target "
	    << session.value("targetBitrateMbps").toDouble() << " Mbps, decoder " << session.value("decoder").toString() << "\n";
	out << "Started: " << session.value("started").toString() << ", samples: " << seconds << " s"
	    << (streaming ? " (still streaming)" : "") << "\n";
	out << "Network round trip at connect: " << (rtt_ms > 0 ? QString::number(rtt_ms, 'f', 1) + " ms" : QString("not measured")) << "\n\n";
	auto stat = [&out](const char *label, const QVector<double> &v, const char *unit, int decimals) {
		if(v.isEmpty())
			return;
		const double mn = *std::min_element(v.begin(), v.end()), mx = *std::max_element(v.begin(), v.end());
		const double avg = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
		out << QString("%1 avg %2  min %3  max %4 %5\n").arg(QString(label), -22).arg(avg, 0, 'f', decimals).arg(mn, 0, 'f', decimals).arg(mx, 0, 'f', decimals).arg(unit);
	};
	stat("Source frame rate", fps, "fps", 1);
	stat("Bitrate", bitrate, "Mbps", 1);
	stat("Packet loss", loss, "%", 2);
	stat("Frame wait", frame_age, "ms", 1);
	stat("Decode queue", queue, "frames", 2);
	stat("App CPU", cpu, "%", 1);
	stat("Input update rate", input_hz, "Hz", 0);
	out << "Frames lost (network): " << (int)std::accumulate(lost.begin(), lost.end(), 0.0) << "\n";
	out << "Frames dropped (render): " << (int)std::accumulate(dropped.begin(), dropped.end(), 0.0) << "\n\n";
	out << "Findings:\n";
	for(const QVariant &t : tips)
	{
		const QVariantMap m = t.toMap();
		out << " [" << m.value("level").toString() << "] " << m.value("title").toString() << "\n       " << m.value("text").toString() << "\n";
	}
	out << "\nPer-second samples (fps, Mbps, loss%, frame wait ms, cpu%):\n";
	for(int i = 0; i < fps.size(); i++)
		out << QString("%1,%2,%3,%4,%5\n").arg(fps[i], 0, 'f', 0).arg(bitrate[i], 0, 'f', 1).arg(loss[i], 0, 'f', 2).arg(frame_age[i], 0, 'f', 0).arg(cpu[i], 0, 'f', 0);
	return r;
}

QString RsDiagnostics::exportReport(const QUrl &url) const
{
	QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
	if(!path.endsWith(".txt", Qt::CaseInsensitive))
		path += ".txt";
	QFile f(path);
	if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		return tr("Could not write file: %1").arg(f.errorString());
	f.write(Report().toUtf8());
	return {};
}
