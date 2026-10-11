# Droid Controller Layer

A transport independent controller interface for ShadowMD Lite, kept as a **separate sketch folder**. Nothing in here is included by `ShadowMDLite.ino`, and `ShadowMDLite.ino` is not modified in any way. You can ignore this folder entirely and your droid behaves exactly as it does today.

## Why

`ShadowMDLite.ino` talks to the PS3 Move Navigation controllers directly, all over the file: `PS3NavFoot->getAnalogHat(LeftHatX)`, `PS3NavDome->getButtonPress(CROSS)`, `PS3NavFoot->PS3NavigationConnected`, and so on. That is fine as long as the Nav is the only controller anyone uses. The moment you want to support something else, every one of those calls is a place to change.

The Navs are long out of production. Supporting a second controller should not mean forking the sketch.

This layer puts one thin interface in between. The droid logic asks "is the stick pushed forward" and "is the UP role button held", and a small implementation per controller answers. Adding a Joy-Con then means writing one new class, not touching the droid logic at all.

## Files

| File | What it is |
|---|---|
| `DroidController.h` | The interface. Button roles, normalised stick, click and release edge detection, watchdog timing, dead zone helpers. |
| `PS3NavController.h` | The reference implementation, wrapping `PS3BT`. Must behave exactly like the current sketch. |
| `JoyConController.h` | Working implementation for a single Nintendo Switch Joy-Con, left or right, built on `SwitchProBT`. Auto centring, configurable role mapping, grip orientation, battery level and rumble. |
| `DroidControllerLayer.ino` | Test bench. Reads your real Navs through the interface and prints what it sees. Drives nothing, so it cannot move the droid. |

## Design decisions worth knowing

**Button roles, not button labels.** A PS3 Nav has CROSS and CIRCLE. A Joy-Con has A, B, X, Y, or four direction buttons depending on which half you hold. The interface defines ten roles (`DB_UP`, `DB_A`, `DB_L1`, `DB_SYS` and so on) and each implementation decides which physical button fills each one.

**The stick is normalised to +/- 512, centred on 0.** The Nav gives 8 bits, a Joy-Con gives 12. Normalising upwards rather than down means no resolution is thrown away on controllers that have it. Forward is negative on the Y axis, which matches the Nav's raw convention, so nothing inverts when porting.

**`legacyX()` and `legacyY()` return the old 0..255 scale.** This is deliberate scaffolding. The existing drive code in `ShadowMDLite.ino` is full of `map(..., 54, 200, ...)` calls tuned to that scale. Those functions can be moved onto this layer one at a time, still reading 0..255, and converted to the normalised scale later if it is ever worth it. A port that has to rewrite all the drive math in one go is a port nobody dares to merge.

**Edge detection lives in the layer.** `clicked()` and `released()` compare against the previous `update()`, rather than relying on each library's own click semantics. Same behaviour on every controller.

**The watchdog contract is non negotiable.** `millisSinceLastMessage()` must keep growing when the link is lost, on every implementation. ShadowMD Lite already stops the motors when that lag grows, and that is what keeps a droid from running off when a controller dies.

## How to test it

1. Open `DroidControllerLayer.ino` in the Arduino IDE 2.x, board Arduino Mega ADK.
2. Upload it with the same USB Host Shield and Bluetooth dongle you use for the droid.
3. Pair your Navs as usual, open the Serial Monitor at 115200.
4. Check the list at the top of the .ino: sticks at rest read 0 and legacy 128, each button lights the right role, a click prints once, and walking away makes the lag counter climb.

If all of that matches what your droid does today, the layer is faithful.

## Requirements

- **USB Host Shield Library 2.0, version 1.7.0 or newer.** Older releases do not ship `SwitchProBT`, which the Joy-Con support is built on. The PS3 Nav side works on older versions.
- Arduino Mega ADK, the same Bluetooth dongle used for the Navs.

## Joy-Con notes

A Joy-Con speaks the same Bluetooth HID protocol as the Switch Pro controller, so the library's parser does the heavy lifting, including the one real trap: a Joy-Con boots sending report `0x3F`, "simple HID mode", where the stick is only an 8 direction hat. The parser sees that and immediately asks for the full `0x30` report with real 12 bit analog axes.

What this class adds on top:

- **Auto centring.** Factory stick calibration lives in the controller's SPI flash at `0x603D` and the library does not expose it, so instead the centre is measured on every connection by averaging the stick for about half a second. Leave it alone while it does that. It also soaks up the drift these sticks are known for. Call `recalibrate()` to redo it.
- **Configurable role mapping.** A single Joy-Con exposes nine usable buttons, the same count as a Nav but not the same nine. `SL` and `SR` are not exposed by the Switch Pro parser. That leaves one of the ten droid roles unmapped: by default `DB_B`. If your button map needs `DB_B` more than `DB_SYS`, swap them in `JoyConMapping`, which is the only place that decides.
- **Grip orientation.** `JOYCON_UPRIGHT` is the Nav-like grip and the default. `JOYCON_SIDEWAYS` rotates the stick axes 90 degrees for the mini controller grip.
- **Things a Nav cannot do:** `batteryLevel()` and `buzz()`. A short rumble when voice mode engages is real feedback, where a Nav leaves you guessing.

## What comes next

1. Bench test with real hardware: Navs and Joy-Cons, through the same interface. **This is where it is now.**
2. Port the drive functions in a copy of the main sketch, one at a time, using `legacyX()` / `legacyY()` so the math is untouched.
3. Port the button dispatcher, replacing `getButtonPress(CROSS)` with `pressed(DB_A)`.

Steps 2 and 3 happen in a copy, not in the working sketch, until they are proven on a real droid.
