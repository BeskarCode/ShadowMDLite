# Shadow MD with Joy-Con support

*Start at [../README.md](../README.md) if you have not read it. This file covers only what changed inside Eebel's sketch.*

This is Eebel's Shadow MD, unchanged in behaviour, with one structural addition: the sketch no longer talks to the PS3 controller directly. It talks to a small controller interface, and behind that interface there is either a PS3 Move Navigation, exactly as before, or a single Nintendo Switch Joy-Con.

**Why.** Sony stopped making the Nav years ago. The buttons work, the sticks drift, and replacements get more expensive and harder to find every year. A single Joy-Con is the closest thing still in production: one hand, pocket size, analog stick, USB-C charging, and it speaks the same classic Bluetooth HID that the Navs use, so no new hardware is needed. Same Mega ADK, same Bluetooth dongle.

## How to switch controller

Edit `ControllerConfig.h`. That is the only file you need to touch.

```c
#define USE_PS3_NAV        // original behaviour, two PS3 Navs
//#define USE_JOYCON       // two Joy-Cons, left for the feet, right for the dome
```

For the first run with new Joy-Cons, set `JOYCON_PAIRING_MODE` to `true`, upload, then hold the small sync button on the Joy-Con rail until the lights run. Once it connects, set it back to `false` and upload again. From then on it reconnects by itself.

## What actually changed in the sketch

41 lines added, 5 removed, across three files. That is the whole port.

| File | Change |
|---|---|
| `a_INIT.ino` | The two `PS3BT` pointers become two `DroidPad` pointers, built from whichever controller `ControllerConfig.h` selects. Plus the includes. |
| `b_SETUP.ino` | Two `begin()` calls. |
| `d_FUNCTIONS.ino` | Three function signatures change from `PS3BT*` to `DroidPad*`, and `readUSB()` gains two `update()` calls right after `Usb.Task()`. |

Everything else is untouched. All 161 calls to `getButtonPress()`, `getAnalogHat()`, `PS3NavigationConnected` and the rest keep working exactly as they are, because `DroidPad` answers the same API.

That was the design goal. A port that rewrites every call site produces a diff nobody can review and a droid nobody trusts at an event.

## New files

| File | What it is |
|---|---|
| `ControllerConfig.h` | The only file you edit. Controller choice and Joy-Con options. |
| `DroidPad.h` | The compatibility shim: speaks the old PS3BT API, answers from the abstraction. |
| `DroidController.h` | The controller interface: button roles, normalised stick, click detection, watchdog. |
| `PS3NavController.h` | The PS3 Nav implementation. The reference: it must behave as before. |
| `JoyConController.h` | The Joy-Con implementation, built on `SwitchProBT`. |

## Requirements

**USB Host Shield Library 2.0: the version depends on the controller.** For Joy-Cons you need **1.7.0 or newer**, because older releases do not ship `SwitchProBT`. For PS3 Navs, **keep the older library**: the Navs do not work with 1.7.0. Keep both copies around and install the one that matches the controller you build for. A PS3 Nav build does not include any Joy-Con code, so it compiles fine against the older library.

## Button map

A Joy-Con has nine usable buttons, the same number as a Nav, but not the same nine, so one role has to give.

| Foot, left Joy-Con | Dome, right Joy-Con | Was on the Nav |
|---|---|---|
| d-pad | X / B / Y / A | d-pad |
| Minus | Plus | CROSS |
| Capture | Home | CIRCLE |
| L | R | L1 |
| ZL | ZR | L2 |
| stick click | stick click | PS |
| nothing | nothing | L3 |

L3 is the one that goes, because in the stock map almost nothing uses it: every cross controller L3 combination is FREE. The single function that moves is the over speed toggle, which was `L3 + L1` and is now `stick click + ZL`. Sacrificing CIRCLE instead would have cost eight working functions.

`SL` and `SR` on the rail are not exposed by the Switch Pro parser, and they are awkward to reach while gripping the controller anyway.

## Build sizes, Arduino Mega ADK

| Build | Flash | RAM |
|---|---|---|
| Original, untouched, older library | 56,548 | 4,738 (57%) |
| This fork, PS3 Nav, older library | 55,812 | 5,318 (64%) |
| This fork, Joy-Con, library 1.7.0 | 55,452 | 5,391 (65%) |

Each build is measured with the library it actually needs (see Requirements). The PS3 Nav build comes out slightly smaller in flash than the original. RAM goes up by about 600 bytes, which on a Mega leaves roughly 2.8 KB free. That is enough, but it is worth knowing if you have already added things of your own to this sketch.

## What is tested and what is not

- Both configurations compile clean. **Neither has been run on a droid yet.**
- The Joy-Con implementation handles stick auto centring on connection, the different button map of each half, and grip orientation.
- There is a bench test sketch, `DroidControllerLayer`, that reads controllers and prints what it sees without driving anything. Run that first, with the droid powered down.

Do not take this to an event until you have driven it in your workshop.

## Credit

Original Shadow MD by Eebel, from https://github.com/Eebel/SHADOW_MD_EEBEL, built on KnightShade's SHADOW. The controller abstraction and the Joy-Con support come from the work on ShadowMD Lite, https://github.com/Fernando-Beltran/ShadowMDLite
