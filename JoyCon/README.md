# Driving a droid with a Nintendo Switch Joy-Con

Everything here is about replacing the PS3 Move Navigation controller, which Sony stopped making years ago, with a single Joy-Con: one hand, pocket size, analog stick, USB-C, still sold everywhere.

**No new hardware is needed.** A Joy-Con speaks the same classic Bluetooth HID as the Navs, so the same Arduino Mega ADK and the same Bluetooth dongle work.

## Status

Everything compiles. **Nothing has been tested on a droid yet.** Run the bench test first, with the droid powered down, and do not take any of this to an event until you have driven it in your workshop.

## Which folder do I open?

| I want to... | Open |
|---|---|
| Check my Joy-Cons connect and read correctly, without touching my droid | **`BenchTest/`** |
| Run Eebel's Shadow MD with Joy-Cons instead of Navs | **`ShadowMD_JoyCon/`** |
| Print the button map and keep it on the bench | **`CheatSheet_JoyCon.html`** |

Start with `BenchTest`. It drives nothing: no Sabertooth, no SyRen, no MarcDuino. It only reads the controllers and prints what it sees over serial. If the numbers there look right, the rest will behave.

## Requirements

**USB Host Shield Library 2.0, version 1.7.0 or newer.** Older releases do not ship `SwitchProBT`, which the Joy-Con support is built on. If you stay on PS3 Navs, older versions still work.

Watch out for one thing in that library: the Switch shoulder buttons are `L`, `ZL`, `R` and `ZR`. The `L1` and `R1` enums land in empty slots of the Switch button table and read the wrong bit.

## How it works

Shadow talks to the PS3 controller directly, in 161 places. While the Nav is the only controller that is fine, but the moment you want a second one, every one of those lines is a place to change.

So there is a thin interface in between. The droid logic asks "is the stick pushed forward" and "is the button with the UP role held", and one small class per controller answers:

| File | What it is |
|---|---|
| `DroidController.h` | The interface. Button roles, normalised stick, click and release edges, watchdog timing. |
| `PS3NavController.h` | The PS3 Nav implementation. The reference: it must behave exactly as before. |
| `JoyConController.h` | A single Joy-Con, left or right, built on `SwitchProBT`. |
| `DroidPad.h` | Only in the Shadow MD fork. A shim that speaks the old PS3BT API on the outside and answers from the interface on the inside, so the 161 call sites stay untouched. |

Adding another controller means writing one more implementation, and nothing else.

**About the duplicated headers.** The same `.h` files appear inside `BenchTest/` and inside `ShadowMD_JoyCon/Shadow_MD_Eebel_Tabbed/`. That is not an oversight: the Arduino IDE compiles the files that sit next to the sketch, so each sketch needs its own copy. They are identical. If you change one, change the other.

## Button map

A Joy-Con has nine usable buttons, the same count as a Nav, but not the same nine, so one role has to give.

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

## Pairing a Joy-Con

1. Set the pairing flag to `true`: `PAIR_NEW_DEVICE` in `BenchTest.ino`, or `JOYCON_PAIRING_MODE` in `ControllerConfig.h`.
2. Upload, then hold the small sync button on the Joy-Con rail until its lights run back and forth.
3. Once it connects, set the flag back to `false` and upload again. From then on it reconnects by itself.

Plug the Bluetooth dongle in before powering up the controllers. The library reads the dongle's address first.

## Two Joy-Con details worth knowing

**It boots in the wrong mode.** A Joy-Con starts out sending report `0x3F`, "simple HID mode", where the stick is only an 8 direction hat, not an analog axis. Useless for driving. The library's parser spots that report and immediately asks for the full `0x30` report with real 12 bit axes.

**The centre is not where you think.** Factory stick calibration lives in the controller's own SPI flash and the library does not expose it, so `JoyConController` measures the centre itself on every connection, by averaging the stick for about half a second. Leave it alone while it does that. It also soaks up the drift these sticks are known for.

## Credit

The Shadow MD fork is Eebel's Shadow MD, from https://github.com/Eebel/SHADOW_MD_EEBEL, itself built on KnightShade's SHADOW. None of that behaviour has been changed.
