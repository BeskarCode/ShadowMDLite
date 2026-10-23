# Driving a droid with a Nintendo Switch Joy-Con

Everything here is about replacing the PS3 Move Navigation controller, which Sony stopped making years ago, with a single Joy-Con: one hand, pocket size, analog stick, USB-C, still sold everywhere.

**No new hardware is needed.** A Joy-Con speaks the same classic Bluetooth HID as the Navs, so the same Arduino Mega ADK and the same Bluetooth dongle work.

## Status

**The bench test passes with two real Joy-Cons connected at once**: both sticks reach full scale, all nine buttons on each half read correctly, and the link lag stays under 25 ms. Pairing without re-uploading, reconnecting by button and the Switch check are bench-tested too (see below). **Nothing has been driven on a droid yet.** Run the bench test first, with the droid powered down, and do not take any of this to an event until you have driven it in your workshop.

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

## Pairing

No re-uploading and no editing. The droid pairs by itself the first time, and you can ask it to pair again whenever you want.

**The first time.** Upload the sketch with `USE_JOYCON` selected and power up. With no Joy-Con recorded, the droid opens a pairing session on its own. Hold the small sync button on the rail of the **left** Joy-Con until it connects and vibrates, then do the same with the **right** one. That is all. The second Joy-Con is optional: if you only use one, let the session end by itself after a minute.

**Every day.** Power up the droid, wait a few seconds, then press any button on each Joy-Con. They call the droid back by themselves. After a Joy-Con disconnects, for instance when you turn it off or the droid restarts, wait about ten seconds before pressing: a Joy-Con that is still shutting down ignores the button.

**Pairing again**, a new Joy-Con or one that stopped connecting. Any of these opens a session:

- Hold **Minus + Capture** on the left Joy-Con, or **Plus + Home** on the right one, for **5 seconds**. It vibrates once per second while you hold, then longer when the session starts. Let go before that and it starts counting again.
- Reset the Mega **three times in a row**, a few seconds apart.
- Press **Pair Joy-Con** in the web console, or send **`JOYPAIR`** over serial (ShadowMD Lite only).

While a session is open the droid does not move, the Joy-Cons already connected flash all four LEDs, and a Joy-Con that is already paired cannot reconnect by button: hold its sync button instead. A session closes as soon as both halves are connected, or after 60 seconds.

**What the droid tells you.** The sketches play an R2 sound at each step (change them in `ControllerConfig.h`) and print a line on serial:

| When | Serial | Default sound |
|---|---|---|
| Session open, hold sync on a Joy-Con | `JOYPAIR started` | `$51` |
| One paired, now the other | `JOYPAIR paired one, now the other` | `$31` |
| Both paired, session closed | `JOYPAIR done` | `$32` |
| A minute went by without a new Joy-Con | `JOYPAIR timed out` | `$41` |
| That Joy-Con still remembers a Switch: hold sync again | `JOYPAIR hold sync again on that Joy-Con` | `$21` |

When a Joy-Con connects, its player LEDs alternate between the outer pair and the inner pair for two seconds, then settle on LED 1 for the left half and LED 2 for the right. If you do not see that, it did not connect.

**A Joy-Con that has been used on a Switch.** A Joy-Con keeps two pairing records in its flash. A new pairing goes into the second one and the old second one moves to the first. If either of the two points to a Switch, pressing a button no longer reconnects to the droid. The droid checks both records when a Joy-Con pairs: if one still points elsewhere it asks you, with the sound above, to hold sync again. The second time both records point to the droid and it is accepted.

**With the bench test** the same session runs, and the serial monitor at 115200 takes three commands: `p` opens a session, `r` prints the two pairing records of each connected Joy-Con (`0x2000` and `0x2026`, which should hold your dongle's address), and `f` forgets the paired Joy-Cons so the next boot pairs from scratch.

Plug the Bluetooth dongle in before powering up the controllers. The library reads the dongle's address first.

## Joy-Con details worth knowing

**It boots in the wrong mode.** A Joy-Con starts out sending report `0x3F`, "simple HID mode", where the stick is only an 8 direction hat, not an analog axis. Useless for driving. In simple mode it only sends a report when a button changes, and the library only sends its own commands right after it receives a report, so a Joy-Con nobody touches never gets asked to switch. `JoyConController` therefore asks for the full `0x30` report itself, every 250 ms, until full reports start arriving.

**The centre is not where you think.** Factory stick calibration lives in the controller's own SPI flash and the library does not expose it, so `JoyConController` measures the centre itself on every connection, by averaging the first 30 full reports, about half a second. Leave it alone while it does that.

**It does not come back to the same centre.** Joy-Con sticks are known for drift: let go after a full push and the stick often rests somewhere else. On the test unit it settled about 15% off centre, enough to make the feet creep. So the centre keeps adjusting: when the stick sits within 25% of the centre and does not move at all for 1.5 seconds, that spot becomes the new centre. A thumb on the stick is never that still, so it does not fight you while you drive.

**Which half is which.** Each half reports the other half's stick as all zeros, so `JoyConController` works out on connection whether it got a left or a right Joy-Con, and uses that side's stick and button map whatever slot it was created for. `routeBySlot()` in `DroidController.h` then swaps the foot and dome slots when needed, and `DroidPad::updatePair()` calls it every loop. A PS3 Nav does not know which one it is, so with Navs nothing changes.

**It calls the droid in short bursts.** A sleeping Joy-Con woken by a button pages its host only briefly, because a Switch listens for it almost all the time. The dongle's default listens 11.25 ms every 1.28 s, so those calls almost never got through and pressing a button did nothing, while a laptop, which keeps paging for seconds, connected at once. So while a Joy-Con is missing, the droid has the dongle listen all the time, and goes back to the default once both are connected: listening all the time roughly doubles the report lag. This is in `JoyConPairing.h`, `tunePageScan()`.

**The library does not notice when a Joy-Con goes away.** When you turn a Joy-Con off, the Bluetooth library sees the link drop but does not tell the object that was handling it, which stays "connected" and then refuses the Joy-Con when it calls back. A Joy-Con in full mode reports about 60 times a second, so `JoyConController` treats three seconds of silence as gone and frees that object.

**The Switch sends `08 00` after every connection**, and so do these sketches. It takes the Joy-Con out of its "shipment" low power mode, in which a button press does not wake it up to call its host.

**Full scale is smaller than the spec says.** The axes are 12 bit, but a real Joy-Con stick reaches about ±1100 from centre, not ±2048. The default range is 1000, so a full push reads full scale.

**Safety: the stick only drives once it has been seen centred.** A worn Joy-Con can rest far off centre (one of ours rested 40% off), which would drive the droid the moment it pairs. So after every connection the stick reads zero until it sits in the centre, and the serial console says why: `JOY-L: stick rests far from centre (held, or drift). Stick off until it centres.` Move the stick and let go; when it centres you get `stick centred, driving again`. A stick that freezes off centre for 2 seconds is switched off the same way. Buttons keep working. What no software can do is tell a stick that rests at 25% and keeps wobbling from a thumb holding it there: **if your stick does not come back to zero in the bench test, replace it** (a spare stick costs a few euros) before driving with it.

## Credit

The Joy-Con protocol details, the subcommands and the pairing records come from **dekuNukem's** reverse engineering notes, https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering. The note on subcommand `08` is what put us on the trail of the reconnection problem. Thank you.

The Shadow MD fork (https://github.com/Fernando-Beltran/SHADOW_MD_EEBEL) is Eebel's Shadow MD, from https://github.com/Eebel/SHADOW_MD_EEBEL, itself built on KnightShade's SHADOW. None of that behaviour has been changed.
