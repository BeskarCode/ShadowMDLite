// ROS2D2, created by NandoMadrid (https://www.astromechspain.com/droides/r2-d2-nandomadrid/) · Astromech Spain (https://www.astromechspain.com)
// Host tests for VirtualPad.h (no Arduino needed). Run with test/run_tests.sh.
#include <stdio.h>
#include <string.h>

#include "VirtualPad.h"

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __func__, __LINE__, #cond); failures++; } } while (0)

static PadParse parse(const char* line, PadPacket& p)
{
    const char* err = NULL;
    return parsePadCommand(line, p, err);
}

static void test_parse_packets()
{
    PadPacket p;
    CHECK(parse("ROS:PAD:D,0280,128,128", p) == PAD_PACKET);
    CHECK(p.pad == 1 && p.buttons == 0x0280 && p.x == 128 && p.y == 128);
    CHECK(parse("ROS:PAD:F,0000,0,255", p) == PAD_PACKET);
    CHECK(p.pad == 0 && p.buttons == 0 && p.x == 0 && p.y == 255);
    CHECK(parse("ROS:PAD:?", p) == PAD_QUERY);
    CHECK(parse("ROS:FOOT:1,2", p) == PAD_NOT_MINE);
    CHECK(parse("DM:20", p) == PAD_NOT_MINE);
}

static void test_parse_bad_lines_keep_the_packet()
{
    const char* bad[] = {"ROS:PAD:F,02", "ROS:PAD:X,0000,128,128", "ROS:PAD:F,0400,128,128",
                         "ROS:PAD:F,0000,256,128", "ROS:PAD:F,0000,128,128 ", "ROS:PAD:f,0000,128,128",
                         "ROS:PAD:F,0000,128", "ROS:PAD:", "ROS:PAD:F,00G0,128,128", "ROS:PAD:F,0000,-1,128"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        PadPacket p = {0, 0x0123, 7, 9};
        const char* err = NULL;
        CHECK(parsePadCommand(bad[i], p, err) == PAD_BAD);
        CHECK(err != NULL && err[0] != 0);
        CHECK(p.buttons == 0x0123 && p.x == 7 && p.y == 9);   // untouched
    }
}

static void test_state_releases_and_disconnects_on_silence()
{
    VirtualPadState s;
    CHECK(!s.connected(0));
    PadPacket l2 = {0, 0x0080, 200, 60};
    s.feed(l2, 1000);
    CHECK(s.connected(1000) && s.buttons(1000) == 0x0080 && s.x(1000) == 200 && s.y(1000) == 60);
    CHECK(s.buttons(1299) == 0x0080);
    CHECK(s.buttons(1300) == 0 && s.x(1300) == 128 && s.y(1300) == 128);   // let go after 300 ms
    CHECK(s.connected(2999));
    CHECK(!s.connected(3000));                                             // gone after 2 s
    CHECK(s.lastMs() == 1000);
    CHECK(!s.atRest());
    PadPacket rest = {0, 0, 130, 125};
    s.feed(rest, 3100);
    CHECK(s.atRest() && s.connected(3100));
}

static void test_arbiter_real_wins_and_virtual_waits_for_rest()
{
    PadArbiter a;
    a.update(0, false, false, true, false);
    CHECK(a.virtualWins());                       // no real Nav
    a.update(10, true, false, true, false);
    CHECK(a.virtualWins());                       // real connected but still
    a.update(20, true, true, true, false);
    CHECK(!a.virtualWins());                      // real moving
    a.update(600, true, false, true, false);
    CHECK(!a.virtualWins());                      // real still again, but no rest packet yet
    a.update(700, true, false, true, true);
    CHECK(a.virtualWins());                       // after a rest packet
    a.update(710, true, true, true, false);       // real moves in the middle of a virtual hold
    CHECK(!a.virtualWins());
    a.update(800, false, false, true, false);     // real gone, virtual still mid-press: wait
    CHECK(!a.virtualWins());
    a.update(900, false, false, true, true);
    CHECK(a.virtualWins());
}

static void test_arbiter_real_active_window()
{
    PadArbiter a;
    a.update(0, true, true, true, true);
    CHECK(!a.virtualWins());
    a.update(499, true, false, true, true);
    CHECK(!a.virtualWins());                      // moved less than 500 ms ago
    a.update(500, true, false, true, true);
    CHECK(a.virtualWins());
}

static void test_report()
{
    char buf[32];
    padReport(buf, sizeof(buf), "real", "none");
    CHECK(strcmp(buf, "PAD F:real D:none") == 0);
}

int main()
{
    test_parse_packets();
    test_parse_bad_lines_keep_the_packet();
    test_state_releases_and_disconnects_on_silence();
    test_arbiter_real_wins_and_virtual_waits_for_rest();
    test_arbiter_real_active_window();
    test_report();
    if (failures) { printf("VirtualPadTest: %d failure(s)\n", failures); return 1; }
    printf("VirtualPadTest: all passed\n");
    return 0;
}
