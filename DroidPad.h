// ---------------------------------------------------------------------------------------
// DroidPad.h
//
// Compatibility shim: it speaks the old PS3BT API that Shadow MD already calls everywhere,
// and answers using the DroidController abstraction underneath.
//
// Why a shim instead of rewriting the sketch? Because Shadow MD calls the controller 161
// times across the file. Rewriting all of those in one go gives you a diff nobody can
// review and a droid nobody dares to take to an event. With this class, the sketch changes
// in five places and every one of those 161 calls keeps working, unchanged, whether there
// is a PS3 Nav or a Joy-Con in your hand.
//
// What the sketch actually uses, and therefore all this has to provide:
//
//   PS3NavigationConnected     getButtonPress()      getAnalogHat()
//   getLastMessageTime()       getStatus()           disconnect()
//   setLedOn() / setLedOff()   attachOnInit()
//
// ---------------------------------------------------------------------------------------

#ifndef DROID_PAD_H
#define DROID_PAD_H

#include "ControllerConfig.h"
#include "DroidController.h"
#include "PS3NavController.h"

// Only pulled in for Joy-Con builds. It needs SwitchProBT, which only exists in USB Host
// Shield 1.7.0, and PS3 Navs do not work with 1.7.0. So a PS3 Nav build has to keep
// compiling against the older library, with no trace of the Joy-Con code in it.
#ifdef USE_JOYCON
  #include "JoyConController.h"
#endif

class DroidPad
{
public:
  DroidPad(DroidController* impl, PS3NavController* nav = NULL)
    : PS3NavigationConnected(false), PS3Connected(false), _impl(impl), _nav(nav) {}

  void begin() { _impl->begin(); }

  // Call once per loop, right after Usb.Task(). This is the ONE line the main loop gains.
  void update()
  {
    _impl->update();
    const bool now = _impl->isConnected();

    if (now && !PS3NavigationConnected && _onInit)
      _onInit();                       // same moment PS3BT would have fired attachOnInit

    PS3NavigationConnected = now;

    // Shadow checks "Navigation or full DualShock". Only a real PS3BT can answer the
    // second half, and on anything else it is simply false.
    PS3Connected = _nav ? _nav->raw()->PS3Connected : false;
  }

  // ---- the old API, as the sketch knows it --------------------------------------------

  // Public members, not methods, exactly like PS3BT. Refreshed by update().
  bool PS3NavigationConnected;
  bool PS3Connected;

  bool getButtonPress(ButtonEnum b) { return _impl->pressed(role(b)); }
  bool getButtonClick(ButtonEnum b) { return _impl->clicked(role(b)); }

  // Returns the old 0..255 scale with 128 at centre, so every map() call in the drive code
  // keeps working with the numbers it was tuned for.
  uint8_t getAnalogHat(AnalogHatEnum a)
  {
    return (a == LeftHatX || a == RightHatX) ? _impl->legacyX() : _impl->legacyY();
  }

  uint32_t getLastMessageTime() { return millis() - _impl->millisSinceLastMessage(); }

  // Shadow uses this to spot a controller that is connected but sending nothing useful.
  // On a Nav the real thing is asked. On anything else, connected means good data: the
  // watchdog in millisSinceLastMessage() is what actually catches a dead link.
  bool getStatus(StatusEnum s)
  {
    if (_nav) return _nav->raw()->getStatus(s);
    if (s == Plugged)   return _impl->isConnected();
    if (s == Unplugged) return !_impl->isConnected();
    return false;
  }

  void disconnect() { _impl->disconnect(); }

  // Player LEDs. The Nav and the Joy-Con both have them, so they are handled by each
  // implementation on connection. Kept here so the sketch's calls still compile.
  void setLedOn(LEDEnum)  {}
  void setLedOff(LEDEnum) {}

  void attachOnInit(void (*f)(void)) { _onInit = f; }

  // Escape hatch for anything controller specific that is left.
  DroidController* controller() { return _impl; }

private:
  static uint16_t role(ButtonEnum b)
  {
    switch ((uint8_t)b)
    {
      case UP:     return DB_UP;
      case DOWN:   return DB_DOWN;
      case LEFT:   return DB_LEFT;
      case RIGHT:  return DB_RIGHT;
      case CROSS:  return DB_A;
      case CIRCLE: return DB_B;
      case L1:     return DB_L1;
      case L2:     return DB_L2;
      case L3:     return DB_STICK;
      case PS:     return DB_SYS;
      default:     return 0;      // never pressed
    }
  }

  DroidController*  _impl;
  PS3NavController* _nav;            // non NULL only when the implementation is a real Nav
  void (*_onInit)(void) = NULL;
};

#endif // DROID_PAD_H
