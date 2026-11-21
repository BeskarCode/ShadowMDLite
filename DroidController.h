// ---------------------------------------------------------------------------------------
// DroidController.h
//
// Transport independent controller interface for ShadowMD Lite.
//
// The point of this layer is that the droid logic should not care WHICH controller is
// in your hand. It asks for "is the stick pushed forward" and "is the UP button down",
// not for "PS3NavFoot->getAnalogHat(LeftHatY)". Once the sketch talks to this interface,
// adding a new controller (Joy-Con, Wii Nunchuk, a custom BLE pad) means writing one new
// implementation of DroidController, and nothing else.
//
// This file is standalone: it does not include, modify or depend on ShadowMDLite.ino.
// ---------------------------------------------------------------------------------------

#ifndef DROID_CONTROLLER_H
#define DROID_CONTROLLER_H

#include <Arduino.h>

// ---------------------------------------------------------------------------------------
// Buttons
//
// These are ROLES, not the labels printed on any particular controller. A PS3 Nav reports
// CROSS and CIRCLE; a Joy-Con reports A/B or down/right depending on which half you hold.
// Each implementation decides which physical button fills which role, so the droid logic
// stays the same.
// ---------------------------------------------------------------------------------------
enum DroidButton : uint16_t
{
  DB_UP     = 1 << 0,   // d-pad up
  DB_DOWN   = 1 << 1,   // d-pad down
  DB_LEFT   = 1 << 2,   // d-pad left
  DB_RIGHT  = 1 << 3,   // d-pad right
  DB_A      = 1 << 4,   // PS3 Nav: CROSS
  DB_B      = 1 << 5,   // PS3 Nav: CIRCLE
  DB_L1     = 1 << 6,   // PS3 Nav: L1          (first shoulder / modifier)
  DB_L2     = 1 << 7,   // PS3 Nav: L2          (second shoulder / dead man)
  DB_STICK  = 1 << 8,   // PS3 Nav: L3          (stick click)
  DB_SYS    = 1 << 9    // PS3 Nav: PS          (system / modifier)
};

// Analog stick range used across the whole layer.
// Centre is 0, full deflection is +/- DC_STICK_MAX. A PS3 Nav only really has 8 bits of
// resolution, a Joy-Con has 12, so the common range is deliberately wider than the Nav's
// to avoid throwing away resolution on controllers that have it.
static const int16_t DC_STICK_MAX = 512;

// Which job a controller wants. A Joy-Con knows which half it is, so it can ask for the
// feet or the dome. A Nav looks the same either way and stays in the slot it was given.
enum DroidSlot : uint8_t
{
  SLOT_ANY  = 0,
  SLOT_FOOT = 1,
  SLOT_DOME = 2
};

class DroidController
{
public:
  virtual ~DroidController() {}

  // Called once from setup(). Registers callbacks, nothing more.
  virtual void begin() = 0;

  // Called once per loop(), AFTER Usb.Task(). Refreshes the cached state below.
  virtual void update() = 0;

  // Is a controller actually talking to us right now.
  virtual bool isConnected() const = 0;

  // Drop the link on purpose (low battery shutdown, mode change, etc).
  virtual void disconnect() = 0;

  // Milliseconds since the last packet arrived. The droid logic uses this as a watchdog:
  // if this grows, stop the motors. ShadowMD Lite already does exactly this with
  // getLastMessageTime(), so the semantics are intentionally the same.
  virtual uint32_t millisSinceLastMessage() const = 0;

  // ---- analog stick, normalised -------------------------------------------------------
  // x: left negative, right positive.
  // y: forward (stick pushed away from you) negative, back positive. This matches the raw
  //    PS3 Nav convention, where the rest position is 128 and pushing forward goes down
  //    towards 0, so no existing behaviour changes when porting.
  int16_t x() const { return _x; }
  int16_t y() const { return _y; }

  // Same stick, expressed on the old 0..255 PS3 scale with 128 at centre.
  // This exists so the existing drive code in ShadowMDLite.ino can be moved over to this
  // layer one function at a time, without rewriting every map() call in the same commit.
  uint8_t legacyX() const { return _toLegacy(_x); }
  uint8_t legacyY() const { return _toLegacy(_y); }

  // ---- buttons ------------------------------------------------------------------------
  // Held down right now.
  bool pressed(uint16_t mask) const { return (_buttons & mask) == mask; }

  // Went down since the previous update(). Edge detection lives here, in the layer, so it
  // behaves identically no matter what the underlying library offers.
  bool clicked(uint16_t mask) const { return ((_buttons & ~_prevButtons) & mask) == mask; }

  // Released since the previous update().
  bool released(uint16_t mask) const { return ((_prevButtons & ~_buttons) & mask) == mask; }

  // Raw masks, for logging and for "is anything at all pressed" checks.
  uint16_t buttons() const { return _buttons; }
  bool anyPressed() const { return _buttons != 0; }

  // ---- helpers ------------------------------------------------------------------------
  // Returns 0 inside the dead zone, the original value outside it. Dead zone is given on
  // the same normalised scale as x() and y().
  static int16_t deadzone(int16_t value, int16_t range)
  {
    return (abs(value) < range) ? 0 : value;
  }

  // Convert a dead zone expressed on the old 0..255 scale (the joystickFootDeadZoneRange
  // and joystickDomeDeadZoneRange values in ShadowMDLite.ino) to the normalised scale.
  static int16_t legacyDeadzone(uint8_t legacyRange)
  {
    return (int16_t)legacyRange * (DC_STICK_MAX / 128);
  }

  // Short human readable name, for console output.
  virtual const char* name() const = 0;

  // The slot this controller belongs in, once it knows. SLOT_ANY keeps it where it is.
  virtual DroidSlot preferredSlot() const { return SLOT_ANY; }

protected:
  // Implementations fill these in during update() and call rememberPrevious() first.
  void rememberPrevious() { _prevButtons = _buttons; }
  void setStick(int16_t x, int16_t y) { _x = x; _y = y; }
  void setButtons(uint16_t b) { _buttons = b; }
  void clearState() { _x = 0; _y = 0; _buttons = 0; _prevButtons = 0; }

private:
  static uint8_t _toLegacy(int16_t v)
  {
    long scaled = 128L + ((long)v * 128L) / DC_STICK_MAX;
    if (scaled < 0)   scaled = 0;
    if (scaled > 255) scaled = 255;
    return (uint8_t)scaled;
  }

  int16_t  _x = 0;
  int16_t  _y = 0;
  uint16_t _buttons = 0;
  uint16_t _prevButtons = 0;
};

// Put each controller in the slot it asks for, whatever order they connected in. Call it
// after both have been updated. Returns true when it swapped them.
//
// It only swaps when that does not take a controller out of a slot it wants, so two halves
// of the same side stay where they are rather than swapping back and forth every loop.
inline bool routeBySlot(DroidController*& foot, DroidController*& dome)
{
  const DroidSlot f = foot->preferredSlot();
  const DroidSlot d = dome->preferredSlot();
  if ((f == SLOT_DOME && d != SLOT_DOME) || (d == SLOT_FOOT && f != SLOT_FOOT))
  {
    DroidController* t = foot;
    foot = dome;
    dome = t;
    return true;
  }
  return false;
}

#endif // DROID_CONTROLLER_H
