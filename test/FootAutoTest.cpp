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

// --- ROS: command parser ------------------------------------------------------------------

#include <string.h>

static void parses_foot()
{
    RosCmd c = parseRosCommand("ROS:FOOT:30,-20");
    CHECK(c.kind == ROS_FOOT);
    CHECK(c.a == 30 && c.b == -20);
}

static void parses_auto_and_dome()
{
    CHECK(parseRosCommand("ROS:AUTO:0").kind == ROS_AUTO_OFF);
    CHECK(parseRosCommand("ROS:AUTO:?").kind == ROS_AUTO_QUERY);
    RosCmd c = parseRosCommand("ROS:DOME:-40");
    CHECK(c.kind == ROS_DOME && c.a == -40);
}

static void auto_on_from_pi_is_an_error()
{
    RosCmd c = parseRosCommand("ROS:AUTO:1");
    CHECK(c.kind == ROS_ERROR);
    CHECK(c.error && !strcmp(c.error, "ROS:AUTO only 0 or ?"));
}

static void malformed_foot_lines_are_errors()
{
    const char* bad[] = {"ROS:FOOT:30", "ROS:FOOT:a,b", "ROS:FOOT:30,20,5", "ROS:FOOT:99999,0",
                         "ROS:FOOT:", "ROS:FOOT:128,0", "ROS:FOOT:30,", "ROS:DOME:", "ROS:DOME:5x"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        RosCmd c = parseRosCommand(bad[i]);
        if (c.kind != ROS_ERROR) printf("  not an error: %s\n", bad[i]);
        CHECK(c.kind == ROS_ERROR && c.error);
    }
}

static void unknown_ros_command_is_an_error()
{
    CHECK(parseRosCommand("ROS:JUMP:1").kind == ROS_ERROR);
}

static void non_ros_lines_pass_through()
{
    CHECK(parseRosCommand("MD:12").kind == ROS_NONE);
    CHECK(parseRosCommand("DM:30").kind == ROS_NONE);
    CHECK(parseRosCommand("PING").kind == ROS_NONE);
}

static void reason_names()
{
    CHECK(!strcmp(reasonName(FA_BOOT), "boot"));
    CHECK(!strcmp(reasonName(FA_BUTTON), "button"));
    CHECK(!strcmp(reasonName(FA_STICK), "stick"));
    CHECK(!strcmp(reasonName(FA_CONTROLLER), "controller"));
    CHECK(!strcmp(reasonName(FA_PI), "pi"));
}

static void rate_limit_once_per_second()
{
    uint32_t last = (uint32_t)0 - (uint32_t)1000;
    CHECK(rateLimitAllows(last, 0, 1000));
    CHECK(!rateLimitAllows(last, 500, 1000));
    CHECK(rateLimitAllows(last, 1000, 1000));
}

int main()
{
    parses_foot();
    parses_auto_and_dome();
    auto_on_from_pi_is_an_error();
    malformed_foot_lines_are_errors();
    unknown_ros_command_is_an_error();
    non_ros_lines_pass_through();
    reason_names();
    rate_limit_once_per_second();

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
