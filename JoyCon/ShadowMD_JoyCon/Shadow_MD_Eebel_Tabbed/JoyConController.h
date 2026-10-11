// ---------------------------------------------------------------------------------------
// JoyConController.h
//
// DroidController implementation for a single Nintendo Switch Joy-Con, left or right.
//
// A Joy-Con speaks the same Bluetooth HID protocol as the Switch Pro controller, so this
// class sits on top of SwitchProBT from the USB Host Shield 2.0 library (1.7.0 or newer)
// and only deals with what is different about a single half:
//
//   - A Joy-Con has ONE stick. The left half reports it in the left stick bytes, the right
//     half in the right stick bytes, so which one we read depends on the side.
//   - The buttons live in different bits on each half, and there is no CROSS/CIRCLE pair.
//     The role mapping below is configurable for exactly that reason.
//   - Held upright, like you would hold a Nav, the stick axes are as reported. Held
//     sideways, the way you would play a two player game, everything is rotated 90 degrees.
//   - The centre is not exactly 2048. Factory calibration lives in the controller's SPI
//     flash at 0x603D, which the library does not expose, so this class auto centres on
//     connection instead: it averages the stick while it is untouched and uses that as
//     zero. Cheaper, and it also soaks up the drift these sticks are famous for.
//
// The library parser handles the one real trap for us: a Joy-Con boots in "simple HID
// mode" (report 0x3F) where the stick is only an 8 direction hat. SwitchProParser sees
// that report and immediately asks for the full 0x30 report with real analog axes.
//
// Reference: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering
// ---------------------------------------------------------------------------------------

#ifndef JOYCON_CONTROLLER_H
#define JOYCON_CONTROLLER_H

#include <SwitchProBT.h>
#include "DroidController.h"

enum JoyConSide { JOYCON_LEFT, JOYCON_RIGHT };

// How the Joy-Con sits in your hand.
//   JOYCON_UPRIGHT : stick at the top, rail against your palm. This is the Nav-like grip
//                    and the default.
//   JOYCON_SIDEWAYS: turned 90 degrees, the way it is held as a mini controller.
enum JoyConGrip { JOYCON_UPRIGHT, JOYCON_SIDEWAYS };

// ---------------------------------------------------------------------------------------
// Role mapping
//
// A single Joy-Con exposes nine usable buttons through this library, the same number a PS3
// Nav has, but they are not the same nine. SL and SR exist physically on the rail, but the
// Switch Pro parser does not expose those bits, and they are awkward to reach while
// gripping the thing anyway.
//
// That leaves one of the ten droid roles without a home, and which one you sacrifice is a
// real design decision, not an arbitrary one.
//
// Shadow MD uses three modifier banks: CROSS, CIRCLE and PS, each one combined with the
// four directions. A Joy-Con only has two free buttons for that (minus and capture on the
// left, plus and home on the right), so the three banks do not fit... unless the stick
// click is used as the third one.
//
// So the default mapping below gives up DB_STICK instead of DB_B, because in the stock
// ShadowMD Lite map the stick click is almost unused: its only real job is the
// stick enable chord, and every cross controller L3 combination is FREE. Sacrificing
// DB_B instead would cost eight working functions. Sacrificing DB_STICK costs one chord,
// which moves to the SYS bank.
//
// This struct is the only place that decides, so if your own map uses L3 heavily, swap it.
// ---------------------------------------------------------------------------------------
struct JoyConMapping
{
  ButtonEnum up, down, left, right;
  ButtonEnum a, b, l1, l2, stick, sys;
};

// Left Joy-Con, the foot controller. The four direction buttons are a real d-pad.
//   d-pad -> directions, L -> L1, ZL -> L2,
//   minus -> A (the CROSS bank), capture -> B (the CIRCLE bank),
//   stick click -> SYS (the PS bank), nothing -> STICK
static const JoyConMapping JOYCON_MAP_LEFT =
{
  /* up    */ UP,
  /* down  */ DOWN,
  /* left  */ LEFT,
  /* right */ RIGHT,
  /* a     */ MINUS,
  /* b     */ CAPTURE,
  /* l1    */ L,                  // the L shoulder. NOT L1: on this library L1 lands in an
  /* l2    */ ZL,                 // empty slot of the Switch button table and reads wrong.
  /* stick */ (ButtonEnum)0xFF,   // unmapped, see the note above
  /* sys   */ L3                  // stick click carries the third modifier bank
};

// Right Joy-Con, the dome controller. The four face buttons stand in for the d-pad, in the
// diamond you see when the Joy-Con is upright: X on top, B below, Y left, A right.
//   R -> L1, ZR -> L2,
//   plus -> A (the CROSS bank), home -> B (the CIRCLE bank),
//   stick click -> SYS (the PS bank), nothing -> STICK
static const JoyConMapping JOYCON_MAP_RIGHT =
{
  /* up    */ X,
  /* down  */ B,
  /* left  */ Y,
  /* right */ A,
  /* a     */ PLUS,
  /* b     */ HOME,
  /* l1    */ R,                  // the R shoulder. NOT R1, same reason as the left map.
  /* l2    */ ZR,
  /* stick */ (ButtonEnum)0xFF,   // unmapped, see the note above
  /* sys   */ R3                  // stick click carries the third modifier bank
};

class JoyConController : public DroidController
{
public:
  // btd  : the shared BTD instance.
  // side : which half this object is talking to.
  // label: short name for the console.
  // pair : true the first time, to pair a new Joy-Con (hold its sync button).
  JoyConController(BTD* btd, JoyConSide side, const char* label, bool pair = false)
    : _joy(btd, pair),
      _side(side),
      _label(label),
      _map(side == JOYCON_LEFT ? JOYCON_MAP_LEFT : JOYCON_MAP_RIGHT) {}

  // ---- configuration, all optional ------------------------------------------------------

  void setGrip(JoyConGrip grip) { _grip = grip; }

  // Override the default role mapping.
  void setMapping(const JoyConMapping& m) { _map = m; }

  // Raw stick deflection treated as full scale. Joy-Con sticks rarely reach the
  // theoretical 2048, so the default is deliberately lower and the result is clamped.
  void setStickRange(int16_t range) { _range = range; }

  // Throw away the stored centre and measure it again. Leave the stick alone while this
  // runs. Called automatically on every connection.
  void recalibrate()
  {
    _calibrating = true;
    _calSamples = 0;
    _calSumX = 0;
    _calSumY = 0;
  }

  // ---- DroidController -----------------------------------------------------------------

  void begin() override
  {
    clearState();
    _wasConnected = false;
    _centreX = 0;
    _centreY = 0;
    _lastMessage = millis();
    _lastCounter = 0;
  }

  void update() override
  {
    rememberPrevious();

    const bool now = _joy.connected();

    if (now && !_wasConnected)
    {
      recalibrate();                       // fresh connection, measure the centre again
      _joy.setLedOn(_side == JOYCON_LEFT ? LED1 : LED2);
      _lastMessage = millis();
      _lastCounter = _joy.getMessageCounter();
    }
    _wasConnected = now;

    if (!now)
    {
      setStick(0, 0);
      setButtons(0);
      return;
    }

    // The parser counts every full report it decodes. A counter that stops moving means
    // the link is gone even though the stack has not noticed yet, which is exactly what
    // the droid watchdog needs to see.
    const uint16_t counter = _joy.getMessageCounter();
    if (counter != _lastCounter)
    {
      _lastCounter = counter;
      _lastMessage = millis();
    }

    // ---- stick ---------------------------------------------------------------------------
    // Only one stick is populated on a single Joy-Con. The parser already inverts Y so that
    // forward is positive; this layer wants forward negative, like the Nav reports it, so
    // it gets inverted back below.
    int16_t rawX, rawY;
    if (_side == JOYCON_LEFT)
    {
      rawX = _joy.getAnalogHat(LeftHatX);
      rawY = _joy.getAnalogHat(LeftHatY);
    }
    else
    {
      rawX = _joy.getAnalogHat(RightHatX);
      rawY = _joy.getAnalogHat(RightHatY);
    }

    if (_calibrating)
    {
      _calSumX += rawX;
      _calSumY += rawY;
      if (++_calSamples >= CAL_SAMPLES)
      {
        _centreX = _calSumX / CAL_SAMPLES;
        _centreY = _calSumY / CAL_SAMPLES;
        _calibrating = false;
      }
      setStick(0, 0);       // report centred while measuring, never a stale value
    }
    else
    {
      int16_t x = rawX - _centreX;
      int16_t y = rawY - _centreY;

      if (_grip == JOYCON_SIDEWAYS)
      {
        // Turning the Joy-Con 90 degrees turns the stick with it.
        const int16_t t = x;
        if (_side == JOYCON_LEFT) { x = -y;  y = t;  }
        else                      { x =  y;  y = -t; }
      }

      setStick(scale(x), -scale(y));   // invert Y: forward is negative in this layer
    }

    // ---- buttons -------------------------------------------------------------------------
    uint16_t b = 0;
    if (held(_map.up))    b |= DB_UP;
    if (held(_map.down))  b |= DB_DOWN;
    if (held(_map.left))  b |= DB_LEFT;
    if (held(_map.right)) b |= DB_RIGHT;
    if (held(_map.a))     b |= DB_A;
    if (held(_map.b))     b |= DB_B;
    if (held(_map.l1))    b |= DB_L1;
    if (held(_map.l2))    b |= DB_L2;
    if (held(_map.stick)) b |= DB_STICK;
    if (held(_map.sys))   b |= DB_SYS;
    setButtons(b);
  }

  bool isConnected() const override { return const_cast<SwitchProBT&>(_joy).connected(); }

  void disconnect() override { _joy.disconnect(); }

  uint32_t millisSinceLastMessage() const override
  {
    const uint32_t now = millis();
    return (now >= _lastMessage) ? (now - _lastMessage) : 0;
  }

  const char* name() const override { return _label; }

  // ---- extras a Nav cannot do ------------------------------------------------------------

  // 0 to 4, where 0 is critical. Worth printing at startup and before an event.
  uint8_t batteryLevel() { return _joy.getBatteryLevel(); }

  // A short buzz. Useful as confirmation when a mode changes, for instance entering voice
  // mode, which on a Nav you can only tell by listening to the droid.
  void buzz(bool on) { _joy.setRumble(on, on); }

  bool isCalibrating() const { return _calibrating; }

  // Escape hatch for anything this wrapper does not cover yet.
  SwitchProBT* raw() { return &_joy; }

private:
  static const uint8_t CAL_SAMPLES = 30;   // about half a second at 60 Hz

  bool held(ButtonEnum b)
  {
    if ((uint8_t)b == 0xFF) return false;  // role deliberately unmapped
    return _joy.getButtonPress(b);
  }

  int16_t scale(int32_t v) const
  {
    int32_t out = (v * (int32_t)DC_STICK_MAX) / _range;
    if (out >  DC_STICK_MAX) out =  DC_STICK_MAX;
    if (out < -DC_STICK_MAX) out = -DC_STICK_MAX;
    return (int16_t)out;
  }

  SwitchProBT  _joy;
  JoyConSide   _side;
  const char*  _label;
  JoyConMapping _map;
  JoyConGrip   _grip = JOYCON_UPRIGHT;
  int16_t      _range = 1700;

  bool     _wasConnected = false;
  uint32_t _lastMessage = 0;
  uint16_t _lastCounter = 0;

  bool     _calibrating = false;
  uint8_t  _calSamples = 0;
  int32_t  _calSumX = 0;
  int32_t  _calSumY = 0;
  int16_t  _centreX = 0;
  int16_t  _centreY = 0;
};

#endif // JOYCON_CONTROLLER_H
