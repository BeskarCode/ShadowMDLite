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
//   - Safety: the stick only drives once it has been seen centred. A worn Joy-Con can rest
//     40% off centre, which would drive the droid the moment it pairs. So the stick reads
//     zero after every connection until it sits in the centre; a centre measured too far
//     from where a stick rests is measured again; and a stick that freezes off centre
//     (a thumb never holds that still) stops counting until it comes back to the centre.
//     Buttons are not affected.
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

// The library only sends its queued commands from inside Parse(), one per incoming report.
// A freshly connected Joy-Con sits in simple mode (0x3F), which reports only on a button
// change, so with nobody touching it the request for full mode (0x30) is never sent. This
// lets the controller class send that request itself.
class JoyConBT : public SwitchProBT
{
public:
  JoyConBT(BTD* p, bool pair) : SwitchProBT(p, pair) {}

  // Rumble on or off right now. The library only sends its rumble report once a second from
  // inside Parse(), so a short buzz would often never go out.
  void rumbleNow(bool on)
  {
    uint8_t buf[10];
    buf[0] = 0x10;                    // rumble only
    buf[1] = _seq++ & 0x0F;
    for (uint8_t side = 0; side < 2; side++)
    {
      uint8_t* r = &buf[2 + side * 4];
      if (on) { r[0] = 0x28; r[1] = 0x88; r[2] = 0x60; r[3] = 0x61; }   // same as the library
      else    { r[0] = 0x00; r[1] = 0x01; r[2] = 0x40; r[3] = 0x40; }   // neutral
    }
    sendOutputReport(buf, sizeof(buf));
  }

  // The link is gone but the library never noticed: BTD handles "HCI disconnected" without
  // telling the services, so this instance would stay "connected" forever and refuse the
  // Joy-Con when it reconnects. Drop the (possibly dead) link and free the instance.
  void forget()
  {
    pBtd->hci_disconnect(hci_handle);
    BTHID::Reset();
  }

  // Subcommand 0x08 00: leave "shipment low power" mode. In that mode a sleeping Joy-Con
  // does not wake up and call its host when a button is pressed. The Switch sends this
  // after every connection (dekuNukem, Nintendo_Switch_Reverse_Engineering).
  void allowButtonWake()
  {
    const uint8_t arg = 0x00;
    sendSubcommand(0x08, &arg, 1);
  }

  // A Joy-Con ignores rumble until vibration is enabled with subcommand 0x48.
  void enableVibration()
  {
    const uint8_t arg = 0x01;
    sendSubcommand(0x48, &arg, 1);
  }

  void requestFullReport()
  {
    const uint8_t arg = 0x30;         // full report
    sendSubcommand(0x03, &arg, 1);    // set input report mode
  }

  // Read up to 0x1D bytes of the controller's SPI flash. The answer arrives later as a
  // 0x21 report; collect it with spiReplyReady().
  void readSpi(uint16_t addr, uint8_t len)
  {
    const uint8_t arg[5] = { (uint8_t)addr, (uint8_t)(addr >> 8), 0, 0, len };
    _spiReady = false;
    sendSubcommand(0x10, arg, sizeof(arg));
  }

  // True once, after the reply to readSpi(addr, ...) arrived. Copies its first 10 bytes.
  bool spiReplyReady(uint16_t addr, uint8_t out[10])
  {
    if (!_spiReady || _spiAddr != addr) return false;
    memcpy(out, _spiData, 10);
    _spiReady = false;
    return true;
  }

  // Print every SPI reply to Serial, for the bench test.
  bool spiEcho = false;

protected:
  virtual void ParseBTHIDData(uint8_t len, uint8_t *buf)
  {
    // 0x21 = subcommand reply. [13] ack, [14] subcommand, then its data.
    if (buf[0] == 0x21 && len > 30 && buf[14] == 0x10)
    {
      _spiAddr = buf[15] | (buf[16] << 8);
      memcpy(_spiData, &buf[20], 10);
      _spiReady = true;
    }
    if (spiEcho && buf[0] == 0x21 && len > 20 && buf[14] == 0x10)
    {
      Serial.print(F("SPI 0x"));
      Serial.print(buf[15] | (buf[16] << 8), HEX);
      Serial.print(F(":"));
      for (uint8_t i = 0; i < buf[19] && 20 + i < len; i++)
      {
        Serial.print(' ');
        if (buf[20 + i] < 0x10) Serial.print('0');
        Serial.print(buf[20 + i], HEX);
      }
      Serial.println();
    }
    SwitchProBT::ParseBTHIDData(len, buf);
  }

private:
  void sendSubcommand(uint8_t id, const uint8_t* data, uint8_t len)
  {
    uint8_t buf[11 + 8] = { 0 };
    buf[0] = 0x01;                    // rumble + subcommand
    buf[1] = _seq++ & 0x0F;
    buf[2] = 0x00; buf[3] = 0x01; buf[4] = 0x40; buf[5] = 0x40;   // neutral rumble, left
    buf[6] = 0x00; buf[7] = 0x01; buf[8] = 0x40; buf[9] = 0x40;   // neutral rumble, right
    buf[10] = id;
    if (len > 8) len = 8;
    memcpy(&buf[11], data, len);
    sendOutputReport(buf, 11 + len);
  }

  uint8_t  _seq = 0;
  bool     _spiReady = false;
  uint16_t _spiAddr = 0;
  uint8_t  _spiData[10];
};
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
    _armed = false;                        // the stick drives again only once seen centred
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
      _warned = false;
      _gotFullReport = false;
      _lastModeRequest = 0;
      _sideKnown = false;
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
    const bool newReport = (counter != _lastCounter);
    if (newReport)
    {
      _lastCounter = counter;
      _lastMessage = millis();
      _gotFullReport = true;
    }
    else if (!_gotFullReport && millis() - _lastModeRequest > 250)
    {
      _lastModeRequest = millis();
      _joy.requestFullReport();
    }
    else if (_gotFullReport && millis() - _lastMessage > LINK_LOST_MS)
    {
      // A Joy-Con in full mode reports 60 times a second, so this silence means it is gone
      // (sync pressed, out of range, battery). Free the instance so it can reconnect.
      _joy.forget();
      setStick(0, 0);
      setButtons(0);
      return;
    }

    // ---- stick ---------------------------------------------------------------------------
    // Only one stick is populated on a single Joy-Con. The parser returns Y with forward
    // NEGATIVE (2048 - raw, "the same coordinate as the simple report"), despite what its
    // comment suggests. Below, Y is first turned to forward positive, so the sideways
    // rotation works in ordinary x/y terms, then back to forward negative for this layer,
    // like the Nav reports it: stick pushed away from you reads negative.
    // Which half connected to this slot depends on who reconnects first, not on the slot.
    // A half reports the other half's stick as all zeros, which the parser turns into
    // exactly (-2048, 2048), so the first full report tells us which one this is.
    if (newReport && !_sideKnown)
    {
      _sideKnown = true;
      const bool leftStickEmpty = _joy.getAnalogHat(LeftHatX) == -2048 &&
                                  _joy.getAnalogHat(LeftHatY) ==  2048;
      const JoyConSide detected = leftStickEmpty ? JOYCON_RIGHT : JOYCON_LEFT;
      if (detected != _side)
      {
        _side = detected;
        _map = (_side == JOYCON_LEFT) ? JOYCON_MAP_LEFT : JOYCON_MAP_RIGHT;
      }
      _ledAnimUntil = millis() + LED_ANIM_MS;   // "connected" signal, see below
      _joy.enableVibration();                    // off by default; buzz() needs it
      _lastLedStep = millis();             // first LED step 200 ms later: subcommands need room
      _wakeSent = false;
      recalibrate();                       // centre must be measured on the right stick
      setStick(0, 0);
      setButtons(0);
      return;
    }

    // ---- connection signal -----------------------------------------------------------------
    // For a couple of seconds after connecting, alternate the outer and inner player LEDs so
    // you can see which Joy-Con just came up, then settle on its player LED.
    if (_ledAnimUntil)
    {
      const uint32_t t = millis();
      if ((int32_t)(t - _ledAnimUntil) >= 0)
      {
        _ledAnimUntil = 0;
        _joy.setLedRaw(_waiting ? 0xF0 : currentLed());
      }
      else if (t - _lastLedStep >= 200)
      {
        _lastLedStep = t;
        if (!_wakeSent) { _wakeSent = true; _joy.allowButtonWake(); }   // takes one LED step
        else
        {
          _ledOuter = !_ledOuter;
          _joy.setLedRaw(_ledOuter ? 0x09 : 0x06);   // LED1+LED4, then LED2+LED3
        }
      }
    }

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
      // Sample once per full report, not once per loop: loop() runs thousands of times a
      // second, and until the first 0x30 report arrives the parser holds its 2048 defaults.
      if (!newReport) { setStick(0, 0); }
      else {
      _calSumX += rawX;
      _calSumY += rawY;
      if (++_calSamples >= CAL_SAMPLES)
      {
        const int16_t cx = _calSumX / CAL_SAMPLES;
        const int16_t cy = _calSumY / CAL_SAMPLES;
        if (abs(cx) > CENTRE_MAX_FROM_NOMINAL || abs(cy) > CENTRE_MAX_FROM_NOMINAL)
        {
          // No healthy stick rests that far out: it is being held, or it is worn. Taking
          // this as the centre would hide a stick that may jump back. Measure again.
          warnOnce(F(": stick rests far from centre (held, or drift). Stick off until it centres."));
          recalibrate();
        }
        else
        {
          _centreX = cx;
          _centreY = cy;
          _calibrating = false;
        }
      }
      setStick(0, 0);       // report centred while measuring, never a stale value
      }
    }
    else
    {
      // Joy-Con sticks often do not spring back to where they started (drift). If the stick
      // sits near the centre and does not move at all for a while, nobody is holding it:
      // take that spot as the new centre. A thumb is never that still.
      if (newReport) trackDrift(rawX, rawY);
      if (newReport) checkArmed(rawX - _centreX, rawY - _centreY, rawX, rawY);

      int16_t x = _armed ? rawX - _centreX : 0;
      int16_t y = _armed ? _centreY - rawY : 0;   // forward positive from here on

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

  bool isConnected() const override { return const_cast<JoyConBT&>(_joy).connected(); }

  void disconnect() override { _joy.disconnect(); }

  uint32_t millisSinceLastMessage() const override
  {
    const uint32_t now = millis();
    return (now >= _lastMessage) ? (now - _lastMessage) : 0;
  }

  // Once connected, name the half that is actually there rather than the slot.
  const char* name() const override
  {
    if (!_sideKnown) return _label;
    return _side == JOYCON_LEFT ? "JOY-L" : "JOY-R";
  }

  JoyConSide side() const { return _side; }

  // Left half drives the feet, right half the dome, whichever slot it connected into.
  DroidSlot preferredSlot() const override
  {
    if (!_sideKnown || !isConnected()) return SLOT_ANY;
    return _side == JOYCON_LEFT ? SLOT_FOOT : SLOT_DOME;
  }

  // ---- extras a Nav cannot do ------------------------------------------------------------

  // 0 to 4, where 0 is critical. Worth printing at startup and before an event.
  uint8_t batteryLevel() { return _joy.getBatteryLevel(); }

  // A short buzz. Useful as confirmation when a mode changes, for instance entering voice
  // mode, which on a Nav you can only tell by listening to the droid.
  void buzz(bool on) { if (isConnected()) _joy.rumbleNow(on); }

  // While pairing: flash all four player LEDs, so the connected half shows the droid is
  // waiting for the other one. Off: back to its player LED.
  void setWaiting(bool on)
  {
    _waiting = on;
    if (isConnected() && !_ledAnimUntil) _joy.setLedRaw(on ? 0xF0 : currentLed());
  }

  // Player LEDs to show once connected, instead of this half's default (LED 1 left, LED 2
  // right). Low nibble on, high nibble flashing, as subcommand 0x30 takes them. 0 goes back
  // to the default. Kept across reconnections.
  void setPlayerLeds(uint8_t pattern)
  {
    _ledOverride = pattern;
    if (isConnected() && !_ledAnimUntil && !_waiting) _joy.setLedRaw(currentLed());
  }

  bool isCalibrating() const { return _calibrating; }

  // False while the stick is ignored for safety (not yet seen centred, or frozen off centre).
  bool isStickArmed() const { return _armed; }

  // Escape hatch for anything this wrapper does not cover yet.
  JoyConBT* raw() { return &_joy; }

private:
  static const uint8_t CAL_SAMPLES = 30;   // about half a second at 60 Hz

  bool held(ButtonEnum b)
  {
    if ((uint8_t)b == 0xFF) return false;  // role deliberately unmapped
    return _joy.getButtonPress(b);
  }

  void trackDrift(int16_t rawX, int16_t rawY)
  {
    const uint32_t now = millis();
    // Near the centre we measured, or near the stick's nominal centre. The second one also
    // repairs a centre measured while someone was touching the stick, for instance while
    // holding the sync button during pairing.
    const bool nearCentre = (abs(rawX - _centreX) < DRIFT_MAX_OFFSET &&
                             abs(rawY - _centreY) < DRIFT_MAX_OFFSET) ||
                            (abs(rawX) < DRIFT_NOMINAL_OFFSET && abs(rawY) < DRIFT_NOMINAL_OFFSET);
    const bool still = abs(rawX - _stillX) <= DRIFT_STILL_NOISE &&
                       abs(rawY - _stillY) <= DRIFT_STILL_NOISE;
    if (!nearCentre || !still)
    {
      _stillX = rawX;
      _stillY = rawY;
      _stillSince = now;
      return;
    }
    if (now - _stillSince >= DRIFT_STILL_MS)
    {
      _centreX = _stillX;
      _centreY = _stillY;
      _stillSince = now;
    }
  }

  // dx, dy: offset from the measured centre. Arms the stick once it is seen centred; disarms
  // it if it freezes off centre for STUCK_MS, short of the edge. Holding full deflection
  // against the gate is steady too, which is why the edge does not count.
  void checkArmed(int16_t dx, int16_t dy, int16_t rawX, int16_t rawY)
  {
#ifdef JOYCON_DEBUG_STICK
    static uint32_t dbgAt = 0;
    if (millis() - dbgAt >= 100)
    {
      dbgAt = millis();
      Serial.print(F("STK ")); Serial.print(name());
      Serial.print(' '); Serial.print(dx); Serial.print(' '); Serial.print(dy);
      Serial.print(F(" armed=")); Serial.println(_armed);
    }
#endif
    const bool centred = abs(dx) < ARM_BAND && abs(dy) < ARM_BAND;
    if (!_armed)
    {
      if (centred)
      {
        _armed = true;
        if (_warned) { Serial.print(name()); Serial.println(F(": stick centred, driving again.")); _warned = false; }
      }
      return;
    }
    const int16_t edge = (int16_t)((int32_t)_range * 85 / 100);
    const bool atEdge = abs(dx) > edge || abs(dy) > edge;
    const bool still = abs(rawX - _stuckX) <= DRIFT_STILL_NOISE &&
                       abs(rawY - _stuckY) <= DRIFT_STILL_NOISE;
    const uint32_t now = millis();
    if (centred || atEdge || !still)
    {
      _stuckX = rawX;
      _stuckY = rawY;
      _stuckSince = now;
      return;
    }
    if (now - _stuckSince >= STUCK_MS)
    {
      _armed = false;
      warnOnce(F(": stick frozen off centre (drift?). Stick off until it centres."));
    }
  }

  void warnOnce(const __FlashStringHelper* msg)
  {
    if (_warned) return;
    _warned = true;
    Serial.print(name());
    Serial.println(msg);
  }

  int16_t scale(int32_t v) const
  {
    int32_t out = (v * (int32_t)DC_STICK_MAX) / _range;
    if (out >  DC_STICK_MAX) out =  DC_STICK_MAX;
    if (out < -DC_STICK_MAX) out = -DC_STICK_MAX;
    return (int16_t)out;
  }

  JoyConBT     _joy;
  JoyConSide   _side;
  const char*  _label;
  JoyConMapping _map;
  JoyConGrip   _grip = JOYCON_UPRIGHT;
  int16_t      _range = 1000;

  bool     _wasConnected = false;
  uint32_t _lastMessage = 0;
  uint16_t _lastCounter = 0;

  bool     _calibrating = false;
  uint8_t  _calSamples = 0;
  int32_t  _calSumX = 0;
  int32_t  _calSumY = 0;
  bool     _gotFullReport = false;
  bool     _sideKnown = false;
  int16_t  _stillX = 0;
  int16_t  _stillY = 0;
  uint32_t _stillSince = 0;
  static const int16_t  DRIFT_MAX_OFFSET  = 250;   // raw, about 25% of the stick range
  static const int16_t  DRIFT_NOMINAL_OFFSET = 400; // raw, from 2048; real rest points seen: up to ~310
  static const int16_t  DRIFT_STILL_NOISE = 12;    // raw jitter of a stick nobody touches
  static const uint16_t DRIFT_STILL_MS    = 1500;
  // Safety (see the top of the file)
  bool     _armed = false;
  bool     _warned = false;
  int16_t  _stuckX = 0;
  int16_t  _stuckY = 0;
  uint32_t _stuckSince = 0;
  static const int16_t  ARM_BAND = 80;                 // raw, about 8% of the range: "centred"
  static const int16_t  CENTRE_MAX_FROM_NOMINAL = 400; // raw; healthy rest points seen: up to ~310
  static const uint16_t STUCK_MS = 2000;               // a thumb never holds this still for 2 s
  bool     _ledOuter = false;
  uint32_t _ledAnimUntil = 0;
  bool     _waiting = false;
  bool     _wakeSent = true;
  uint8_t  _ledOverride = 0;
  uint8_t  playerLed() const { return _side == JOYCON_LEFT ? 0x01 : 0x02; }
  uint8_t  currentLed() const { return _ledOverride ? _ledOverride : playerLed(); }
  uint32_t _lastLedStep = 0;
  static const uint16_t LED_ANIM_MS = 2000;
  static const uint16_t LINK_LOST_MS = 3000;
  uint32_t _lastModeRequest = 0;
  int16_t  _centreX = 0;
  int16_t  _centreY = 0;
};

#endif // JOYCON_CONTROLLER_H
