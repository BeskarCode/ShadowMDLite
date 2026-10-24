// Host tests for FootAuto.h (no Arduino needed). Run with test/run_tests.sh.
#include <stdio.h>

#include "FootAuto.h"

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __func__, __LINE__, #cond); failures++; } } while (0)

static FootAuto makeOn()
{
    FootAuto fa(FootAutoConfig{40, 30, 300, 4});
    fa.toggle();
    return fa;
}

// --- state ------------------------------------------------------------------------------

static void starts_off_from_boot()
{
    FootAuto fa(FootAutoConfig{40, 30, 300, 4});
    CHECK(!fa.on());
    CHECK(fa.lastOffReason() == FA_BOOT);
    CHECK(!fa.command(10, 0, 0));
}

static void toggle_turns_on_and_off_with_button_reason()
{
    FootAuto fa(FootAutoConfig{40, 30, 300, 4});
    CHECK(fa.toggle());
    CHECK(fa.on());
    CHECK(fa.toggle());
    CHECK(!fa.on());
    CHECK(fa.lastOffReason() == FA_BUTTON);
}

static void turn_off_when_off_returns_false()
{
    FootAuto fa(FootAutoConfig{40, 30, 300, 4});
    CHECK(!fa.turnOff(FA_PI));
    CHECK(fa.lastOffReason() == FA_BOOT);
}

// --- commands, ramp and timeout -----------------------------------------------------------

static void command_is_clamped()
{
    FootAuto fa = makeOn();
    CHECK(fa.command(127, -127, 0));
    FootOutput o = {false, false, 0, 0};
    for (int i = 0; i < 10; i++) o = fa.tick(10 + i);
    CHECK(o.send);
    CHECK(o.drive == 40);
    CHECK(o.turn == -30);
}

static void ramp_limits_each_tick()
{
    FootAuto fa = makeOn();
    fa.command(40, 0, 0);
    FootOutput o = fa.tick(10);
    CHECK(o.send && o.drive == 4);
    o = fa.tick(20);
    CHECK(o.send && o.drive == 8);
}

static void stale_command_stops_once()
{
    FootAuto fa = makeOn();
    fa.command(20, 0, 0);
    fa.tick(10);
    FootOutput o = fa.tick(301);
    CHECK(o.stop && !o.send);
    o = fa.tick(350);
    CHECK(!o.stop && !o.send);
}

static void zero_target_stops_once_then_silent()
{
    FootAuto fa = makeOn();
    fa.command(4, 0, 0);
    FootOutput o = fa.tick(10);
    CHECK(o.send && o.drive == 4);
    fa.command(0, 0, 20);
    o = fa.tick(30);                 // ramps down to 0 in one step -> stop
    CHECK(o.stop && !o.send);
    o = fa.tick(40);
    CHECK(!o.stop && !o.send);
}

static void turning_off_while_moving_stops_once()
{
    FootAuto fa = makeOn();
    fa.command(20, 0, 0);
    fa.tick(10);
    CHECK(fa.turnOff(FA_PI));
    CHECK(fa.lastOffReason() == FA_PI);
    FootOutput o = fa.tick(20);
    CHECK(o.stop && !o.send);
    o = fa.tick(30);
    CHECK(!o.stop && !o.send);
}

static void off_and_still_says_nothing()
{
    FootAuto fa(FootAutoConfig{40, 30, 300, 4});
    FootOutput o = fa.tick(10);
    CHECK(!o.stop && !o.send);
}

static void timeout_survives_millis_wrap()
{
    FootAuto fa = makeOn();
    fa.command(20, 0, 0xFFFFFF00UL);
    FootOutput o = fa.tick(0x00000010UL);      // 272 ms later
    CHECK(o.send);
    o = fa.tick(0x00000100UL);                 // 512 ms later
    CHECK(o.stop);
}

// --- stick and combo ------------------------------------------------------------------------

static void stick_in_deadzone_does_not_cancel()
{
    CHECK(!footStickCancels(128 + 14, 128 - 14, 15, false, true));
}

static void stick_out_of_deadzone_cancels()
{
    CHECK(footStickCancels(128, 110, 15, false, true));
    CHECK(footStickCancels(200, 128, 15, false, true));
}

static void stick_with_foot_l2_as_dome_does_not_cancel()
{
    CHECK(!footStickCancels(255, 128, 15, true, false));
}

static void stick_with_foot_l2_but_dome_connected_cancels()
{
    CHECK(footStickCancels(255, 128, 15, true, true));
}

static void combo_held_toggles_once()
{
    ComboEdge edge;
    CHECK(edge.update(true));
    for (int i = 0; i < 10; i++) CHECK(!edge.update(true));
    CHECK(!edge.update(false));
    CHECK(edge.update(true));
}

int main()
{
    starts_off_from_boot();
    toggle_turns_on_and_off_with_button_reason();
    turn_off_when_off_returns_false();
    command_is_clamped();
    ramp_limits_each_tick();
    stale_command_stops_once();
    zero_target_stops_once_then_silent();
    turning_off_while_moving_stops_once();
    off_and_still_says_nothing();
    timeout_survives_millis_wrap();
    stick_in_deadzone_does_not_cancel();
    stick_out_of_deadzone_cancels();
    stick_with_foot_l2_as_dome_does_not_cancel();
    stick_with_foot_l2_but_dome_connected_cancels();
    combo_held_toggles_once();

    if (failures) { printf("%d FAILED\n", failures); return 1; }
    printf("ALL PASS\n");
    return 0;
}
