// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
// RemoteShift input engine — platform-neutral input codes.
#pragma once

#include <cstdint>
#include <string>

namespace rs {

// Platform-neutral physical input codes. Hosts translate native events
// (Windows virtual keys / scan codes, Qt keys) into these. Names are what
// profiles store on disk, so they must stay stable.
enum class Key : uint16_t {
	None = 0,
	// Letters
	A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
	// Top-row digits
	Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
	// Function keys
	F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
	// Modifiers (left/right distinguished where the platform allows)
	LShift, RShift, LCtrl, RCtrl, LAlt, RAlt, LWin, RWin,
	// Editing / navigation
	Escape, Tab, CapsLock, Space, Enter, Backspace, Insert, Delete, Home, End, PageUp, PageDown,
	Up, Down, Left, Right,
	// Punctuation
	Grave, Minus, Equals, LBracket, RBracket, Backslash, Semicolon, Apostrophe, Comma, Period, Slash,
	// Numpad
	Kp0, Kp1, Kp2, Kp3, Kp4, Kp5, Kp6, Kp7, Kp8, Kp9, KpAdd, KpSubtract, KpMultiply, KpDivide, KpDecimal, KpEnter,
	// Misc
	PrintScreen, ScrollLock, Pause, Menu,
	// Mouse buttons
	MouseLeft, MouseRight, MouseMiddle, MouseX1, MouseX2,
	// Mouse wheel (delivered as short pulses)
	WheelUp, WheelDown, WheelLeft, WheelRight,
	Count
};

constexpr int kKeyCount = static_cast<int>(Key::Count);

const char *KeyName(Key k);
// Returns Key::None when the name is unknown. Case-insensitive; accepts a few aliases
// ("Shift" -> LShift, "Ctrl" -> LCtrl, "Alt" -> LAlt, "Mouse1" -> MouseLeft, ...).
Key KeyFromName(const std::string &name);
bool IsMouseKey(Key k);
bool IsWheelKey(Key k);
bool IsModifierKey(Key k);

} // namespace rs
