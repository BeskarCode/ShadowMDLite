// FootAuto.h - ROS2D2 autonomous mode for the feet, as plain C++ (no Arduino, no STL)
// so it can be tested on a PC (test/run_tests.sh).
//
// The mode is switched on only from the controller (PS + L2 on the foot Nav). While it
// is on, the Raspberry Pi drives the feet with ROS:FOOT:<drive>,<turn> and the dome
// tracker may move the dome. The foot stick, the same combo, losing the controller or
// the Pi (ROS:AUTO:0) switch it off, and then the droid stays still until it is
// switched on again from the controller.
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

// Is the foot stick being used? Out of the dead zone on either axis, except when the
// foot L2 is held with no dome controller: then the foot stick is turning the dome.
inline bool footStickCancels(int hatX, int hatY, int deadzone, bool footL2Held, bool domeConnected)
{
    if (footL2Held && !domeConnected) return false;
    return footAutoAbs(hatX - 128) >= deadzone || footAutoAbs(hatY - 128) >= deadzone;
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

#endif
