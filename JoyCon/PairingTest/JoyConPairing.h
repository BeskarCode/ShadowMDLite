// ---------------------------------------------------------------------------------------
// JoyConPairing.h
//
// Pairing Joy-Cons without editing or re-uploading the sketch.
//
//   - First boot: no Joy-Con recorded in EEPROM, so pairing starts by itself. The second
//     (dome) Joy-Con is optional: one paired is enough to finish when the time runs out.
//   - Re-pair: hold Minus + Capture (left) or Plus + Home (right) for 5 s on a connected
//     Joy-Con, or reset the Mega three times within 10 s of each other, or (ShadowMD Lite)
//     send JOYPAIR from the web console.
//   - A session lasts at most 60 s. The droid must not move while it runs; the sketch asks
//     active() and stops the motors.
//   - The sketch is told what happens through one callback, so it can play R2 sounds.
//
// Same file at the root of ShadowMD Lite, in JoyCon/BenchTest, JoyCon/PairingTest and in
// the Shadow MD fork. Keep them identical.
// ---------------------------------------------------------------------------------------

#ifndef JOYCON_PAIRING_H
#define JOYCON_PAIRING_H

#include <Arduino.h>

#ifndef JOYCON_COMBO_HOLD_MS
  #define JOYCON_COMBO_HOLD_MS          5000UL
#endif
#ifndef JOYCON_PAIR_TIMEOUT_MS
  #define JOYCON_PAIR_TIMEOUT_MS        60000UL
#endif
#ifndef JOYCON_PAIR_REARM_MS
  #define JOYCON_PAIR_REARM_MS          15000UL
#endif
#ifndef JOYCON_TRIPLE_RESET_WINDOW_MS
  #define JOYCON_TRIPLE_RESET_WINDOW_MS 10000UL
#endif
#ifndef JOYCON_EEPROM_ADDR
  #define JOYCON_EEPROM_ADDR            4000     // 3 bytes, near the end of the Mega's 4096
#endif

// ---- pure logic, no hardware ----------------------------------------------------------

// True once, when a button combination has been held without a break for
// JOYCON_COMBO_HOLD_MS. Releasing it starts again from zero.
struct HoldTimer
{
  void reset() { _holding = false; _fired = false; _start = 0; _now = 0; }

  bool update(bool held, uint32_t now)
  {
    if (!held) { reset(); return false; }
    if (!_holding) { _holding = true; _fired = false; _start = now; }
    _now = now;
    if (!_fired && now - _start >= JOYCON_COMBO_HOLD_MS) { _fired = true; return true; }
    return false;
  }

  // Whole seconds held so far, for a rumble tick each second.
  uint8_t ticks() const { return _holding ? (uint8_t)((_now - _start) / 1000UL) : 0; }

private:
  bool     _holding = false;
  bool     _fired = false;
  uint32_t _start = 0;
  uint32_t _now = 0;
};

// Counts boots that come within JOYCON_TRIPLE_RESET_WINDOW_MS of each other (the sketch
// clears the counter once it has been running that long). The third one asks for pairing.
inline bool jcpTripleReset(uint8_t& counter)
{
  if (counter < 3) counter++;
  if (counter >= 3) { counter = 0; return true; }
  return false;
}

// What is kept in EEPROM: which halves have been paired, and the boot counter.
struct JcpRecord
{
  static const uint8_t MAGIC = 0x4A;
  static const uint8_t LEFT  = 1;
  static const uint8_t RIGHT = 2;

  uint8_t magic;
  uint8_t sides;
  uint8_t boots;

  bool valid() const    { return magic == MAGIC; }
  bool complete() const { return (sides & (LEFT | RIGHT)) == (LEFT | RIGHT); }
};

// A Joy-Con keeps two host records in its flash, at 0x2000 and 0x2026. A new pairing goes
// into 0x2026 and the old one moves to 0x2000. If either points to another host, a Switch
// for instance, pressing a button no longer reconnects to the droid. Each record starts
// with magic, size, two checksum bytes, then the host address big-endian; my_bdaddr in
// the BTD library is little-endian.
inline bool jcpHostRecordsMatch(const uint8_t rec2000[10], const uint8_t rec2026[10],
                                const uint8_t dongle[6])
{
  for (uint8_t i = 0; i < 6; i++)
  {
    if (rec2000[4 + i] != dongle[5 - i]) return false;
    if (rec2026[4 + i] != dongle[5 - i]) return false;
  }
  return true;
}

// ---- the pairing session --------------------------------------------------------------
#ifndef JOYCON_PAIRING_LOGIC_ONLY

#include <EEPROM.h>
#include "JoyConController.h"

enum JoyConPairEvent : uint8_t
{
  JCP_STARTED,           // pairing mode on: hold sync on a Joy-Con
  JCP_PAIRED_ONE,        // one Joy-Con paired, the other one is still missing
  JCP_DONE,              // finished: both halves, or one when the time ran out
  JCP_TIMEOUT,           // 60 s without finishing, back to normal
  JCP_HOLD_SYNC_AGAIN    // this Joy-Con still remembers another host: hold sync again
};

class JoyConPairing
{
public:
  JoyConPairing(BTD* btd, JoyConController* a, JoyConController* b) : _btd(btd)
  {
    _jc[0] = a;
    _jc[1] = b;
  }

  // Call once in setup(), after the controllers' begin(). force = JOYCON_PAIRING_MODE.
  void begin(void (*onEvent)(JoyConPairEvent), bool force = false)
  {
    _onEvent = onEvent;
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    EEPROM.get(JOYCON_EEPROM_ADDR, _rec);
    if (!_rec.valid()) { _rec.magic = JcpRecord::MAGIC; _rec.sides = 0; _rec.boots = 0; }
    const bool triple = jcpTripleReset(_rec.boots);
    EEPROM.put(JOYCON_EEPROM_ADDR, _rec);

    // Only when no Joy-Con is recorded at all: the dome Joy-Con is optional, as the second
    // Nav always was, so a droid driven with one Joy-Con must not pair on every boot.
    if (triple || _rec.sides == 0 || force) request();
  }

  // Call every loop, right after DroidPad::updatePair().
  void update()
  {
    // Library quirk (USB Host Shield 1.7.0): l2capConnectionClaimed is set when an instance
    // claims a connection and only cleared when the next incoming connection completes.
    // Pairing is an outgoing connection, so after one Joy-Con has connected the flag stays
    // set and no free instance would open the HID channels for a newly paired one. Clear it
    // once, when a Joy-Con has finished connecting: its claim is no longer needed. Clearing
    // it while a connection is still being set up would let both instances grab the same
    // Joy-Con.
    for (uint8_t i = 0; i < 2; i++)
    {
      const bool c = _jc[i]->isConnected();
      if (c && !_wasConnected[i]) _btd->l2capConnectionClaimed = false;
      _wasConnected[i] = c;
    }

    const uint32_t now = millis();

    // Running for a while: this boot was not part of a triple reset.
    if (!_bootsCleared && now > JOYCON_TRIPLE_RESET_WINDOW_MS)
    {
      _bootsCleared = true;
      if (_rec.boots) { _rec.boots = 0; EEPROM.put(JOYCON_EEPROM_ADDR, _rec); }
    }

    updateBuzz(now);

    if (!_active) { checkCombos(now); return; }

    digitalWrite(LED_BUILTIN, (now / 250) & 1);   // 2 Hz while pairing

    // The library gives up searching after a while, and stops after each pairing.
    if (!_btd->pairWithHIDDevice && _verify < 0 && now - _lastArm > JOYCON_PAIR_REARM_MS)
    {
      _lastArm = now;
      _btd->pairWithHID();
    }

    if (_verify >= 0) { verifyStep(now); return; }

    for (uint8_t i = 0; i < 2; i++)
    {
      if (_jc[i]->isConnected() && !_known[i]) { startVerify(i, now); return; }
      if (!_jc[i]->isConnected()) _known[i] = false;
    }

    // Out of time. If one Joy-Con was paired in this session that is a success: the second
    // one is optional.
    if (now - _start > JOYCON_PAIR_TIMEOUT_MS) end(_pairedThisSession ? JCP_DONE : JCP_TIMEOUT);
  }

  // Start a pairing session (JOYPAIR command, combo, or from begin()).
  void request()
  {
    if (_active) return;
    _active = true;
    _pairedThisSession = false;
    _start = _lastArm = millis();
    _verify = -1;
    for (uint8_t i = 0; i < 2; i++)
    {
      _known[i] = _jc[i]->isConnected();    // already here: not a new pairing
      _hold[i].reset();
      _jc[i]->setWaiting(true);
    }
    _btd->pairWithHID();
    if (_onEvent) _onEvent(JCP_STARTED);
  }

  bool active() const { return _active; }

private:
  void end(JoyConPairEvent ev)
  {
    _active = false;
    _verify = -1;
    // Stop searching and send the library back to waiting for reconnections. hci_state is
    // private, but pairWithHID() resets it to "check device service", which falls straight
    // through to scanning when pairWithHIDDevice is false.
    _btd->hci_inquiry_cancel();
    _btd->pairWithHID();
    _btd->pairWithHIDDevice = false;
    for (uint8_t i = 0; i < 2; i++) _jc[i]->setWaiting(false);
    digitalWrite(LED_BUILTIN, LOW);
    if (_onEvent) _onEvent(ev);
  }

  // A Joy-Con connected during the session. Before counting it, check that both of its
  // host records point to this dongle (see jcpHostRecordsMatch).
  void startVerify(uint8_t i, uint32_t now)
  {
    _verify = (int8_t)i;
    _vStep = 0;
    _vSince = now;
  }

  void verifyStep(uint32_t now)
  {
    JoyConController* jc = _jc[_verify];
    if (!jc->isConnected()) { _verify = -1; return; }

    switch (_vStep)
    {
      case 0:   // wait until it sends full reports and knows its side
        if (jc->preferredSlot() == SLOT_ANY) { if (now - _vSince > 3000) accept(now); return; }
        jc->raw()->readSpi(0x2000, 10);
        _vStep = 1; _vSince = now;
        return;
      case 1:
        if (jc->raw()->spiReplyReady(0x2000, _r0))
        {
          jc->raw()->readSpi(0x2026, 10);
          _vStep = 2; _vSince = now;
        }
        else if (now - _vSince > 1500) accept(now);   // no answer: do not block the user
        return;
      case 2:
        if (jc->raw()->spiReplyReady(0x2026, _r1))
        {
          if (jcpHostRecordsMatch(_r0, _r1, _btd->my_bdaddr)) accept(now);
          else reject(now);
        }
        else if (now - _vSince > 1500) accept(now);
        return;
    }
  }

  void accept(uint32_t now)
  {
    const uint8_t i = (uint8_t)_verify;
    _verify = -1;
    _known[i] = true;
    _pairedThisSession = true;
    const DroidSlot slot = _jc[i]->preferredSlot();
    if (slot == SLOT_FOOT) _rec.sides |= JcpRecord::LEFT;
    if (slot == SLOT_DOME) _rec.sides |= JcpRecord::RIGHT;
    EEPROM.put(JOYCON_EEPROM_ADDR, _rec);
    buzz(i, 300, now);

    if (_rec.complete()) end(JCP_DONE);
    else
    {
      _start = now;                      // a fresh minute for the other half
      if (_onEvent) _onEvent(JCP_PAIRED_ONE);
    }
  }

  void reject(uint32_t now)
  {
    const uint8_t i = (uint8_t)_verify;
    _verify = -1;
    _known[i] = false;
    _jc[i]->disconnect();
    _start = now;                        // give the user time to hold sync again
    if (_onEvent) _onEvent(JCP_HOLD_SYNC_AGAIN);
  }

  // Hold Minus + Capture (left) or Plus + Home (right) for 5 s to pair. No stick condition:
  // a drifting stick would block it, holding two face buttons for 5 s while driving does
  // not happen, and the motors stop as soon as the session starts anyway.
  void checkCombos(uint32_t now)
  {
    for (uint8_t i = 0; i < 2; i++)
    {
      JoyConController* jc = _jc[i];
      const bool held = jc->isConnected() && jc->pressed(DB_A | DB_B);
      const uint8_t before = _hold[i].ticks();
      if (_hold[i].update(held, now)) { buzz(i, 500, now); request(); return; }
      if (_hold[i].ticks() > before) buzz(i, 120, now);   // one tick per second held
    }
  }

  void buzz(uint8_t i, uint16_t ms, uint32_t now)
  {
    _jc[i]->buzz(true);
    _buzzUntil[i] = now + ms;
  }

  void updateBuzz(uint32_t now)
  {
    for (uint8_t i = 0; i < 2; i++)
      if (_buzzUntil[i] && (int32_t)(now - _buzzUntil[i]) >= 0)
      {
        _jc[i]->buzz(false);
        _buzzUntil[i] = 0;
      }
  }

  BTD*              _btd;
  JoyConController* _jc[2];
  void            (*_onEvent)(JoyConPairEvent) = NULL;
  JcpRecord         _rec = { JcpRecord::MAGIC, 0, 0 };
  HoldTimer         _hold[2];
  bool              _known[2] = { false, false };
  bool              _wasConnected[2] = { false, false };
  uint32_t          _buzzUntil[2] = { 0, 0 };
  bool              _active = false;
  bool              _pairedThisSession = false;
  bool              _bootsCleared = false;
  uint32_t          _start = 0;
  uint32_t          _lastArm = 0;
  int8_t            _verify = -1;        // index of the Joy-Con being checked, -1 none
  uint8_t           _vStep = 0;
  uint32_t          _vSince = 0;
  uint8_t           _r0[10];
  uint8_t           _r1[10];
};

#endif // JOYCON_PAIRING_LOGIC_ONLY

#endif // JOYCON_PAIRING_H
