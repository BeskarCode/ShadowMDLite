// FootAuto.h - ROS2D2 autonomous mode for the feet, as plain C++ (no Arduino, no STL)
// so it can be tested on a PC (test/run_tests.sh).
//
// The mode is switched on only from the controller (PS + L2 on the foot Nav). While it
// is on, the Raspberry Pi drives the feet with ROS:FOOT:<drive>,<turn>. The foot stick,
// the same combo, losing the controller or the Pi (ROS:AUTO:0) switch it off, and then
// the droid stays still until it is switched on again from the controller.
//
// Face tracking (the dome follows faces) is a mode of its own, DomeTrack, switched with
// PS + L2 on the dome Nav: the dome can look at people while the feet stay still.
//
// Times are uint32_t on purpose: millis() is 32 bits on the Mega, and the unsigned
// subtraction keeps the timeout right when millis() wraps after 49 days.

#ifndef FOOT_AUTO_H
#define FOOT_AUTO_H

#include <stddef.h>
#include <stdint.h>

// Long constant tables live in flash on the Mega (RAM is only 8 KB); on the PC they are
// plain arrays. Read them with rosFlashChar().
#if defined(__AVR__)
#include <avr/pgmspace.h>
#define ROS_FLASH PROGMEM
inline char rosFlashChar(const char* p) { return (char)pgm_read_byte(p); }
#else
#define ROS_FLASH
inline char rosFlashChar(const char* p) { return *p; }
#endif

enum FootAutoReason : uint8_t { FA_BOOT, FA_BUTTON, FA_STICK, FA_CONTROLLER, FA_PI };

// The autonomous modes. Each one is switched on by its own controller combo (ComboBindings).
enum RosMode : uint8_t { MODE_NONE = 0, MODE_GOTO, MODE_FORWARD, MODE_PATROL, MODE_FOLLOW };

inline const char* modeName(RosMode m)
{
    switch (m)
    {
        case MODE_GOTO:    return "goto";
        case MODE_FORWARD: return "forward";
        case MODE_PATROL:  return "patrol";
        case MODE_FOLLOW:  return "follow";
        default:           return "none";
    }
}

inline size_t rosStrLen(const char* s)
{
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

// Does the name (len chars, not NUL-terminated) equal the C string lit?
inline bool nameEquals(const char* name, size_t len, const char* lit)
{
    size_t i = 0;
    for (; i < len; i++)
        if (lit[i] == '\0' || name[i] != lit[i]) return false;
    return lit[i] == '\0';
}

// nameEquals() with lit in ROS_FLASH.
inline bool nameEqualsFlash(const char* name, size_t len, const char* lit)
{
    size_t i = 0;
    for (; i < len; i++)
    {
        char c = rosFlashChar(lit + i);
        if (c == '\0' || name[i] != c) return false;
    }
    return rosFlashChar(lit + i) == '\0';
}

// MODE_NONE when the name is not a mode (and for "none").
inline RosMode modeFromName(const char* name, size_t len)
{
    for (uint8_t m = MODE_GOTO; m <= MODE_FOLLOW; m++)
        if (nameEquals(name, len, modeName((RosMode)m))) return (RosMode)m;
    return MODE_NONE;
}

// Every controller combo that can start a mode. Same names and order in firmware, modes.yaml
// and the console. Each one is a check...() function of ShadowMDLite.ino. In flash on the
// Mega (ROS_FLASH); ROS_COMBO_NAME_SIZE fits the longest name, DOME_RIGHT_L2_CIRCLE.
const int ROS_COMBO_COUNT = 61;
const int ROS_COMBO_NAME_SIZE = 21;
const int ROS_FIRST_DOME_COMBO = 25;   // DOME_UP: from here on the arrow is on the dome Nav
static const char ROS_COMBO_NAMES[ROS_COMBO_COUNT][ROS_COMBO_NAME_SIZE] ROS_FLASH = {
    "FOOT_PS_L2",
    "FOOT_UP", "FOOT_UP_CROSS", "FOOT_UP_CIRCLE", "FOOT_UP_L1", "FOOT_UP_PS", "FOOT_UP_L3",
    "FOOT_DOWN", "FOOT_DOWN_CROSS", "FOOT_DOWN_CIRCLE", "FOOT_DOWN_L1", "FOOT_DOWN_PS", "FOOT_DOWN_L3",
    "FOOT_LEFT", "FOOT_LEFT_CROSS", "FOOT_LEFT_CIRCLE", "FOOT_LEFT_L1", "FOOT_LEFT_PS", "FOOT_LEFT_L3",
    "FOOT_RIGHT", "FOOT_RIGHT_CROSS", "FOOT_RIGHT_CIRCLE", "FOOT_RIGHT_L1", "FOOT_RIGHT_PS", "FOOT_RIGHT_L3",
    "DOME_UP", "DOME_UP_CROSS", "DOME_UP_CIRCLE", "DOME_UP_L1", "DOME_UP_PS", "DOME_UP_L3",
    "DOME_UP_L2", "DOME_UP_L2_CROSS", "DOME_UP_L2_CIRCLE",
    "DOME_DOWN", "DOME_DOWN_CROSS", "DOME_DOWN_CIRCLE", "DOME_DOWN_L1", "DOME_DOWN_PS", "DOME_DOWN_L3",
    "DOME_DOWN_L2", "DOME_DOWN_L2_CROSS", "DOME_DOWN_L2_CIRCLE",
    "DOME_LEFT", "DOME_LEFT_CROSS", "DOME_LEFT_CIRCLE", "DOME_LEFT_L1", "DOME_LEFT_PS", "DOME_LEFT_L3",
    "DOME_LEFT_L2", "DOME_LEFT_L2_CROSS", "DOME_LEFT_L2_CIRCLE",
    "DOME_RIGHT", "DOME_RIGHT_CROSS", "DOME_RIGHT_CIRCLE", "DOME_RIGHT_L1", "DOME_RIGHT_PS", "DOME_RIGHT_L3",
    "DOME_RIGHT_L2", "DOME_RIGHT_L2_CROSS", "DOME_RIGHT_L2_CIRCLE"
};

// Index into ROS_COMBO_NAMES, or -1.
inline int comboFromName(const char* name, size_t len)
{
    for (int i = 0; i < ROS_COMBO_COUNT; i++)
        if (nameEqualsFlash(name, len, ROS_COMBO_NAMES[i])) return i;
    return -1;
}

// Which combo starts which mode: up to 8 assignments. Without any ROS:BIND: the droid
// behaves as it always did (PS + L2 on the foot Nav -> goto).
class ComboBindings
{
public:
    static const int MAX = 8;

    ComboBindings() { reset(); }

    void clear()
    {
        for (int i = 0; i < MAX; i++) { combo[i] = -1; mode[i] = MODE_NONE; }
    }

    void reset()
    {
        clear();
        combo[0] = 0;                 // FOOT_PS_L2
        mode[0] = MODE_GOTO;
    }

    // Assign (or replace) a combo; MODE_NONE frees it. false if the table is full.
    bool bind(int c, RosMode m)
    {
        if (c < 0 || c >= ROS_COMBO_COUNT) return false;
        int slot = -1, freeSlot = -1;
        for (int i = 0; i < MAX; i++)
        {
            if (combo[i] == c) slot = i;
            else if (combo[i] < 0 && freeSlot < 0) freeSlot = i;
        }
        if (m == MODE_NONE)
        {
            if (slot >= 0) { combo[slot] = -1; mode[slot] = MODE_NONE; }
            return true;
        }
        if (slot < 0) slot = freeSlot;
        if (slot < 0) return false;
        combo[slot] = (int8_t)c;
        mode[slot] = m;
        return true;
    }

    RosMode modeFor(int c) const
    {
        if (c < 0) return MODE_NONE;
        for (int i = 0; i < MAX; i++)
            if (combo[i] == c) return mode[i];
        return MODE_NONE;
    }

private:
    int8_t combo[MAX];
    RosMode mode[MAX];
};

struct FootAutoConfig
{
    int maxDrive;        // |drive| clamp, Sabertooth units
    int maxTurn;         // |turn| clamp
    uint32_t timeoutMs;  // no ROS:FOOT: for this long -> stop
    int ramp;            // max change per tick
};

// What to send to the Sabertooth on one tick: drive/turn, a stop, or nothing.
struct FootOutput
{
    bool send;
    bool stop;
    int drive;
    int turn;
};

inline int footAutoAbs(int v) { return v < 0 ? -v : v; }

inline int footAutoClamp(int v, int limit)
{
    if (v > limit) return limit;
    if (v < -limit) return -limit;
    return v;
}

inline int footAutoStep(int current, int target, int ramp)
{
    if (target > current + ramp) return current + ramp;
    if (target < current - ramp) return current - ramp;
    return target;
}

class FootAuto
{
public:
    explicit FootAuto(FootAutoConfig c)
        : cfg(c), isOn(false), current(MODE_NONE), reason(FA_BOOT), hasCommand(false), moving(false),
          lastCommandMs(0), targetDrive(0), targetTurn(0), drive(0), turn(0) {}

    bool on() const { return isOn; }
    FootAutoReason lastOffReason() const { return reason; }

    RosMode mode() const { return current; }

    // A mode combo: off -> on with m; on with m -> off (reason: button); on with another
    // mode -> switch to m (the feet stop until the next command). Returns true if
    // something changed (false only for MODE_NONE).
    bool press(RosMode m)
    {
        if (m == MODE_NONE) return false;
        if (isOn && current == m) return turnOff(FA_BUTTON);
        isOn = true;
        current = m;
        hasCommand = false;
        return true;
    }

    // false if it was already off.
    bool turnOff(FootAutoReason why)
    {
        if (!isOn) return false;
        isOn = false;
        current = MODE_NONE;
        reason = why;
        hasCommand = false;
        return true;
    }

    // A ROS:FOOT: command. Rejected (false) while the mode is off.
    bool command(int d, int t, uint32_t nowMs)
    {
        if (!isOn) return false;
        targetDrive = footAutoClamp(d, cfg.maxDrive);
        targetTurn = footAutoClamp(t, cfg.maxTurn);
        lastCommandMs = nowMs;
        hasCommand = true;
        return true;
    }

    // Once per Sabertooth send cycle.
    FootOutput tick(uint32_t nowMs)
    {
        if (!isOn || !hasCommand || (uint32_t)(nowMs - lastCommandMs) >= cfg.timeoutMs)
        {
            // Off, or the Pi went quiet: stop at once (no ramp), then say nothing.
            hasCommand = false;
            drive = turn = targetDrive = targetTurn = 0;
            return stopOnce();
        }
        drive = footAutoStep(drive, targetDrive, cfg.ramp);
        turn = footAutoStep(turn, targetTurn, cfg.ramp);
        if (drive == 0 && turn == 0) return stopOnce();
        moving = true;
        FootOutput o = {true, false, drive, turn};
        return o;
    }

private:
    FootOutput stopOnce()
    {
        FootOutput o = {false, moving, 0, 0};
        moving = false;
        return o;
    }

    FootAutoConfig cfg;
    bool isOn;
    RosMode current;
    FootAutoReason reason;
    bool hasCommand;
    bool moving;
    uint32_t lastCommandMs;
    int targetDrive, targetTurn;
    int drive, turn;
};

// Face tracking: whether the dome tracker (ROS:DOME: / DM:) may turn the dome. Switched on
// only from the controller (PS + L2 on the dome Nav); the same combo, losing the dome Nav
// or the Pi (ROS:TRACK:0) switch it off. The dome stick only pauses it for a moment, and
// the feet's autonomous mode has nothing to do with it.
class DomeTrack
{
public:
    DomeTrack() : isOn(false), reason(FA_BOOT) {}

    bool on() const { return isOn; }
    FootAutoReason lastOffReason() const { return reason; }

    // The combo: off -> on, on -> off (reason: button).
    void toggle()
    {
        if (isOn) turnOff(FA_BUTTON);
        else isOn = true;
    }

    // false if it was already off.
    bool turnOff(FootAutoReason why)
    {
        if (!isOn) return false;
        isOn = false;
        reason = why;
        return true;
    }

private:
    bool isOn;
    FootAutoReason reason;
};

// Is the foot stick being used? Out of the dead zone on either axis, except when the
// foot L2 is held with no dome controller: then the foot stick is turning the dome.
inline bool footStickCancels(int hatX, int hatY, int deadzone, bool footL2Held, bool domeConnected)
{
    if (footL2Held && !domeConnected) return false;
    return footAutoAbs(hatX - 128) >= deadzone || footAutoAbs(hatY - 128) >= deadzone;
}

// A Nav that has sent nothing for limitMs is as good as lost for the autonomous mode, even
// while the Bluetooth stack still calls it connected (out of range, dying battery): its
// stick readings are frozen, so the person cannot take over with it.
inline bool footControllerSilent(uint32_t nowMs, uint32_t lastMessageMs, uint32_t limitMs)
{
    return (uint32_t)(nowMs - lastMessageMs) > limitMs;
}

// true only on the cycle a button combo goes from released to pressed.
class ComboEdge
{
public:
    ComboEdge() : was(false) {}
    bool update(bool pressed)
    {
        bool edge = pressed && !was;
        was = pressed;
        return edge;
    }

private:
    bool was;
};

// A bound MarcDuino combo re-fires about once a second while it is held (the sketch's
// marcDuinoButtonCounter), but a mode combo must act once per press. One latch per Nav:
// press() is true only the first time a combo fires; release() when that Nav's arrows
// are all up.
class ComboLatch
{
public:
    ComboLatch() : held(-1) {}
    bool press(int c)
    {
        if (c == held) return false;
        held = c;
        return true;
    }
    void release() { held = -1; }

private:
    int held;
};

// --- ROS: commands ------------------------------------------------------------------------
//
//   ROS:FOOT:<drive>,<turn>   feet, each -127..127 (FootAuto clamps them to its limits)
//   ROS:AUTO:0                switch the mode off (the Pi cannot switch it on)
//   ROS:AUTO:?                ask for the state
//   ROS:DOME:<speed>          dome, -127..127, same as DM:
//   ROS:TRACK:0               switch face tracking off (the Pi cannot switch it on)
//   ROS:TRACK:?               ask for the state
//   ROS:BIND:<COMBO>,<mode>   assign a combo to a mode (goto|forward|patrol|follow; none frees it)
//   ROS:BIND:CLEAR            free every assignment
//   ROS:BIND:RESET            back to the default (FOOT_PS_L2 -> goto)
//
// Exact upper case: these come from software. Nothing may follow the last number.

enum RosCmdKind : uint8_t { ROS_NONE, ROS_FOOT, ROS_AUTO_OFF, ROS_AUTO_QUERY, ROS_DOME,
                           ROS_TRACK_OFF, ROS_TRACK_QUERY, ROS_BIND, ROS_BIND_CLEAR,
                           ROS_BIND_RESET, ROS_ERROR };

struct RosCmd
{
    RosCmdKind kind;
    int a;               // ROS_FOOT: drive, ROS_DOME: speed, ROS_BIND: combo index
    int b;               // ROS_FOOT: turn, ROS_BIND: RosMode
    const char* error;   // ROS_ERROR: what was wrong, without the "ERR " prefix
};

inline bool rosSkipPrefix(const char*& p, const char* prefix)
{
    const char* q = p;
    while (*prefix)
    {
        if (*q != *prefix) return false;
        q++;
        prefix++;
    }
    p = q;
    return true;
}

// Signed integer from -127 to 127; advances p past it.
inline bool rosReadSpeed(const char*& p, int& out)
{
    bool negative = false;
    if (*p == '-' || *p == '+')
    {
        negative = (*p == '-');
        p++;
    }
    if (*p < '0' || *p > '9') return false;
    int v = 0;
    while (*p >= '0' && *p <= '9')
    {
        v = v * 10 + (*p - '0');
        if (v > 127) return false;      // also keeps a 16-bit int from overflowing
        p++;
    }
    out = negative ? -v : v;
    return true;
}

inline RosCmd rosCmd(RosCmdKind kind, int a = 0, int b = 0, const char* error = 0)
{
    RosCmd c = {kind, a, b, error};
    return c;
}

inline RosCmd parseRosCommand(const char* line)
{
    const char* p = line;
    if (!rosSkipPrefix(p, "ROS:")) return rosCmd(ROS_NONE);

    if (rosSkipPrefix(p, "FOOT:"))
    {
        int drive, turn;
        if (rosReadSpeed(p, drive) && *p == ',' && rosReadSpeed(++p, turn) && *p == '\0')
            return rosCmd(ROS_FOOT, drive, turn);
        return rosCmd(ROS_ERROR, 0, 0, "ROS:FOOT needs <drive>,<turn> from -127 to 127");
    }
    if (rosSkipPrefix(p, "AUTO:"))
    {
        if (p[0] == '0' && p[1] == '\0') return rosCmd(ROS_AUTO_OFF);
        if (p[0] == '?' && p[1] == '\0') return rosCmd(ROS_AUTO_QUERY);
        return rosCmd(ROS_ERROR, 0, 0, "ROS:AUTO only 0 or ?");
    }
    if (rosSkipPrefix(p, "TRACK:"))
    {
        if (p[0] == '0' && p[1] == '\0') return rosCmd(ROS_TRACK_OFF);
        if (p[0] == '?' && p[1] == '\0') return rosCmd(ROS_TRACK_QUERY);
        return rosCmd(ROS_ERROR, 0, 0, "ROS:TRACK only 0 or ?");
    }
    if (rosSkipPrefix(p, "BIND:"))
    {
        if (nameEquals(p, rosStrLen(p), "CLEAR")) return rosCmd(ROS_BIND_CLEAR);
        if (nameEquals(p, rosStrLen(p), "RESET")) return rosCmd(ROS_BIND_RESET);
        const char* comma = p;
        while (*comma && *comma != ',') comma++;
        if (*comma != ',') return rosCmd(ROS_ERROR, 0, 0, "ROS:BIND needs <combo>,<mode>");
        int combo = comboFromName(p, (size_t)(comma - p));
        if (combo < 0) return rosCmd(ROS_ERROR, 0, 0, "unknown combo");
        const char* name = comma + 1;
        size_t len = rosStrLen(name);
        if (nameEquals(name, len, "none")) return rosCmd(ROS_BIND, combo, MODE_NONE);
        RosMode m = modeFromName(name, len);
        if (m == MODE_NONE) return rosCmd(ROS_ERROR, 0, 0, "unknown mode");
        return rosCmd(ROS_BIND, combo, m);
    }
    if (rosSkipPrefix(p, "DOME:"))
    {
        int speed;
        if (rosReadSpeed(p, speed) && *p == '\0') return rosCmd(ROS_DOME, speed);
        return rosCmd(ROS_ERROR, 0, 0, "ROS:DOME needs a speed from -127 to 127");
    }
    return rosCmd(ROS_ERROR, 0, 0, "unknown ROS: command");
}

inline const char* reasonName(FootAutoReason r)
{
    switch (r)
    {
        case FA_BUTTON:     return "button";
        case FA_STICK:      return "stick";
        case FA_CONTROLLER: return "controller";
        case FA_PI:         return "pi";
        default:            return "boot";
    }
}

// At most once every minGapMs. Start lastMs at (now - minGapMs) so the first call passes.
inline bool rateLimitAllows(uint32_t& lastMs, uint32_t nowMs, uint32_t minGapMs)
{
    if ((uint32_t)(nowMs - lastMs) < minGapMs) return false;
    lastMs = nowMs;
    return true;
}

#endif
