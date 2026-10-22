// ---------------------------------------------------------------------------------------
// DroidControllerLayer.ino
//
// Test bench for the controller abstraction layer. It does NOT drive a droid: no
// Sabertooth, no SyRen, no MarcDuino, nothing that can move. It reads controllers through
// the DroidController interface and prints what it sees, so the layer can be verified
// before a single line of ShadowMDLite.ino is touched.
//
// Hardware: the same Arduino Mega ADK, USB Host Shield and Bluetooth dongle as the droid.
//
// ---------------------------------------------------------------------------------------
// HOW TO RUN IT
//
// 1. Pick what you are testing below with TEST_MODE.
// 2. Pairing works as in the droid sketches (JoyConPairing.h): on the first boot it starts
//    by itself, hold sync on each Joy-Con. Later: Minus+Capture or Plus+Home for 5 s, three
//    resets in a row, or send 'p'.
// 3. Serial Monitor at 115200. Commands: 'p' pair, 'r' dump each Joy-Con's two pairing
//    records, 'f' forget the paired Joy-Cons (the next boot pairs from scratch).
//
// WHAT TO CHECK
//
//   - Both controllers connect, left lights LED1 and right lights LED2.
//   - Leave the sticks alone for the first second: that is the auto centring. After it,
//     the stick must read 0,0 at rest and legacy 128,128.
//   - Full deflection reaches roughly +/-512. If it saturates too early or never gets
//     there, adjust setStickRange().
//   - Pushing the stick AWAY from you gives NEGATIVE y. That is the Nav convention and the
//     droid logic depends on it.
//   - Every button prints the right role. DB_B is unmapped on a Joy-Con by design, see the
//     mapping notes in JoyConController.h.
//   - A click prints once per press, not continuously.
//   - Walking out of range makes lag climb. That is what stops the motors on a real droid.
// ---------------------------------------------------------------------------------------

#include <usbhub.h>
#include <PS3BT.h>

#include "DroidController.h"
#include "PS3NavController.h"
#include "JoyConController.h"
#include "JoyConPairing.h"

// --- configuration ----------------------------------------------------------------------
#define TEST_PS3NAV   0   // set to 1 to test the PS3 Navs
#define TEST_JOYCON   1   // set to 1 to test the Joy-Cons
// -----------------------------------------------------------------------------------------

USB Usb;
BTD Btd(&Usb);

#if TEST_PS3NAV
PS3NavController navFoot(&Btd, "NAV-FOOT", LED1);
PS3NavController navDome(&Btd, "NAV-DOME", LED2);
#endif

#if TEST_JOYCON
JoyConController joyFoot(&Btd, JOYCON_LEFT,  "JOY-L");
JoyConController joyDome(&Btd, JOYCON_RIGHT, "JOY-R");
JoyConPairing joyPairing(&Btd, &joyFoot, &joyDome);

void onPairEvent(JoyConPairEvent e)
{
  switch (e)
  {
    case JCP_STARTED:         Serial.println(F("PAIRING started: hold sync on a Joy-Con")); break;
    case JCP_PAIRED_ONE:      Serial.println(F("PAIRING paired one, now the other")); break;
    case JCP_DONE:            Serial.println(F("PAIRING done")); break;
    case JCP_TIMEOUT:         Serial.println(F("PAIRING timed out")); break;
    case JCP_HOLD_SYNC_AGAIN: Serial.println(F("PAIRING that Joy-Con remembers another host: hold sync again")); break;
  }
}

// Which controller is driving what, after routing. Starts in creation order.
DroidController* footSlot = &joyFoot;
DroidController* domeSlot = &joyDome;
#endif

// Same dead zones as the main sketch, converted to the normalised scale.
const int16_t footDeadZone = DroidController::legacyDeadzone(15);
const int16_t domeDeadZone = DroidController::legacyDeadzone(10);

uint32_t lastPrint = 0;
uint32_t _buzzUntil = 0;

void setup()
{
  Serial.begin(115200);
  while (!Serial);

  Serial.println(F("\r\n=== Droid Controller Layer test bench ==="));
  Serial.println(F("Nothing here drives motors. Safe to run with the droid powered down."));

  if (Usb.Init() == -1)
  {
    Serial.println(F("USB host did not start. HALT."));
    while (1);
  }
  Serial.println(F("USB + Bluetooth library started."));

#if TEST_PS3NAV
  navFoot.begin();
  navDome.begin();
#endif
#if TEST_JOYCON
  joyFoot.begin();
  joyDome.begin();

  // A Joy-Con stick is physically smaller than a Nav's, so a slightly tighter dead zone
  // feels better. Left as the sketch default here on purpose, so the numbers printed are
  // comparable between the two controllers.
  joyFoot.setGrip(JOYCON_UPRIGHT);
  joyDome.setGrip(JOYCON_UPRIGHT);
  joyFoot.raw()->spiEcho = true;     // 'r' prints the pairing records
  joyDome.raw()->spiEcho = true;
  joyPairing.begin(onPairEvent);
#endif

  Serial.println(F("\r\nWaiting for controllers..."));
  Serial.println();
}

void loop()
{
  Usb.Task();

#if TEST_PS3NAV
  navFoot.update();
  navDome.update();
#endif
#if TEST_JOYCON
  joyFoot.update();
  joyDome.update();

  // Same routing the droid sketches use: left half to the feet, right half to the dome.
  if (routeBySlot(footSlot, domeSlot))
  {
    Serial.print(F("ROUTED: feet <- "));
    Serial.print(footSlot->name());
    Serial.print(F(", dome <- "));
    Serial.println(domeSlot->name());
  }

  // The same pairing session the droid sketches run.
  joyPairing.update();

  // Serial commands: 'p' pair, 'r' dump pairing records, 'f' forget paired Joy-Cons.
  static uint32_t _spiStep = 0;
  if (Serial.available())
  {
    const char c = Serial.read();
    if (c == 'p')
    {
      joyPairing.request();
    }
    else if (c == 'f')
    {
      EEPROM.write(JOYCON_EEPROM_ADDR, 0xFF);   // invalid magic: next boot pairs from scratch
      Serial.println(F("Paired Joy-Cons forgotten. Reset the Mega to pair from scratch."));
    }
    else if (c == 'r')
    {
      _spiStep = millis() | 1;
      Serial.println(F("Reading pairing info..."));
    }
  }
  // One read at a time, spaced out, so each reply has room to arrive.
  if (_spiStep && millis() - _spiStep > 300)
  {
    static uint8_t n = 0;
    JoyConController* jc = (n / 2 == 0) ? &joyFoot : &joyDome;
    if (jc->isConnected())
    {
      Serial.println(); Serial.print(jc->name()); Serial.print(' ');
      jc->raw()->readSpi((n & 1) ? 0x2026 : 0x2000, 0x1D);
    }
    _spiStep = millis() | 1;
    if (++n >= 4) { n = 0; _spiStep = 0; }
  }
#endif

  // Print every 500 ms, or at once when a button changes so short presses are not missed.
  static uint32_t lastButtons = 0;
  const uint32_t nowButtons = joyFoot.buttons() | ((uint32_t)joyDome.buttons() << 16);
  const bool buttonsChanged = nowButtons != lastButtons;
  lastButtons = nowButtons;
  if (!buttonsChanged && millis() - lastPrint < 500) return;
  lastPrint = millis();

#if TEST_PS3NAV
  report(navFoot, footDeadZone);
  report(navDome, domeDeadZone);
  if (navFoot.clicked(DB_A)) Serial.println(F("  [click] NAV-FOOT A"));
#endif

#if TEST_JOYCON
  Serial.print(F("FEET -> "));
  reportJoyCon(*(JoyConController*)footSlot, footDeadZone);
  Serial.print(F("DOME -> "));
  reportJoyCon(*(JoyConController*)domeSlot, domeDeadZone);

  // Edge detection check: once per press, never repeatedly.
  if (joyFoot.clicked(DB_A))   Serial.println(F("  [click] JOY-L A"));
  if (joyFoot.clicked(DB_SYS)) Serial.println(F("  [click] JOY-L SYS"));
  if (joyDome.clicked(DB_A))   Serial.println(F("  [click] JOY-R A"));

  // The combination ShadowMD Lite uses for the voice toggle, read through the layer, and
  // confirmed on the controller itself with a buzz. A Nav cannot do this.
  if (joyFoot.clicked(DB_DOWN | DB_L1))
  {
    Serial.println(F("  [combo] DOWN + L1 -> would toggle voice mode"));
    joyFoot.buzz(true);
    _buzzUntil = millis() + 200;
  }
  if (_buzzUntil && millis() > _buzzUntil)
  {
    joyFoot.buzz(false);
    _buzzUntil = 0;
  }
#endif
}


void report(DroidController& c, int16_t deadZone)
{
  Serial.print(c.name());
  Serial.print(F(": "));

  if (!c.isConnected())
  {
    Serial.println(F("not connected"));
    return;
  }

  Serial.print(F("x="));
  Serial.print(DroidController::deadzone(c.x(), deadZone));
  Serial.print(F(" y="));
  Serial.print(DroidController::deadzone(c.y(), deadZone));
  Serial.print(F("  legacy("));
  Serial.print(c.legacyX());
  Serial.print(F(","));
  Serial.print(c.legacyY());
  Serial.print(F(")  lag="));
  Serial.print(c.millisSinceLastMessage());
  Serial.print(F("ms  buttons: "));

  printButtons(c);
  Serial.println();
}

#if TEST_JOYCON
void reportJoyCon(JoyConController& c, int16_t deadZone)
{
  if (c.isConnected() && c.isCalibrating())
  {
    Serial.print(c.name());
    Serial.println(F(": centring the stick, do not touch it"));
    return;
  }

  report(c, deadZone);

  if (c.isConnected())
  {
    Serial.print(F("        battery: "));
    Serial.print(c.batteryLevel());
    Serial.print(F("/4  raw L("));
    Serial.print(c.raw()->getAnalogHat(LeftHatX));
    Serial.print(F(","));
    Serial.print(c.raw()->getAnalogHat(LeftHatY));
    Serial.print(F(") R("));
    Serial.print(c.raw()->getAnalogHat(RightHatX));
    Serial.print(F(","));
    Serial.print(c.raw()->getAnalogHat(RightHatY));
    Serial.println(F(")"));
  }
}
#endif

void printButtons(DroidController& c)
{
  if (c.pressed(DB_UP))    Serial.print(F("UP "));
  if (c.pressed(DB_DOWN))  Serial.print(F("DOWN "));
  if (c.pressed(DB_LEFT))  Serial.print(F("LEFT "));
  if (c.pressed(DB_RIGHT)) Serial.print(F("RIGHT "));
  if (c.pressed(DB_A))     Serial.print(F("A "));
  if (c.pressed(DB_B))     Serial.print(F("B "));
  if (c.pressed(DB_L1))    Serial.print(F("L1 "));
  if (c.pressed(DB_L2))    Serial.print(F("L2 "));
  if (c.pressed(DB_STICK)) Serial.print(F("STICK "));
  if (c.pressed(DB_SYS))   Serial.print(F("SYS "));
  if (!c.anyPressed())     Serial.print(F("-"));
}
