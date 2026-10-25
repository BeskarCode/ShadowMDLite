# PC serial protocol — commands over USB (`Serial` on Mega ADK)

This lets a **PC** drive **Marcduino** boards through the same **USB serial** port as the **Mega ADK** (`Serial`, typically **115200**).

**Note:** `Serial` is also used for **debug logs** to the PC. If your terminal **echoes** traffic or you paste unrelated text, the firmware may treat it as protocol input.

## Enable / disable

- Controlled by:

  `#define ENABLE_PC_TO_MARCDUINO_FORWARDING ...`

- The parser runs only when that define is enabled.

## Protocol (one line per command)

- Send **one command per line**, terminated with `\n` or `\r\n` (`\r` alone is accepted).
- If the line already ends with `\r`, it is kept; otherwise the firmware **adds** `\r` when forwarding to Marcduino UARTs as needed.

## Commands

### `HELP`

Prints protocol help on `Serial`.

### `PING`

Replies with `PONG`.

### `S1:<COMMAND>`

Forwards `<COMMAND>` to `Serial1` (dome or body Marcduino depending on your wiring).

Examples:

- `S1::SE01`
- `S1::OP11`

`<COMMAND>` may start with `:` (Marcduino / BetterDuino style), e.g. `:SE01`.

### `S3:<COMMAND>`

Forwards `<COMMAND>` to `Serial3`.

Examples:

- `S3::OP04`
- `S3::CL00`

### `DT:<LOCAL>`

Optional prefix meaning **handle on the Mega over USB only** — nothing is sent to `Serial1` / `Serial3`. The part after `DT:` must be one of the same **local** forms as without any prefix: `HELP`, `PING`, or `MD:<mdFunc>` (0–89).

Examples:

- `DT:HELP`
- `DT:PING`
- `DT:MD:14`

Marcduino-style lines (`*…`, `:…`, `#…`, `@…`, etc.) **cannot** be used after `DT:`; forward those with `S1:` (dome) or `S3:` (body), e.g. `S1:*ON00`.

### `MD:<mdFunc>`

Calls `marcDuinoButtonPush(1, mdFunc)` with **type = 1** (standard Marcduino function code).

This uses the same `switch (MD_func)` path as the PS3 Nav button map.

**Do not** prefix this with `S1:` or `S3:` — the Mega must receive the line as `MD:14` (etc.) on USB. If a PC tool prepends `S1:`, the payload is forwarded to Serial1 as text and BetterDuino will not run `marcDuinoButtonPush` on the Mega.

Examples:

- `MD:14` — Full Awake+ reset (if mapped in your sketch)
- `MD:30` — Open all dome panels
- `MD:33` — Close all dome panels

## ROS2D2: autonomous mode and face tracking (`ENABLE_ROS2D2`, `ENABLE_DOME_TRACKER`)

Commands sent by the Raspberry Pi running ROS2D2 (the ROS 2 brain).
They all start with `ROS:`, in exact upper case, so they stand apart from the console commands above.

**Off by default** (`#define ENABLE_ROS2D2 0`): the sketch then builds byte for byte as before and none of this exists, so droids without ROS2D2 are not affected. Set it to `1` to use it.

**The autonomous mode is switched on only from the controller: PS + L2 on the foot Nav.**
The same combo switches it off, and so do touching the foot stick, losing (or a fault on) the
foot controller, disabling the drive stick (PS + CROSS) and `ROS:AUTO:0`. After that the droid
stays still until PS + L2 again; nothing resumes by itself. The Mega boots with the mode off.

**Face tracking (the dome follows faces) is a separate mode: PS + L2 on the dome Nav.**
The same combo switches it off, and so do losing (or a fault on) the dome controller and
`ROS:TRACK:0`. The dome stick does not switch it off: it only holds the tracker off for
`DOME_TRACKER_MANUAL_HOLD_MS` (2 s). The two modes are independent: the dome can follow faces
with the feet still, and the feet can drive on their own with the dome left alone. Face
tracking needs a dome Nav. The Mega boots with it off.

| Command | What it does | Reply |
|---|---|---|
| `ROS:FOOT:<drive>,<turn>` | Feet, Sabertooth units −127..127 each, clamped to `FOOT_AUTO_MAX_DRIVE` / `FOOT_AUTO_MAX_TURN` and ramped by `FOOT_AUTO_RAMP`. The feet stop if none arrives for `FOOT_AUTO_TIMEOUT_MS` (300 ms). Only in autonomous mode | None (the Pi sends ~15 a second). `ERR FOOT AUTO OFF` when the mode is off, at most once a second |
| `ROS:AUTO:0` | Switch the autonomous mode off | `AUTO OFF pi` (or the current state if it was already off) |
| `ROS:AUTO:?` | Ask for the state | `AUTO ON` or `AUTO OFF <reason>` |
| `ROS:TRACK:0` | Switch face tracking off | `TRACK OFF pi` (or the current state if it was already off) |
| `ROS:TRACK:?` | Ask for the state | `TRACK ON` or `TRACK OFF <reason>` |
| `ROS:DOME:<speed>` | Dome, −127..127, clamped to `DOME_TRACKER_MAX_SPEED`. Stops after `DOME_TRACKER_TIMEOUT_MS` without one; the dome stick always wins (and holds off the tracker for `DOME_TRACKER_MANUAL_HOLD_MS`). Only with face tracking on | None |
| `DM:<speed>` | Same as `ROS:DOME:`, kept for the DomeTracker proof of concept | None, or `ERR Invalid DM speed` |

`ROS:AUTO:1` (or anything but `0` / `?`) → `ERR ROS:AUTO only 0 or ?`: the Pi cannot switch the mode on.
`ROS:TRACK:1` → `ERR ROS:TRACK only 0 or ?`, for the same reason.
Malformed numbers, out-of-range values or extra characters → `ERR ROS:FOOT needs <drive>,<turn> from -127 to 127` (or the `ROS:DOME` equivalent), and nothing moves.

The Mega also prints the state by itself every time it changes:

```
AUTO OFF boot          at start-up
AUTO ON                PS + L2
AUTO OFF <reason>      reason: button | stick | controller | pi
TRACK OFF boot         at start-up
TRACK ON               PS + L2 on the dome Nav
TRACK OFF <reason>     reason: button | controller | pi
```

`FOOT_AUTO_ON_MD_FUNC` / `FOOT_AUTO_OFF_MD_FUNC` play a MarcDuino function (a sound, for example)
when the mode goes on or off; 0 = silent.

## Replies

On valid `HELP` / `PING` / `S1` / `S3` / `MD`, the firmware may print lines such as:

- `ShadowMD PC Serial forwarding ON`
- `PONG`
- `OK forwarded to Serial1` / `OK forwarded to Serial3`
- `OK marcDuinoButtonPush(1, mdFunc)`

On bad input (exact text is firmware-defined), for example:

- `ERR formato…` — use `S1:` / `S3:` for Marcduino, or local `HELP` / `PING` / `MD:<n>` / `DT:…`
- `ERR Missing mdFunc`, `ERR Invalid mdFunc`, `ERR mdFunc out of range (0-89)`, etc.
