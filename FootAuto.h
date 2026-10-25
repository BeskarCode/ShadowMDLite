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

#include <stdint.h>

enum FootAutoReason : uint8_t { FA_BOOT, FA_BUTTON, FA_STICK, FA_CONTROLLER, FA_PI };

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
        : cfg(c), isOn(false), reason(FA_BOOT), hasCommand(false), moving(false),
          lastCommandMs(0), targetDrive(0), targetTurn(0), drive(0), turn(0) {}

    bool on() const { return isOn; }
    FootAutoReason lastOffReason() const { return reason; }

    // The combo: off -> on, on -> off (reason: button). Always a change.
    bool toggle()
    {
        if (isOn) return turnOff(FA_BUTTON);
        isOn = true;
        hasCommand = false;
        return true;
    }

    // false if it was already off.
    bool turnOff(FootAutoReason why)
    {
        if (!isOn) return false;
        isOn = false;
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

// --- ROS: commands ------------------------------------------------------------------------
//
//   ROS:FOOT:<drive>,<turn>   feet, each -127..127 (FootAuto clamps them to its limits)
//   ROS:AUTO:0                switch the mode off (the Pi cannot switch it on)
//   ROS:AUTO:?                ask for the state
//   ROS:DOME:<speed>          dome, -127..127, same as DM:
//   ROS:TRACK:0               switch face tracking off (the Pi cannot switch it on)
//   ROS:TRACK:?               ask for the state
//
// Exact upper case: these come from software. Nothing may follow the last number.

enum RosCmdKind : uint8_t { ROS_NONE, ROS_FOOT, ROS_AUTO_OFF, ROS_AUTO_QUERY, ROS_DOME,
                           ROS_TRACK_OFF, ROS_TRACK_QUERY, ROS_ERROR };

struct RosCmd
{
    RosCmdKind kind;
    int a;               // ROS_FOOT: drive, ROS_DOME: speed
    int b;               // ROS_FOOT: turn
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
