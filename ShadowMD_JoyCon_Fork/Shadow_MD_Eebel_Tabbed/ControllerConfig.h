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

// Set to true the first time, to pair a new Joy-Con: upload, then hold the small sync
// button on the Joy-Con rail until the lights run. Set it back to false and upload again,
// and from then on it reconnects by itself.
#define JOYCON_PAIRING_MODE  false

// How you hold it. JOYCON_UPRIGHT is the Nav-like grip, stick at the top, rail against your
// palm. JOYCON_SIDEWAYS turns the stick axes 90 degrees for the mini controller grip.
#define JOYCON_GRIP          JOYCON_UPRIGHT

// Raw stick deflection treated as full scale. Lower it if the droid reaches full speed
// before the stick is all the way over, raise it if it never gets there.
#define JOYCON_STICK_RANGE   1700

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
