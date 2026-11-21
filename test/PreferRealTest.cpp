// ROS2D2, created by NandoMadrid (https://www.astromechspain.com/droides/r2-d2-nandomadrid/) · Astromech Spain (https://www.astromechspain.com)
// Host tests for VirtualNavController.h and PreferRealController.h. Run with test/run_tests.sh.
#include <stdio.h>
#include "Arduino.h"
#include "DroidController.h"
#include "VirtualPad.h"
#include "VirtualNavController.h"
#include "PreferRealController.h"

uint32_t g_fakeMillis = 0;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __func__, __LINE__, #cond); failures++; } } while (0)

// A real Nav whose state the test sets
class FakeNav : public DroidController
{
public:
    bool connected = false;
    int disconnects = 0;
    uint16_t want = 0;
    int16_t wx = 0, wy = 0;
    void begin() override {}
    void update() override { rememberPrevious(); setButtons(want); setStick(wx, wy); }
    bool isConnected() const override { return connected; }
    void disconnect() override { connected = false; disconnects++; }
    uint32_t millisSinceLastMessage() const override { return 5; }
    const char* name() const override { return "real"; }
};

static void feed(VirtualPadState& s, uint16_t b, uint8_t x = 128, uint8_t y = 128)
{
    PadPacket p = {0, b, x, y};
    s.feed(p, g_fakeMillis);
}

static void test_virtual_nav_follows_packets_and_lets_go()
{
    VirtualPadState s;
    VirtualNavController v(s);
    g_fakeMillis = 1000;
    v.update();
    CHECK(!v.isConnected());
    feed(s, DB_L2, 255, 0);
    v.update();
    CHECK(v.isConnected() && v.pressed(DB_L2) && v.clicked(DB_L2));
    CHECK(v.legacyX() >= 254 && v.legacyY() <= 1);
    g_fakeMillis = 1100;
    v.update();
    CHECK(v.pressed(DB_L2) && !v.clicked(DB_L2));          // the click comes once
    g_fakeMillis = 1300;
    v.update();
    CHECK(!v.pressed(DB_L2) && v.legacyX() == 128 && v.released(DB_L2));
    CHECK(v.millisSinceLastMessage() == 300);
    g_fakeMillis = 3000;
    v.update();
    CHECK(!v.isConnected());
}

static void test_prefer_real_switches_between_them()
{
    FakeNav real;
    VirtualPadState s;
    VirtualNavController v(s);
    PadArbiter arb;
    PreferRealController pr(&real, &v, arb, 15);
    g_fakeMillis = 0;
    feed(s, DB_SYS | DB_L2);
    pr.update();
    CHECK(pr.isConnected() && pr.isVirtual() && pr.pressed(DB_SYS | DB_L2));   // no real Nav
    real.connected = true;
    real.want = DB_A;
    g_fakeMillis = 50;
    feed(s, DB_SYS | DB_L2);
    pr.update();
    CHECK(!pr.isVirtual() && pr.pressed(DB_A) && !pr.pressed(DB_L2));        // real in use wins
    real.want = 0;
    g_fakeMillis = 600;
    feed(s, 0);                                                                // virtual at rest
    pr.update();
    CHECK(pr.isVirtual() && !pr.anyPressed());
    g_fakeMillis = 650;
    feed(s, DB_B);
    pr.update();
    CHECK(pr.isVirtual() && pr.clicked(DB_B));                                 // edge seen once
    g_fakeMillis = 700;
    feed(s, DB_B);
    pr.update();
    CHECK(pr.pressed(DB_B) && !pr.clicked(DB_B));
}

static void test_real_stick_counts_as_in_use()
{
    FakeNav real;
    VirtualPadState s;
    VirtualNavController v(s);
    PadArbiter arb;
    PreferRealController pr(&real, &v, arb, 15);
    real.connected = true;
    g_fakeMillis = 0;
    feed(s, 0);
    pr.update();
    CHECK(pr.isVirtual());
    real.wx = DroidController::legacyDeadzone(15) + 4;                         // past the dead zone
    g_fakeMillis = 20;
    feed(s, 0);
    pr.update();
    CHECK(!pr.isVirtual());
    CHECK(pr.isConnected());
}

static void test_disconnect_only_touches_the_one_driving()
{
    // the sketch drops a controller it does not like: with the virtual one driving, the real
    // (switched off) Nav must not be told anything (a PS3BT that is not there errors on L2CAP)
    FakeNav real;
    VirtualPadState s;
    VirtualNavController v(s);
    PadArbiter arb;
    PreferRealController pr(&real, &v, arb, 15);
    g_fakeMillis = 0;
    feed(s, DB_SYS);
    pr.update();
    CHECK(pr.isVirtual());
    pr.disconnect();
    CHECK(real.disconnects == 0);
    pr.update();
    CHECK(!pr.isConnected());                     // the virtual one is gone until its next packet
    real.connected = true;
    real.want = DB_A;
    g_fakeMillis = 100;
    pr.update();
    pr.disconnect();
    CHECK(real.disconnects == 1);                 // the real one driving: it is the one dropped
}

int main()
{
    test_virtual_nav_follows_packets_and_lets_go();
    test_prefer_real_switches_between_them();
    test_real_stick_counts_as_in_use();
    test_disconnect_only_touches_the_one_driving();
    if (failures) { printf("PreferRealTest: %d failure(s)\n", failures); return 1; }
    printf("PreferRealTest: all passed\n");
    return 0;
}
