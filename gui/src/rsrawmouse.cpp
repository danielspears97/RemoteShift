// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "rsrawmouse.h"

#include <QCoreApplication>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

RsRawMouse::RsRawMouse(DeltaCallback cb) : callback(std::move(cb)) {}

RsRawMouse::~RsRawMouse()
{
	Stop();
	if(filter_installed && QCoreApplication::instance())
		QCoreApplication::instance()->removeNativeEventFilter(this);
}

bool RsRawMouse::Supported()
{
#ifdef _WIN32
	return true;
#else
	return false;
#endif
}

#ifdef _WIN32

bool RsRawMouse::Start(QWindow *w)
{
	if(active)
		return true;
	if(!w)
		return false;
	window = w;
	HWND hwnd = reinterpret_cast<HWND>(w->winId());
	RAWINPUTDEVICE rid;
	rid.usUsagePage = 0x01; // generic desktop
	rid.usUsage = 0x02;     // mouse
	rid.dwFlags = 0;        // foreground only: no input is read while another app is focused
	rid.hwndTarget = hwnd;
	if(!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
		return false;
	if(!filter_installed)
	{
		QCoreApplication::instance()->installNativeEventFilter(this);
		filter_installed = true;
	}
	active = true;
	Reclip();
	return true;
}

void RsRawMouse::Stop()
{
	if(!active)
		return;
	RAWINPUTDEVICE rid;
	rid.usUsagePage = 0x01;
	rid.usUsage = 0x02;
	rid.dwFlags = RIDEV_REMOVE;
	rid.hwndTarget = nullptr;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));
	ClipCursor(nullptr);
	active = false;
}

void RsRawMouse::Reclip()
{
	if(!active || !window)
		return;
	HWND hwnd = reinterpret_cast<HWND>(window->winId());
	RECT rc;
	if(!GetClientRect(hwnd, &rc))
		return;
	POINT centre = {(rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2};
	ClientToScreen(hwnd, &centre);
	// Confine the (hidden) cursor to a small box in the middle of the stream so clicks can
	// never land on another window or the taskbar while playing.
	RECT clip = {centre.x - 8, centre.y - 8, centre.x + 8, centre.y + 8};
	SetCursorPos(centre.x, centre.y);
	ClipCursor(&clip);
}

bool RsRawMouse::nativeEventFilter(const QByteArray &event_type, void *message, qintptr *)
{
	if(!active || event_type != "windows_generic_MSG")
		return false;
	MSG *msg = static_cast<MSG *>(message);
	if(msg->message != WM_INPUT)
		return false;
	RAWINPUT raw;
	UINT size = sizeof(raw);
	if(GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1)
		return false;
	if(raw.header.dwType == RIM_TYPEMOUSE && !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
	{
		const LONG dx = raw.data.mouse.lLastX, dy = raw.data.mouse.lLastY;
		if((dx || dy) && callback)
			callback((int)dx, (int)dy);
	}
	// Let DefWindowProc run so the system can clean up the raw input buffer.
	return false;
}

#else

bool RsRawMouse::Start(QWindow *) { return false; }
void RsRawMouse::Stop() { active = false; }
void RsRawMouse::Reclip() {}
bool RsRawMouse::nativeEventFilter(const QByteArray &, void *, qintptr *) { return false; }

#endif
