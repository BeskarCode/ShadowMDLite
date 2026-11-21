// ---------------------------------------------------------------------------------------
// PairingTest.ino
//
// On-device tests for the pure logic in JoyConPairing.h: the combo hold timer, the triple
// reset counter, the EEPROM record and the Switch two-record check. Nothing Bluetooth,
// nothing that drives anything. Upload, open the serial monitor at 115200, read the result.
//
// JoyConPairing.h is a copy of the one at the root of the repo. Keep them identical.
// ---------------------------------------------------------------------------------------

#define JOYCON_PAIRING_LOGIC_ONLY
#include "JoyConPairing.h"

uint8_t failures = 0;

void check(const __FlashStringHelper* name, bool ok)
{
  Serial.print(ok ? F("PASS ") : F("FAIL "));
  Serial.println(name);
  if (!ok) failures++;
}

void testHoldFiresAt5s()
{
  HoldTimer t;
  bool early = false;
  for (uint32_t ms = 0; ms < 5000; ms += 100) early |= t.update(true, ms);
  early |= t.update(true, 4999);
  const bool fired = t.update(true, 5000);
  const bool again = t.update(true, 6000);
  check(F("hold_fires_at_5s"), !early && fired && !again);
}

void testHoldReleaseResets()
{
  HoldTimer t;
  for (uint32_t ms = 0; ms <= 3000; ms += 100) t.update(true, ms);
  t.update(false, 3001);
  bool early = false;
  for (uint32_t ms = 3002; ms <= 8001; ms += 1) early |= t.update(true, ms);
  const bool fired = t.update(true, 8002);
  check(F("hold_release_resets"), !early && fired);
}

void testHoldTicks()
{
  HoldTimer t;
  t.update(true, 0);
  t.update(true, 2500);
  check(F("hold_ticks"), t.ticks() == 2);
}

void testTripleReset()
{
  uint8_t c = 0;
  const bool a = jcpTripleReset(c);
  const bool b = jcpTripleReset(c);
  const bool d = jcpTripleReset(c);
  check(F("triple_reset"), !a && !b && d && c == 0);
}

void testRecordValid()
{
  JcpRecord full    = { 0x4A, 3, 0 };
  JcpRecord half    = { 0x4A, 1, 0 };
  JcpRecord garbage = { 0xFF, 3, 0 };
  check(F("record_valid"), full.valid() && full.complete() &&
                           half.valid() && !half.complete() &&
                           !garbage.valid());
}

void testHostMatch()
{
  // As read on the bench: magic, size, checksum x2, then the host address big-endian.
  const uint8_t dongleRec[10] = { 0x95, 0x22, 0x99, 0x05, 0x00, 0x1A, 0x7D, 0xDA, 0x71, 0x13 };
  const uint8_t oldRec[10]    = { 0x00, 0x22, 0x98, 0x1A, 0x00, 0x1A, 0x7D, 0xDA, 0x71, 0x13 };
  const uint8_t switchRec[10] = { 0x00, 0x22, 0x92, 0x1C, 0x98, 0x41, 0x5C, 0x48, 0xC6, 0xF9 };
  const uint8_t dongleLE[6]   = { 0x13, 0x71, 0xDA, 0x7D, 0x1A, 0x00 };   // BTD::my_bdaddr order
  check(F("host_match"), jcpHostRecordsMatch(oldRec, dongleRec, dongleLE) &&
                         !jcpHostRecordsMatch(switchRec, dongleRec, dongleLE));
}

void setup()
{
  Serial.begin(115200);
  while (!Serial);
  Serial.println(F("\r\n=== JoyConPairing logic tests ==="));

  testHoldFiresAt5s();
  testHoldReleaseResets();
  testHoldTicks();
  testTripleReset();
  testRecordValid();
  testHostMatch();

  if (failures == 0) Serial.println(F("ALL PASS"));
  else { Serial.print(failures); Serial.println(F(" FAILED")); }
}

void loop() {}
