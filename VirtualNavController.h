// ROS2D2, created by NandoMadrid (https://www.astromechspain.com/droides/r2-d2-nandomadrid/) · Astromech Spain (https://www.astromechspain.com)
// ---------------------------------------------------------------------------------------
// VirtualNavController.h
//
// A PS3 Nav driven by the ROS2D2 web console (ROS:PAD lines, see VirtualPad.h), as one more
// DroidController: the sketch cannot tell it from a real one. It lets everything go when the
// console goes quiet and counts as switched off soon after, so losing the browser stops R2.
// Normally used through PreferRealController, so a real Nav in use always wins.
// ---------------------------------------------------------------------------------------

#ifndef VIRTUAL_NAV_CONTROLLER_H
#define VIRTUAL_NAV_CONTROLLER_H

#include "DroidController.h"
#include "VirtualPad.h"

class VirtualNavController : public DroidController
{
public:
  explicit VirtualNavController(VirtualPadState& state) : _s(state), _connected(false) {}

  void begin() override {}

  void update() override
  {
    const uint32_t now = millis();
    rememberPrevious();
    _connected = _s.connected(now);
    setButtons(_s.buttons(now));
    // 0..255 with 128 at centre, onto the layer's +/- DC_STICK_MAX (the inverse of legacyX())
    setStick((int16_t)(((int)_s.x(now) - 128) * (DC_STICK_MAX / 128)),
             (int16_t)(((int)_s.y(now) - 128) * (DC_STICK_MAX / 128)));
  }

  bool isConnected() const override { return _connected; }

  void disconnect() override { _s.drop(); _connected = false; }

  uint32_t millisSinceLastMessage() const override { return millis() - _s.lastMs(); }

  const char* name() const override { return "virtual Nav"; }

  bool isVirtual() const override { return true; }

  const VirtualPadState& state() const { return _s; }

private:
  VirtualPadState& _s;
  bool _connected;
};

#endif
