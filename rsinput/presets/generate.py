#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
"""Generates the bundled RemoteShift starter profiles.

The GTA 6 profile mirrors Grand Theft Auto V's default PlayStation layout,
because Rockstar has not published GTA 6's controller scheme in a form we can
verify. Every binding is editable in the app's Keybind Editor.

Run:  python3 generate.py   (writes *.json next to this script)
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))


def bind(key, action, mods=None, **kw):
    b = {"key": key, "action": action}
    if mods:
        b["modifiers"] = mods
    b.update(kw)
    return b


def btn(name):
    return {"type": "Button", "button": name}


def act(t, value=None, mode=None):
    a = {"type": t}
    if value is not None:
        a["value"] = value
    if mode is not None:
        a["mode"] = mode
    return a


def mouse(**kw):
    base = {
        "target": "right_stick", "sensitivity": 1.0, "sens_x": 1.0, "sens_y": 0.9,
        "invert_y": False, "ads_multiplier": 0.6, "ads_follows_l2": True,
        "sample_window_ms": 12.0, "smoothing": 0.15, "acceleration": 0.0, "accel_cap": 2.0,
        "deadzone": 0.0, "curve": "power", "curve_exponent": 0.75, "curve_points": [],
        "anti_deadzone": 0.20, "anti_deadzone_shape": "radial", "outer_limit": 1.0,
        "square_saturation": 0.25,
    }
    base.update(kw)
    return base


def keys(**kw):
    base = {"ramp_up_ms": 0.0, "ramp_down_ms": 0.0, "socd": "last_wins", "circular_diagonals": True}
    base.update(kw)
    return base


def mode_hotkeys():
    return [
        bind("F1", act("SetMode", mode="On Foot")),
        bind("F2", act("SetMode", mode="Driving")),
        bind("F3", act("SetMode", mode="Flying")),
        bind("F4", act("SetMode", mode="Flying (Mouse Steer)")),
        bind("F5", act("SetMode", mode="Menus")),
        bind("Grave", act("CycleMode")),
    ]


def wasd_left(value=1.0):
    return [
        bind("W", act("LeftStickUp", value)), bind("S", act("LeftStickDown", value)),
        bind("A", act("LeftStickLeft", value)), bind("D", act("LeftStickRight", value)),
    ]


# Keys that change mode AND press a button are bound to the same button in the
# destination mode, so the press continues seamlessly across the switch.
def to_menus_via_pause():
    return bind("Escape", {"type": "Button", "button": "Options"}), bind("Escape", act("SetMode", mode="Menus"))


on_foot = {
    "name": "On Foot",
    "mouse": mouse(),
    "left_keys": keys(),
    "right_keys": keys(),
    "trigger_ramp_ms": 0.0,
    "bindings": wasd_left() + [
        # Walk: LAlt + direction gives half deflection (most specific chord wins over plain WASD)
        bind("W", act("LeftStickUp", 0.45), ["LAlt"]), bind("S", act("LeftStickDown", 0.45), ["LAlt"]),
        bind("A", act("LeftStickLeft", 0.45), ["LAlt"]), bind("D", act("LeftStickRight", 0.45), ["LAlt"]),
        bind("MouseLeft", act("R2")),               # fire
        bind("MouseRight", act("L2")),              # aim (ADS multiplier follows L2)
        bind("LShift", btn("Cross")),               # sprint
        bind("Space", btn("Square")),               # jump
        bind("R", btn("Circle")),                   # reload / light melee
        bind("E", btn("DpadRight")),                # contextual interact
        bind("F", btn("Triangle")),                 # enter vehicle
        bind("F", act("SetMode", mode="Driving")),  # ...and switch to driving controls
        bind("Q", btn("R1")),                       # take cover
        bind("Tab", btn("L1")),                     # weapon wheel (hold)
        bind("LCtrl", btn("L3")),                   # crouch / stealth
        bind("C", btn("R3")),                       # look behind
        bind("V", btn("Touchpad")),                 # camera view
        bind("T", btn("DpadUp")),                   # phone
        bind("Up", btn("DpadUp")),
        bind("Down", btn("DpadDown")),
        bind("Left", btn("DpadLeft")),
        bind("Right", btn("DpadRight")),
        bind("WheelUp", btn("DpadLeft")),           # previous weapon / option
        bind("WheelDown", btn("DpadRight")),        # next weapon / option
        bind("MouseMiddle", btn("R3")),
        *to_menus_via_pause(),
        bind("M", btn("Touchpad"), ["LShift"]),     # map shortcut on some layouts
    ] + mode_hotkeys(),
}

driving = {
    "name": "Driving",
    "mouse": mouse(sensitivity=0.85, ads_follows_l2=False, square_saturation=0.0),
    # Gradual analog steering: A/D sweep the stick instead of slamming it.
    "left_keys": keys(ramp_up_ms=170.0, ramp_down_ms=110.0),
    "right_keys": keys(),
    "trigger_ramp_ms": 60.0,
    "bindings": [
        bind("W", act("R2")),                        # accelerate
        bind("S", act("L2")),                        # brake / reverse
        bind("A", act("LeftStickLeft", 1.0)),        # steer
        bind("D", act("LeftStickRight", 1.0)),
        bind("A", act("LeftStickLeft", 0.5), ["LAlt"]),   # fine steering
        bind("D", act("LeftStickRight", 0.5), ["LAlt"]),
        bind("LShift", act("LeftStickUp", 1.0)),     # lean forward (bikes) / pitch
        bind("LCtrl", act("LeftStickDown", 1.0)),    # lean back
        bind("Space", btn("R1")),                    # handbrake
        bind("E", btn("L3")),                        # horn
        bind("H", btn("DpadRight")),                 # headlights / contextual
        bind("Q", btn("DpadLeft")),                  # radio / contextual
        bind("C", btn("R3")),                        # look behind
        bind("V", btn("Touchpad")),                  # camera view
        bind("R", btn("Circle")),
        bind("X", btn("Cross")),
        bind("MouseRight", btn("L1")),               # drive-by aim
        bind("MouseLeft", btn("R1"), ["LAlt"]),
        bind("F", btn("Triangle")),                  # exit vehicle
        bind("F", act("SetMode", mode="On Foot")),
        bind("T", btn("DpadUp")),
        bind("Up", btn("DpadUp")), bind("Down", btn("DpadDown")),
        bind("Left", btn("DpadLeft")), bind("Right", btn("DpadRight")),
        *to_menus_via_pause(),
    ] + mode_hotkeys(),
}

flying = {
    "name": "Flying",
    "mouse": mouse(sensitivity=0.8, ads_follows_l2=False, square_saturation=0.0),
    "left_keys": keys(ramp_up_ms=220.0, ramp_down_ms=160.0),
    "right_keys": keys(),
    "trigger_ramp_ms": 120.0,
    "bindings": [
        bind("W", act("LeftStickUp", 1.0)),          # pitch down (nose forward)
        bind("S", act("LeftStickDown", 1.0)),        # pitch up
        bind("A", act("LeftStickLeft", 1.0)),        # roll left
        bind("D", act("LeftStickRight", 1.0)),       # roll right
        bind("Q", btn("L1")),                        # yaw left
        bind("E", btn("R1")),                        # yaw right
        bind("LShift", act("R2")),                   # throttle up
        bind("LCtrl", act("L2")),                    # throttle down / brake
        bind("G", btn("L3")),                        # landing gear
        bind("Space", btn("Square")),
        bind("MouseLeft", btn("Square")),            # vehicle weapon (GTA V layout)
        bind("C", btn("R3")),
        bind("V", btn("Touchpad")),
        bind("F", btn("Triangle")),                  # exit aircraft
        bind("F", act("SetMode", mode="On Foot")),
        *to_menus_via_pause(),
    ] + mode_hotkeys(),
}

flying_mouse = {
    "name": "Flying (Mouse Steer)",
    # Mouse flies the aircraft (pitch/roll on the left stick) with its own sensitivity;
    # keyboard handles yaw and throttle.
    "mouse": mouse(target="left_stick", sensitivity=0.7, sens_y=1.0, smoothing=0.3,
                   ads_follows_l2=False, anti_deadzone=0.12, curve_exponent=0.9),
    "left_keys": keys(ramp_up_ms=220.0, ramp_down_ms=160.0),
    "right_keys": keys(),
    "trigger_ramp_ms": 120.0,
    "bindings": [
        bind("A", btn("L1")),                        # yaw left
        bind("D", btn("R1")),                        # yaw right
        bind("W", act("R2")),                        # throttle up
        bind("S", act("L2")),                        # throttle down
        bind("Up", act("RightStickUp", 1.0)),        # look around with arrows
        bind("Down", act("RightStickDown", 1.0)),
        bind("Left", act("RightStickLeft", 1.0)),
        bind("Right", act("RightStickRight", 1.0)),
        bind("G", btn("L3")),
        bind("MouseLeft", btn("Square")),
        bind("C", btn("R3")),
        bind("V", btn("Touchpad")),
        bind("F", btn("Triangle")),
        bind("F", act("SetMode", mode="On Foot")),
        *to_menus_via_pause(),
    ] + mode_hotkeys(),
}

menus = {
    "name": "Menus",
    # GTA's map cursor is driven by the left stick, so the mouse moves it.
    "mouse": mouse(target="left_stick", sensitivity=0.6, sens_y=1.0, smoothing=0.25,
                   ads_follows_l2=False, anti_deadzone=0.15, curve_exponent=1.0, square_saturation=0.0),
    "left_keys": keys(),
    "right_keys": keys(),
    "trigger_ramp_ms": 0.0,
    "bindings": [
        bind("W", btn("DpadUp")), bind("S", btn("DpadDown")),
        bind("A", btn("DpadLeft")), bind("D", btn("DpadRight")),
        bind("Up", btn("DpadUp")), bind("Down", btn("DpadDown")),
        bind("Left", btn("DpadLeft")), bind("Right", btn("DpadRight")),
        bind("Enter", btn("Cross")), bind("Space", btn("Cross")), bind("MouseLeft", btn("Cross")),
        bind("Backspace", btn("Circle")), bind("MouseRight", btn("Circle")),
        bind("X", btn("Square")), bind("Delete", btn("Square")),
        bind("Y", btn("Triangle")),
        bind("Num1", btn("Cross")), bind("Num2", btn("Circle")),
        bind("Num3", btn("Square")), bind("Num4", btn("Triangle")),
        bind("Q", btn("L1")), bind("E", btn("R1")),        # tabs
        bind("Z", act("L2")), bind("C", act("R2")),
        bind("WheelUp", act("R2")), bind("WheelDown", act("L2")),  # map zoom
        bind("PageUp", btn("L1")), bind("PageDown", btn("R1")),
        bind("Tab", btn("R1")), bind("Tab", btn("L1"), ["LShift"]),
        bind("Escape", btn("Options")),                    # resume game...
        bind("Escape", act("SetMode", mode="On Foot")),    # ...and go back to on-foot controls
        bind("V", btn("Touchpad")),
    ] + mode_hotkeys(),
}

gta6 = {
    "format": "remoteshift-profile",
    "version": 1,
    "name": "GTA 6",
    "game": "Grand Theft Auto VI",
    "description": "Starter profile for GTA 6. Bindings follow GTA V's default PS5 layout because GTA 6's "
                   "controller scheme is unverified; adjust in the Keybind Editor. F1-F5 select modes, "
                   "` cycles them, F enters/exits vehicles and switches between On Foot and Driving, "
                   "Esc pauses and switches to Menus.",
    "default_mode": "On Foot",
    "wheel_pulse_ms": 40.0,
    "wheel_gap_ms": 30.0,
    "modes": [on_foot, driving, flying, flying_mouse, menus],
}

generic = {
    "format": "remoteshift-profile",
    "version": 1,
    "name": "Default Third-Person",
    "game": "",
    "description": "General-purpose third-person shooter layout. A good base for new per-game profiles.",
    "default_mode": "Gameplay",
    "wheel_pulse_ms": 40.0,
    "wheel_gap_ms": 30.0,
    "modes": [{
        "name": "Gameplay",
        "mouse": mouse(),
        "left_keys": keys(),
        "right_keys": keys(),
        "trigger_ramp_ms": 0.0,
        "bindings": wasd_left() + [
            bind("MouseLeft", act("R2")), bind("MouseRight", act("L2")),
            bind("Space", btn("Cross")), bind("LCtrl", btn("Circle")), bind("C", btn("Circle")),
            bind("R", btn("Square")), bind("E", btn("Triangle")), bind("F", btn("Triangle")),
            bind("Q", btn("L1")), bind("G", btn("R1")), bind("LShift", btn("L3")),
            bind("V", btn("R3")), bind("MouseMiddle", btn("R3")),
            bind("Up", btn("DpadUp")), bind("Down", btn("DpadDown")),
            bind("Left", btn("DpadLeft")), bind("Right", btn("DpadRight")),
            bind("WheelUp", btn("DpadUp")), bind("WheelDown", btn("DpadDown")),
            bind("Num1", btn("DpadLeft")), bind("Num2", btn("DpadRight")),
            bind("Escape", btn("Options")), bind("Tab", btn("Touchpad")), bind("M", btn("Touchpad")),
            bind("MouseX1", act("SensitivityScale", 0.5)),   # precision aim while held
            bind("Kp0", btn("Create")),
        ],
    }],
}

for fname, prof in (("gta6.json", gta6), ("default.json", generic)):
    with open(os.path.join(HERE, fname), "w", newline="\n") as f:
        json.dump(prof, f, indent=2)
        f.write("\n")
    print("wrote", fname)
