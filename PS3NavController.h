// ---------------------------------------------------------------------------------------
// PS3NavController.h
//
// DroidController implementation for the PS3 Move Navigation controller, built on top of
// the PS3BT class from the USB Host Shield 2.0 library. This is the reference
// implementation: it must behave exactly like the current ShadowMD Lite code, so that
// moving the sketch onto this layer changes nothing the droid actually does.
//
// Nothing here is new behaviour. It is the same calls the sketch already makes, gathered
// in one place behind the common interface.
// ---------------------------------------------------------------------------------------

#ifndef PS3_NAV_CONTROLLER_H
#define PS3_NAV_CONTROLLER_H

#include <PS3BT.h>
#include "DroidController.h"

class PS3NavController : public DroidController
{
public:
  // led: which player LED to light on connection (LED1 for foot, LED2 for dome), matching
  // what onInitPS3NavFoot() and onInitPS3NavDome() do today.
  PS3NavController(BTD* btd, const char* label, LEDEnum led)
    : _ps3(btd), _label(label), _led(led) {}

  void begin() override
  {
    // The PS3BT attachOnInit callback is a plain function pointer with no user data, so an
    // instance method cannot be registered directly. Connection is detected in update()
    // instead, which is simpler and needs no static trampolines.
    clearState();
    _wasConnected = false;
  }

  void update() override
  {
    rememberPrevious();

    const bool now = _ps3.PS3NavigationConnected;

    if (now && !_wasConnected)
    {
      // Fresh connection: light the player LED, same as the current sketch does.
      _ps3.setLedOn(_led);
      _connectedAt = millis();
    }
    _wasConnected = now;

    if (!now)
    {
      setStick(0, 0);
      setButtons(0);
      return;
    }

    // ---- stick ------------------------------------------------------------------------
    // The Nav reports 0..255 with 128 at rest on both axes.
    const int16_t rawX = (int16_t)_ps3.getAnalogHat(LeftHatX) - 128;
    const int16_t rawY = (int16_t)_ps3.getAnalogHat(LeftHatY) - 128;
    setStick(rawX * (DC_STICK_MAX / 128), rawY * (DC_STICK_MAX / 128));

    // ---- buttons ------------------------------------------------------------------------
    uint16_t b = 0;
    if (_ps3.getButtonPress(UP))     b |= DB_UP;
    if (_ps3.getButtonPress(DOWN))   b |= DB_DOWN;
    if (_ps3.getButtonPress(LEFT))   b |= DB_LEFT;
    if (_ps3.getButtonPress(RIGHT))  b |= DB_RIGHT;
    if (_ps3.getButtonPress(CROSS))  b |= DB_A;
    if (_ps3.getButtonPress(CIRCLE)) b |= DB_B;
    if (_ps3.getButtonPress(L1))     b |= DB_L1;
    if (_ps3.getButtonPress(L2))     b |= DB_L2;
    if (_ps3.getButtonPress(L3))     b |= DB_STICK;
    if (_ps3.getButtonPress(PS))     b |= DB_SYS;
    setButtons(b);
  }

  bool isConnected() const override { return _ps3.PS3NavigationConnected; }

  void disconnect() override { _ps3.disconnect(); }

  uint32_t millisSinceLastMessage() const override
  {
    const uint32_t last = _ps3.getLastMessageTime();
    const uint32_t now  = millis();
    return (now >= last) ? (now - last) : 0;   // guard against the millis() rollover
  }

  const char* name() const override { return _label; }

  // Escape hatch. While the sketch is only half ported, some code may still need the raw
  // PS3BT object (battery level, the Bluetooth address, setLedOn, rumble). Anything that
  // uses this is, by definition, not portable to another controller yet.
  PS3BT* raw() { return &_ps3; }

private:
  PS3BT       _ps3;
  const char* _label;
  LEDEnum     _led;
  bool        _wasConnected = false;
  uint32_t    _connectedAt = 0;
};

#endif // PS3_NAV_CONTROLLER_H
