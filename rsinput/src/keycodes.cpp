// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include "rsinput/keycodes.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace rs {

static const char *const kNames[kKeyCount] = {
	"None",
	"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
	"0","1","2","3","4","5","6","7","8","9",
	"F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12",
	"LShift","RShift","LCtrl","RCtrl","LAlt","RAlt","LWin","RWin",
	"Escape","Tab","CapsLock","Space","Enter","Backspace","Insert","Delete","Home","End","PageUp","PageDown",
	"Up","Down","Left","Right",
	"Grave","Minus","Equals","LBracket","RBracket","Backslash","Semicolon","Apostrophe","Comma","Period","Slash",
	"Kp0","Kp1","Kp2","Kp3","Kp4","Kp5","Kp6","Kp7","Kp8","Kp9","KpAdd","KpSubtract","KpMultiply","KpDivide","KpDecimal","KpEnter",
	"PrintScreen","ScrollLock","Pause","Menu",
	"MouseLeft","MouseRight","MouseMiddle","MouseX1","MouseX2",
	"WheelUp","WheelDown","WheelLeft","WheelRight",
};

static std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return s;
}

const char *KeyName(Key k)
{
	int i = static_cast<int>(k);
	if(i < 0 || i >= kKeyCount)
		return "None";
	return kNames[i];
}

Key KeyFromName(const std::string &name)
{
	static const std::unordered_map<std::string, Key> table = [] {
		std::unordered_map<std::string, Key> t;
		for(int i = 0; i < kKeyCount; i++)
			t[Lower(kNames[i])] = static_cast<Key>(i);
		const std::pair<const char *, Key> aliases[] = {
			{"shift", Key::LShift}, {"ctrl", Key::LCtrl}, {"control", Key::LCtrl}, {"alt", Key::LAlt},
			{"win", Key::LWin}, {"esc", Key::Escape}, {"return", Key::Enter}, {"del", Key::Delete},
			{"mouse1", Key::MouseLeft}, {"mouse2", Key::MouseRight}, {"mouse3", Key::MouseMiddle},
			{"mouse4", Key::MouseX1}, {"mouse5", Key::MouseX2}, {"lmb", Key::MouseLeft}, {"rmb", Key::MouseRight},
			{"mmb", Key::MouseMiddle}, {"`", Key::Grave}, {"-", Key::Minus}, {"=", Key::Equals},
			{"[", Key::LBracket}, {"]", Key::RBracket}, {"\\", Key::Backslash}, {";", Key::Semicolon},
			{"'", Key::Apostrophe}, {",", Key::Comma}, {".", Key::Period}, {"/", Key::Slash},
			{"num0", Key::Num0}, {"num1", Key::Num1}, {"num2", Key::Num2}, {"num3", Key::Num3}, {"num4", Key::Num4},
			{"num5", Key::Num5}, {"num6", Key::Num6}, {"num7", Key::Num7}, {"num8", Key::Num8}, {"num9", Key::Num9},
		};
		for(const auto &a : aliases)
			t[a.first] = a.second;
		return t;
	}();
	auto it = table.find(Lower(name));
	return it == table.end() ? Key::None : it->second;
}

bool IsMouseKey(Key k) { return k >= Key::MouseLeft && k <= Key::WheelRight; }
bool IsWheelKey(Key k) { return k >= Key::WheelUp && k <= Key::WheelRight; }
bool IsModifierKey(Key k) { return k >= Key::LShift && k <= Key::RWin; }

} // namespace rs
