// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// RemoteShift: stream diagnostics. Keeps a rolling history of values the app actually measures
// and derives troubleshooting advice from it. Nothing here is estimated or sent anywhere.
#pragma once

#include <QAtomicInt>
#include <QElapsedTimer>
#include <QObject>
#include <QUrl>
#include <QVariant>
#include <QVector>

struct RsDiagSample
{
	double bitrate_mbps = 0;
	double packet_loss = 0;       // 0..1, averaged by the session
	int dropped_frames_total = 0; // frames the renderer dropped since the session started
	int frames_lost_total = 0;    // frames lost on the network since the session started
	double queue_depth = 0;       // decoded frames waiting to be shown
	double frame_age_ms = 0;      // age of the oldest waiting frame
	double rtt_ms = -1;           // network round trip measured while connecting, -1 = not measured
	double input_hz = -1;         // keyboard & mouse engine update rate, -1 = engine off
	QString input_mode;
	QString input_profile;
};

class RsDiagnostics : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool streaming READ Streaming NOTIFY streamingChanged)
	Q_PROPERTY(bool hasData READ HasData NOTIFY sampled)
	Q_PROPERTY(int revision READ Revision NOTIFY sampled)
	Q_PROPERTY(QVariantMap current READ Current NOTIFY sampled)
	Q_PROPERTY(QVariantMap session READ Session NOTIFY streamingChanged)
	Q_PROPERTY(QVariantList tips READ Tips NOTIFY sampled)
	Q_PROPERTY(int seconds READ Seconds NOTIFY sampled)

public:
	explicit RsDiagnostics(QObject *parent = nullptr);
	static RsDiagnostics *Instance();

	static constexpr int kHistory = 600; // seconds kept

	bool Streaming() const { return streaming; }
	bool HasData() const { return !fps.isEmpty(); }
	int Revision() const { return revision; }
	QVariantMap Current() const { return current; }
	QVariantMap Session() const { return session; }
	QVariantList Tips() const { return tips; }
	int Seconds() const { return seconds; }

	// Session lifecycle (called by the backend).
	void BeginSession(int width, int height, int target_fps, int target_bitrate_kbps, const QString &codec, const QString &decoder);
	void EndSession();
	// Called from the decoder thread for every decoded video frame.
	void NoteFrame() { frame_counter.fetchAndAddRelaxed(1); }
	// Called once per second while streaming.
	// `interval_s` overrides the measured time since the previous sample (tests and the UI harness).
	void AddSample(const RsDiagSample &s, double interval_s = -1);

	// name: fps, bitrate, loss, dropped, lost, frameAge, queue, cpu, inputHz
	Q_INVOKABLE QVariantList series(const QString &name) const;
	Q_INVOKABLE QString exportReport(const QUrl &file) const;
	QString Report() const;

signals:
	void streamingChanged();
	void sampled();

private:
	double ProcessCpuPercent();
	void UpdateTips();
	const QVector<double> *Find(const QString &name) const;
	static void Push(QVector<double> &v, double x);

	bool streaming = false;
	int revision = 0;
	int seconds = 0;
	QVariantMap current, session;
	QVariantList tips;
	QVector<double> fps, bitrate, loss, dropped, lost, frame_age, queue, cpu, input_hz;
	QAtomicInt frame_counter;
	QElapsedTimer sample_clock;
	int last_dropped = 0, last_lost = 0;
	int target_fps = 0;
	double rtt_ms = -1;
	qint64 last_cpu_time_us = -1;
	double last_cpu_pct = -1;
	QElapsedTimer cpu_clock;
};
