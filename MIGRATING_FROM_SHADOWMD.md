# Migrating from Shadow MD to ShadowMDLite

> ROS2D2 also works with the classic sketch ([SHADOW_MD_EEBEL](https://github.com/BeskarCode/SHADOW_MD_EEBEL)); migrate only if you want ShadowMD Lite itself.

A step-by-step guide to move a droid from the classic **Shadow MD** sketch
(`Shadow_MD_DualController_Template` and its variants, such as the Eebel tabbed one) to
**ShadowMDLite**. You need ShadowMDLite to add [ROS2D2](https://github.com/BeskarCode/ROS2D2)
(the optional ROS 2 brain on a Raspberry Pi); you may also want it just for the cleaner button
mapping, the PC serial protocol and the web serial console.

The drive and dome code is the same as in Shadow MD: your droid should drive exactly as before.
What changes is how buttons are configured.

## Before you start: is ShadowMDLite for you?

ShadowMDLite expects:

- an **Arduino Mega ADK** with the USB Host Shield setup of Shadow MD,
- **PS3 Move Navigation** controllers or **Nintendo Switch Joy-Cons** (foot and, optionally, dome),
  chosen with one line in [`ControllerConfig.h`](ControllerConfig.h) (`USE_PS3_NAV` or
  `USE_JOYCON`; see the [README](README.md), section 11),
- a **Sabertooth** for the feet and a **SyRen** for the dome, on `Serial2` (packetized serial),
- **MarcDuino** boards on `Serial1` (dome) and `Serial3` (body), running **BetterDuino** firmware
  for the extended commands (see the [README](README.md), section 3).

The one thing that does not carry over: **custom buttons** (`type = 2`). ShadowMDLite only maps
buttons to standard MarcDuino function codes (1–89). If any of your buttons uses `type = 2`
(a custom MP3, logic display text, a single panel, the `DP` panel timers), you will choose a
MarcDuino code for it instead, or leave it free.

## Step 1: back up what you have

1. Copy your current Shadow MD sketch folder somewhere safe. Going back is just flashing it again.
2. Open it and write down (or keep the file open next to the new one):
   - the controller MAC addresses,
   - the speed, dead-zone and direction settings,
   - the Sabertooth and SyRen addresses and baud rates,
   - which MarcDuino code each button plays, and which buttons are `type = 2`.

## Step 2: get ShadowMDLite and its libraries

1. Download or clone this repository:
   `git clone https://github.com/BeskarCode/ShadowMDLite.git`
2. Install the libraries bundled in `SHADOW_MD_Arduino_Libraries/` (USB Host Shield, Sabertooth,
   SyRenSimplified) in your Arduino `libraries` folder. For PS3 Navs keep the **bundled, older
   USB Host Shield library**: the Navs do not work with 1.7.0. For Joy-Cons use **1.7.0 or newer**
   (README, section 6.2).
3. Open `ShadowMDLite/ShadowMDLite.ino` in the Arduino IDE and pick your controllers in
   `ControllerConfig.h`. Coming from Shadow MD with PS3 Navs, leave `USE_PS3_NAV` as it is.

## Step 3: copy your settings

The settings are at the top of `ShadowMDLite.ino`. **The file ships with the author's own values,
including his controller MAC addresses: replace them all with yours.**

Most names are the same as in Shadow MD:

| Setting | Name in both sketches |
|---|---|
| Controller MAC addresses | `PS3ControllerFootMac`, `PS3ControllerDomeMAC`, `PS3ControllerBackupFootMac`, `PS3ControllerBackupDomeMAC` |
| Speeds | `drivespeed1`, `drivespeed2`, `turnspeed`, `domespeed`, `ramping` |
| Dead zones | `joystickFootDeadZoneRange`, `joystickDomeDeadZoneRange`, `driveDeadBandRange` |
| Turn direction | `invertTurnDirection` |
| Dome automation | `domeAutoSpeed`, `time360DomeTurn` |
| Custom songs | `CurrentSongNum`, `CustomSongMax` |
| Motor controllers | `SABERTOOTH_ADDR`, `SYREN_ADDR`, `motorControllerBaudRate` |
| MarcDuinos | `marcDuinoBaudRate` |

ShadowMDLite adds two settings that Shadow MD does not have: `invertDriveDirection` and
`invertDomeDirection`. Leave them as they are for now; you will check them in step 6.

## Step 4: map your buttons

In Shadow MD each button has a block of settings (`btnUP_type`, `btnUP_MD_func`,
`btnUP_cust_MP3_num`, ...). In ShadowMDLite each button is a single line with its MarcDuino code:

```cpp
int footBtnUp_MD_func = 2; // 2 = Scream - all panels open
```

For every button that was `type = 1` in Shadow MD, copy its `_MD_func` value to the matching
line. For `type = 2` buttons, pick the MarcDuino code closest to what it did, or `0` (free).

| Shadow MD (foot) | ShadowMDLite | Shadow MD (dome) | ShadowMDLite |
|---|---|---|---|
| `btnUP_MD_func` | `footBtnUp_MD_func` | `FTbtnUP_MD_func` | `domeBtnUp_MD_func` |
| `btnDown_MD_func` | `footBtnDown_MD_func` | `FTbtnDown_MD_func` | `domeBtnDown_MD_func` |
| `btnLeft_MD_func` | `footBtnLeft_MD_func` | `FTbtnLeft_MD_func` | `domeBtnLeft_MD_func` |
| `btnRight_MD_func` | `footBtnRight_MD_func` | `FTbtnRight_MD_func` | `domeBtnRight_MD_func` |
| `btnUP_CROSS_MD_func` | `footBtnUpCross_MD_func` | `FTbtnUP_CROSS_MD_func` | `domeBtnUpCross_MD_func` |
| `btnUP_CIRCLE_MD_func` | `footBtnUpCircle_MD_func` | `FTbtnUP_CIRCLE_MD_func` | `domeBtnUpCircle_MD_func` |
| `btnUP_PS_MD_func` | `footBtnUpPS_MD_func` | `FTbtnUP_PS_MD_func` | `domeBtnUpPS_MD_func` |

The same pattern applies to Down, Left and Right with CROSS, CIRCLE and PS
(`btnLeft_CROSS_MD_func` → `footBtnLeftCross_MD_func`, `FTbtnRight_PS_MD_func` →
`domeBtnRightPS_MD_func`, and so on).

ShadowMDLite also has combos Shadow MD did not: arrow + **L1** on both controllers, dome arrow +
**L2** (with CROSS and CIRCLE), and the cross-controller **L3** combos (README, section 6.1).
They are filled with the author's choices or `0`; set them to whatever you like. The MarcDuino
codes are listed at the top of the sketch and in [`CheatSheet.md`](CheatSheet.md).

## Step 5: optional features

Near the settings there are a few `#define` switches. For a plain migration, leave them as they
are, or set to `0` what you do not use:

- `ENABLE_PC_TO_MARCDUINO_FORWARDING`: send MarcDuino commands from a PC over USB
  ([`PCSerialProtocol.md`](PCSerialProtocol.md)).
- `ENABLE_DOME_TRACKER`: lets a PC or a Raspberry Pi turn the dome (`DM:<speed>`).
- `ENABLE_ROS2D2`: the bridge to ROS2D2. Leave it at `0` until you add the Raspberry Pi.
- `SHADOW_DEBUG`, `SHADOW_VERBOSE`: debug output on the serial monitor; comment the line out to
  turn it off.

## Step 6: flash and test, wheels in the air

1. Board **Arduino Mega ADK**, your COM port, **Upload**.
2. **Lift the droid so the wheels are off the ground.**
3. Check, in this order:
   - Both controllers connect.
   - Foot stick forward drives forward and right turns right. If not, flip `invertDriveDirection`
     or `invertTurnDirection` (`1` ↔ `-1`).
   - Dome stick right turns the dome right. If not, flip `invertDomeDirection`.
   - The speed toggle (overspeed) works.
   - Every button combo plays the MarcDuino action you mapped.
   - Dome automation: `L2 + CIRCLE` starts it, `L2 + CROSS` stops it.
4. Put the droid down and drive it gently before the first event.

If anything goes wrong, flash your backup from step 1 and you are back where you started.
