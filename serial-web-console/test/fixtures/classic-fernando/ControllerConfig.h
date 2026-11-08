// ---------------------------------------------------------------------------------------
// ControllerConfig.h
//
// THIS IS THE ONLY FILE YOU NEED TO EDIT TO CHANGE CONTROLLER.
//
// Shadow MD was written for PS3 Move Navigation controllers, which Sony stopped making
// years ago. This fork lets you run the same sketch, with the same button map and the same
// functions, using a single Nintendo Switch Joy-Con in each hand instead.
//
// Pick one of the two lines below and leave the other commented out.
// ---------------------------------------------------------------------------------------

#ifndef CONTROLLER_CONFIG_H
#define CONTROLLER_CONFIG_H

// --- Choose your controllers ------------------------------------------------------------

#define USE_PS3_NAV                 // original behaviour, two PS3 Move Navigation controllers
//#define USE_JOYCON                // two Joy-Cons: left for the feet, right for the dome

// --- Joy-Con options, ignored unless USE_JOYCON is selected -----------------------------

// Pairing needs no change here. On the first boot the droid pairs by itself: hold the
// small sync button on the left Joy-Con, then on the right one. To pair again later, hold
// Minus + Capture (left) or Plus + Home (right) for 5 seconds, or reset the Mega three
// times in a row. true forces pairing on every boot.
#define JOYCON_PAIRING_MODE  false

// R2 sounds played during pairing, as MarcDuino commands. Change them to suit your sound
// card: "$51" is bank 5, sound 1.
#define JOYCON_SOUND_STARTED  "$51"   // pairing mode on: hold sync on a Joy-Con
#define JOYCON_SOUND_PAIRED   "$31"   // one paired, now the other one
#define JOYCON_SOUND_DONE     "$32"   // both paired
#define JOYCON_SOUND_TIMEOUT  "$41"   // gave up after 60 seconds
#define JOYCON_SOUND_AGAIN    "$21"   // that Joy-Con still remembers a Switch: hold sync again

// How you hold it. JOYCON_UPRIGHT is the Nav-like grip, stick at the top, rail against your
// palm. JOYCON_SIDEWAYS turns the stick axes 90 degrees for the mini controller grip.
#define JOYCON_GRIP          JOYCON_UPRIGHT

// Raw stick deflection treated as full scale. Lower it if the droid reaches full speed
// before the stick is all the way over, raise it if it never gets there.
#define JOYCON_STICK_RANGE   1000

// ---------------------------------------------------------------------------------------
// Joy-Con button map, for reference. See CheatSheet_JoyCon.html for the printable version.
//
//   FOOT, left Joy-Con              DOME, right Joy-Con          Was on the Nav
//   d-pad                           X / B / Y / A                d-pad
//   Minus                           Plus                         CROSS
//   Capture                         Home                         CIRCLE
//   L                               R                            L1
//   ZL                              ZR                           L2
//   stick click                     stick click                  PS
//   (nothing)                       (nothing)                    L3
//
// A Joy-Con has nine usable buttons, the same as a Nav, but not the same nine, so one role
// has to give. L3 is the one, because in the stock map almost nothing uses it. The single
// function that moves is the over speed toggle: it was L3 + L1, it is now stick click + ZL.
// ---------------------------------------------------------------------------------------

#if defined(USE_PS3_NAV) && defined(USE_JOYCON)
  #error "Pick ONE controller type in ControllerConfig.h, not both."
#endif
#if !defined(USE_PS3_NAV) && !defined(USE_JOYCON)
  #error "Pick a controller type in ControllerConfig.h."
#endif

#endif // CONTROLLER_CONFIG_H
