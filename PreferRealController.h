// ROS2D2, created by NandoMadrid (https://www.astromechspain.com/droides/r2-d2-nandomadrid/) · Astromech Spain (https://www.astromechspain.com)
// ---------------------------------------------------------------------------------------
// PreferRealController.h
//
// One Nav slot (foot or dome) fed by a real controller and by the console's virtual Nav. The
// real one wins while it is in use (PadArbiter in VirtualPad.h); otherwise the virtual one
// drives. The sketch sees one controller through DroidPad, as always.
// ---------------------------------------------------------------------------------------

#ifndef PREFER_REAL_CONTROLLER_H
#define PREFER_REAL_CONTROLLER_H

#include "DroidController.h"
#include "VirtualNavController.h"
#include "VirtualPad.h"

class PreferRealController : public DroidController
{
public:
  // legacyDeadZone: the sketch's joystick dead zone on the old 0..255 scale (a real stick
  // inside it does not count as "in use")
  PreferRealController(DroidController* real, VirtualNavController* virt, PadArbiter& arb, uint8_t legacyDeadZone)
    : _real(real), _virt(virt), _arb(arb), _dz(DroidController::legacyDeadzone(legacyDeadZone)), _virtualWins(false) {}

  void begin() override { _real->begin(); _virt->begin(); }

  void update() override
  {
    _real->update();
    _virt->update();
    const bool moving = _real->anyPressed() || abs(_real->x()) > _dz || abs(_real->y()) > _dz;
    _arb.update(millis(), _real->isConnected(), moving, _virt->isConnected(), _virt->state().atRest());
    _virtualWins = _arb.virtualWins();

    rememberPrevious();
    DroidController* w = winner();
    if (w)
    {
      setButtons(w->buttons());
      setStick(w->x(), w->y());
    }
    else
    {
      setButtons(0);              // a virtual press cut off by the real Nav, waiting for rest
      setStick(0, 0);
    }
  }

  bool isConnected() const override { return _real->isConnected() || _virt->isConnected(); }

  // Only the one driving: a real Nav that is not there must not be told anything (PS3BT would
  // try to send on Bluetooth and print an L2CAP error)
  void disconnect() override
  {
    if (_virtualWins || !_real->isConnected()) _virt->disconnect();
    else _real->disconnect();
  }

  uint32_t millisSinceLastMessage() const override
  {
    if (_virtualWins) return _virt->millisSinceLastMessage();
    if (_real->isConnected()) return _real->millisSinceLastMessage();
    return _virt->millisSinceLastMessage();
  }

  const char* name() const override { return _virtualWins ? _virt->name() : _real->name(); }

  DroidSlot preferredSlot() const override { return _real->preferredSlot(); }

  bool isVirtual() const override { return _virtualWins; }

  // For the "PAD F:... D:..." report: real | virtual | none
  const char* who() const
  {
    if (_real->isConnected()) return "real";
    return _virt->isConnected() ? "virtual" : "none";
  }

private:
  DroidController* winner() const
  {
    if (_virtualWins) return _virt;
    return _real->isConnected() ? _real : NULL;
  }

  DroidController*     _real;
  VirtualNavController* _virt;
  PadArbiter&          _arb;
  int16_t              _dz;
  bool                 _virtualWins;
};

#endif
