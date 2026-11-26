// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// RemoteShift: Windows Raw Input mouse source + cursor confinement.
#pragma once

#include <QAbstractNativeEventFilter>
#include <QWindow>

#include <functional>

class RsRawMouse : public QAbstractNativeEventFilter
{
public:
	using DeltaCallback = std::function<void(int dx, int dy)>;

	explicit RsRawMouse(DeltaCallback cb);
	~RsRawMouse() override;

	// True on platforms where raw (unaccelerated, un-clipped) relative motion is available.
	static bool Supported();

	// Registers for WM_INPUT on the window and confines/hides the cursor. Returns false if
	// registration failed (the caller then falls back to cursor-warp deltas).
	bool Start(QWindow *window);
	void Stop();
	bool Active() const { return active; }
	// Re-applies the cursor clip after the window moved or resized.
	void Reclip();

	bool nativeEventFilter(const QByteArray &event_type, void *message, qintptr *result) override;

private:
	DeltaCallback callback;
	QWindow *window = nullptr;
	bool active = false;
	bool filter_installed = false;
};
