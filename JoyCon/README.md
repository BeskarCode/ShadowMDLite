# Driving a droid with a Nintendo Switch Joy-Con

Everything here is about replacing the PS3 Move Navigation controller, which Sony stopped making years ago, with a single Joy-Con: one hand, pocket size, analog stick, USB-C, still sold everywhere.

**No new hardware is needed.** A Joy-Con speaks the same classic Bluetooth HID as the Navs, so the same Arduino Mega ADK and the same Bluetooth dongle work.

## Status

**The bench test passes with two real Joy-Cons connected at once**: both sticks reach full scale, all nine buttons on each half read correctly, and the link lag stays under 25 ms. **Nothing has been driven on a droid yet.** Run the bench test first, with the droid powered down, and do not take any of this to an event until you have driven it in your workshop.

**The order does not matter.** Whichever Joy-Con wakes up first, the left half always drives the foot and the right half the dome. If only the right one is connected, it goes to the dome and the foot stays still. Tested on the bench with the right half connecting first.

## Which folder do I open?

| I want to... | Open |
|---|---|
| Check my Joy-Cons connect and read correctly, without touching my droid | **`BenchTest/`** |
| Run **ShadowMD Lite** with Joy-Cons | the main sketch at the root of this repo, **`ShadowMDLite.ino`**. Pick the controller in its `ControllerConfig.h` |
| Run Eebel's Shadow MD with Joy-Cons instead of Navs | its own repo, **[https://github.com/Fernando-Beltran/SHADOW_MD_EEBEL](https://github.com/Fernando-Beltran/SHADOW_MD_EEBEL)**. See `JOYCON.md` there |
| Print the button map and keep it on the bench | **`CheatSheet_JoyCon.html`** |

Start with `BenchTest`. It drives nothing: no Sabertooth, no SyRen, no MarcDuino. It only reads the controllers and prints what it sees over serial. If the numbers there look right, the rest will behave.

## Requirements

**USB Host Shield Library 2.0: the version depends on the controller.** For Joy-Cons you need **1.7.0 or newer**, because older releases do not ship `SwitchProBT`. For PS3 Navs, **keep the older library**: the Navs do not work with 1.7.0. Keep both copies around and install the one that matches the controller you build for. A PS3 Nav build does not include any Joy-Con code, so it compiles fine against the older library.

Watch out for one thing in that library: the Switch shoulder buttons are `L`, `ZL`, `R` and `ZR`. The `L1` and `R1` enums land in empty slots of the Switch button table and read the wrong bit.

## How it works

Shadow talks to the PS3 controller directly, in 161 places. While the Nav is the only controller that is fine, but the moment you want a second one, every one of those lines is a place to change.

So there is a thin interface in between. The droid logic asks "is the stick pushed forward" and "is the button with the UP role held", and one small class per controller answers:

| File | What it is |
|---|---|
| `DroidController.h` | The interface. Button roles, normalised stick, click and release edges, watchdog timing. |
| `PS3NavController.h` | The PS3 Nav implementation. The reference: it must behave exactly as before. |
| `JoyConController.h` | A single Joy-Con, left or right, built on `SwitchProBT`. |
| `DroidPad.h` | Used by ShadowMD Lite and by the Shadow MD fork. A shim that speaks the old PS3BT API on the outside and answers from the interface on the inside, so the call sites stay untouched. It only includes the Joy-Con code when `USE_JOYCON` is selected, so a PS3 Nav build still compiles against the older library. |

Adding another controller means writing one more implementation, and nothing else.

**About the duplicated headers.** The same `.h` files appear at the root of this repo (next to `ShadowMDLite.ino`), inside `BenchTest/`, and in the Shadow MD fork (https://github.com/Fernando-Beltran/SHADOW_MD_EEBEL). That is not an oversight: the Arduino IDE compiles the files that sit next to the sketch, so each sketch needs its own copy. They are identical. If you change one, change the other.

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

In ShadowMD Lite there is one more thing to know: of the cross controller `dome arrow + foot L3` combos, only **Up + L3** is assigned in the stock map, to function 90 (random sounds, `$R`). With Joy-Cons it cannot be reached, so move it to a free combo if you use it.

`SL` and `SR` on the rail are not exposed by the Switch Pro parser, and they are awkward to reach while gripping the controller anyway.

## Pairing a Joy-Con

**Pair one Joy-Con at a time.** The library can only pair while no other controller is connected; once one is in, it stops looking. Reconnecting afterwards works fine with both at once.

With the bench test, no re-flashing is needed: open the serial monitor at 115200, send `p`, and hold the small sync button on the Joy-Con rail until its lights run back and forth. Then reset the Mega and do the other half the same way.

With ShadowMD Lite or the Shadow MD fork:

1. Set `JOYCON_PAIRING_MODE` to `true` in `ControllerConfig.h`.
2. Upload, then hold the sync button on one Joy-Con until it connects.
3. Reset the Mega and pair the other one.
4. Set the flag back to `false` and upload again. From then on they reconnect by themselves.

**Reconnecting.** Press any button (minus or plus is a good choice) and the Joy-Con calls the dongle back. After the link drops, for instance when the Mega resets, wait about ten seconds before pressing: a Joy-Con that is still shutting down ignores the button.

When a Joy-Con connects, its player LEDs alternate between the outer pair and the inner pair for two seconds, then settle on LED 1 for the left half and LED 2 for the right. If you do not see that, it did not connect.

The bench test also answers `r`: it reads the pairing record each connected Joy-Con keeps in its flash (`0x2000` and `0x2026`) and prints the host address stored there, which should be your dongle's.

Plug the Bluetooth dongle in before powering up the controllers. The library reads the dongle's address first.

## Joy-Con details worth knowing

**It boots in the wrong mode.** A Joy-Con starts out sending report `0x3F`, "simple HID mode", where the stick is only an 8 direction hat, not an analog axis. Useless for driving. In simple mode it only sends a report when a button changes, and the library only sends its own commands right after it receives a report, so a Joy-Con nobody touches never gets asked to switch. `JoyConController` therefore asks for the full `0x30` report itself, every 250 ms, until full reports start arriving.

**The centre is not where you think.** Factory stick calibration lives in the controller's own SPI flash and the library does not expose it, so `JoyConController` measures the centre itself on every connection, by averaging the first 30 full reports, about half a second. Leave it alone while it does that.

**It does not come back to the same centre.** Joy-Con sticks are known for drift: let go after a full push and the stick often rests somewhere else. On the test unit it settled about 15% off centre, enough to make the feet creep. So the centre keeps adjusting: when the stick sits within 25% of the centre and does not move at all for 1.5 seconds, that spot becomes the new centre. A thumb on the stick is never that still, so it does not fight you while you drive.

**Which half is which.** Each half reports the other half's stick as all zeros, so `JoyConController` works out on connection whether it got a left or a right Joy-Con, and uses that side's stick and button map whatever slot it was created for. `routeBySlot()` in `DroidController.h` then swaps the foot and dome slots when needed, and `DroidPad::updatePair()` calls it every loop. A PS3 Nav does not know which one it is, so with Navs nothing changes.

**Using a Joy-Con on a Switch breaks the link to the droid.** A Joy-Con keeps two pairing records in its flash. A new pairing goes into the second one and the old second one moves to the first. If one of the two records points to a Switch, pressing a button no longer reconnects to the droid. Pairing it with the droid **twice** puts the dongle in both records and fixes it.

**Full scale is smaller than the spec says.** The axes are 12 bit, but a real Joy-Con stick reaches about ±1100 from centre, not ±2048. The default range is 1000, so a full push reads full scale.

## Credit

The Shadow MD fork (https://github.com/Fernando-Beltran/SHADOW_MD_EEBEL) is Eebel's Shadow MD, from https://github.com/Eebel/SHADOW_MD_EEBEL, itself built on KnightShade's SHADOW. None of that behaviour has been changed.
