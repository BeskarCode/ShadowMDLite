// ROS2D2, created by NandoMadrid (https://www.astromechspain.com/droides/r2-d2-nandomadrid/) · Astromech Spain (https://www.astromechspain.com)
// ---------------------------------------------------------------------------------------
// VirtualPad.h
//
// The pure logic of the virtual PS3 Navs that the ROS2D2 web console drives (its "Remote"
// window), with no Arduino in it so it is tested on a PC (test/VirtualPadTest.cpp). The same
// file lives in ShadowMD Lite and in the classic Shadow MD.
//
//   ROS:PAD:<F|D>,<buttons>,<x>,<y>   one Nav's whole state: F = foot, D = dome;
//                                     buttons = 4 hex digits (upper case), x and y = 0..255
//   ROS:PAD:?                         answer "PAD F:<real|virtual|none> D:<...>"
//
// Button bits, the same as DroidButton (DroidController.h): bit0 UP, 1 DOWN, 2 LEFT, 3 RIGHT,
// 4 CROSS, 5 CIRCLE, 6 L1, 7 L2, 8 L3, 9 PS.
//
// Safety: the console sends a Nav's state 20 times a second while something is pressed. If it
// stops (tab closed, WiFi gone, Pi restarting) the Nav lets everything go after
// VPAD_RELEASE_MS, and counts as switched off after VPAD_DISCONNECT_MS, so the sketch's own
// "controller lost" paths stop the feet and the dome. A real Nav in use always wins (PadArbiter).
// ---------------------------------------------------------------------------------------

#ifndef VIRTUAL_PAD_H
#define VIRTUAL_PAD_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const uint32_t VPAD_RELEASE_MS     = 300;    // silence: sticks centred, buttons let go
static const uint32_t VPAD_DISCONNECT_MS  = 2000;   // silence: the virtual Nav is switched off
static const uint32_t VPAD_REAL_ACTIVE_MS = 500;    // a real Nav moved this recently is "in use"
static const uint16_t VPAD_BUTTONS_MASK   = 0x03FF; // the 10 buttons of a PS3 Nav
static const uint8_t  VPAD_REST_RANGE     = 8;      // a stick within 128 +/- this is at rest

struct PadPacket
{
    uint8_t  pad;        // 0 = foot, 1 = dome
    uint16_t buttons;
    uint8_t  x;
    uint8_t  y;
};

enum PadParse : uint8_t { PAD_NOT_MINE, PAD_PACKET, PAD_QUERY, PAD_BAD };

// Reads a decimal 0..255 at p, moving p past it. false if there is none or it is too big.
inline bool vpadByte(const char*& p, uint8_t& out)
{
    if (*p < '0' || *p > '9') return false;
    unsigned v = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9')
    {
        v = v * 10 + (unsigned)(*p - '0');
        if (++digits > 3 || v > 255) return false;
        p++;
    }
    out = (uint8_t)v;
    return true;
}

// ROS:PAD lines. The packet is only written when the whole line is good; a bad line leaves it as
// it was and says why in error (no "ERR " prefix). The reasons are short on purpose: on a Mega
// every string lives in its scarce RAM.
inline PadParse parsePadCommand(const char* line, PadPacket& out, const char*& error)
{
    static const char PREFIX[] = "ROS:PAD:";
    if (strncmp(line, "ROS:PAD", 7) != 0) return PAD_NOT_MINE;
    if (strncmp(line, PREFIX, sizeof(PREFIX) - 1) != 0) { error = "PAD form"; return PAD_BAD; }
    const char* p = line + sizeof(PREFIX) - 1;
    if (p[0] == '?' && p[1] == 0) return PAD_QUERY;

    PadPacket pk;
    if (*p == 'F') pk.pad = 0;
    else if (*p == 'D') pk.pad = 1;
    else { error = "PAD F|D"; return PAD_BAD; }
    p++;
    if (*p++ != ',') { error = "PAD form"; return PAD_BAD; }

    unsigned b = 0;
    for (int i = 0; i < 4; i++, p++)
    {
        char c = *p;
        unsigned d;
        if (c >= '0' && c <= '9') d = (unsigned)(c - '0');
        else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A' + 10);
        else { error = "PAD buttons"; return PAD_BAD; }
        b = (b << 4) | d;
    }
    if (b & ~VPAD_BUTTONS_MASK) { error = "PAD buttons"; return PAD_BAD; }
    pk.buttons = (uint16_t)b;

    if (*p++ != ',' || !vpadByte(p, pk.x) || *p++ != ',' || !vpadByte(p, pk.y))
    {
        error = "PAD x,y";
        return PAD_BAD;
    }
    if (*p != 0) { error = "PAD end"; return PAD_BAD; }
    out = pk;
    return PAD_PACKET;
}

// One virtual Nav: the last packet and when it came.
class VirtualPadState
{
public:
    VirtualPadState() : _have(false), _at(0) { _last.pad = 0; _last.buttons = 0; _last.x = 128; _last.y = 128; }

    void feed(const PadPacket& p, uint32_t nowMs) { _last = p; _at = nowMs; _have = true; }

    bool connected(uint32_t nowMs) const { return _have && nowMs - _at < VPAD_DISCONNECT_MS; }

    uint16_t buttons(uint32_t nowMs) const { return fresh(nowMs) ? _last.buttons : 0; }
    uint8_t x(uint32_t nowMs) const { return fresh(nowMs) ? _last.x : 128; }
    uint8_t y(uint32_t nowMs) const { return fresh(nowMs) ? _last.y : 128; }

    // The last packet had nothing pressed and the stick at the centre.
    bool atRest() const { return _last.buttons == 0 && near(_last.x) && near(_last.y); }

    uint32_t lastMs() const { return _at; }

    // Switched off on purpose (DroidController::disconnect): dark until the next packet.
    void drop() { _have = false; }

private:
    bool fresh(uint32_t nowMs) const { return _have && nowMs - _at < VPAD_RELEASE_MS; }
    static bool near(uint8_t v) { return v >= 128 - VPAD_REST_RANGE && v <= 128 + VPAD_REST_RANGE; }

    PadPacket _last;
    bool      _have;
    uint32_t  _at;
};

// Who drives a Nav slot. The real Nav wins while it is in use (moved in the last
// VPAD_REAL_ACTIVE_MS). After the real one has taken over, the virtual one only counts again
// once the console sends a packet at rest, so a half-finished virtual press never comes back.
class PadArbiter
{
public:
    PadArbiter() : _moved(false), _lastMove(0), _blocked(false), _wins(false) {}

    void update(uint32_t nowMs, bool realConnected, bool realMoving, bool virtualConnected, bool virtualAtRest)
    {
        if (realConnected && realMoving)
        {
            _moved = true;
            _lastMove = nowMs;
            _blocked = true;
        }
        bool realActive = realConnected && _moved && nowMs - _lastMove < VPAD_REAL_ACTIVE_MS;
        if (_blocked && virtualAtRest && !realActive) _blocked = false;
        _wins = virtualConnected && !realActive && !_blocked;
    }

    bool virtualWins() const { return _wins; }

private:
    bool     _moved;
    uint32_t _lastMove;
    bool     _blocked;
    bool     _wins;
};

// "PAD F:<foot> D:<dome>", each real | virtual | none. Returns its length.
inline int padReport(char* buf, size_t size, const char* foot, const char* dome)
{
    return snprintf(buf, size, "PAD F:%s D:%s", foot, dome);
}

#endif
