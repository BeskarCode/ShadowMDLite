// Host tests for FootAuto.h (no Arduino needed). Run with test/run_tests.sh.
#include <stdio.h>
#include <string.h>

#include "FootAuto.h"

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __func__, __LINE__, #cond); failures++; } } while (0)

static FootAuto makeOn()
{
    FootAuto fa(FootAutoConfig{40, 30, 300, 4});
    fa.press(MODE_GOTO);
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

static void turn_off_when_off_returns_false()
{
    FootAuto fa(FootAutoConfig{40, 30, 300, 4});
    CHECK(!fa.turnOff(FA_PI));
    CHECK(fa.lastOffReason() == FA_BOOT);
}

// --- modes and combo bindings ---------------------------------------------------------------

static void press_turns_on_with_mode()
{
    FootAuto fa(FootAutoConfig{40, 30, 300, 4});
    CHECK(fa.press(MODE_FOLLOW));
    CHECK(fa.on());
    CHECK(fa.mode() == MODE_FOLLOW);
}

static void press_same_mode_turns_off()
{
    FootAuto fa(FootAutoConfig{40, 30, 300, 4});
    fa.press(MODE_FOLLOW);
    CHECK(fa.press(MODE_FOLLOW));
    CHECK(!fa.on());
    CHECK(fa.lastOffReason() == FA_BUTTON);
    CHECK(fa.mode() == MODE_NONE);
}

static void switch_mode_stops_feet()
{
    FootAuto fa = makeOn();
    fa.command(40, 0, 0);
    FootOutput o = {false, false, 0, 0};
    for (int i = 1; i <= 5; i++) o = fa.tick(10 * i);
    CHECK(o.send);
    CHECK(fa.press(MODE_FOLLOW));
    CHECK(fa.on());
    CHECK(fa.mode() == MODE_FOLLOW);
    o = fa.tick(60);
    CHECK(o.stop && !o.send);
    CHECK(fa.command(40, 0, 70));
    o = fa.tick(80);
    CHECK(o.send);
}

static void default_binding_is_ps_l2_goto()
{
    ComboBindings b;
    b.reset();
    CHECK(b.modeFor(comboFromName("FOOT_PS_L2", 10)) == MODE_GOTO);
    for (int i = 1; i < ROS_COMBO_COUNT; i++) CHECK(b.modeFor(i) == MODE_NONE);
}

static void bind_replace_free_and_full()
{
    ComboBindings b;
    b.clear();
    for (int i = 0; i < 8; i++) CHECK(b.bind(i, MODE_FORWARD));
    CHECK(!b.bind(8, MODE_FORWARD));
    CHECK(b.bind(3, MODE_PATROL));
    CHECK(b.modeFor(3) == MODE_PATROL);
    CHECK(b.bind(3, MODE_NONE));
    CHECK(b.modeFor(3) == MODE_NONE);
    CHECK(b.bind(8, MODE_FOLLOW));
    CHECK(b.modeFor(8) == MODE_FOLLOW);
    CHECK(!b.bind(9, MODE_GOTO));
}

static void combo_names()
{
    CHECK(ROS_COMBO_COUNT == 61);
    CHECK(!strcmp(ROS_COMBO_NAMES[0], "FOOT_PS_L2"));
    CHECK(!strcmp(ROS_COMBO_NAMES[1], "FOOT_UP"));
    CHECK(!strcmp(ROS_COMBO_NAMES[6], "FOOT_UP_L3"));
    CHECK(!strcmp(ROS_COMBO_NAMES[24], "FOOT_RIGHT_L3"));
    CHECK(!strcmp(ROS_COMBO_NAMES[25], "DOME_UP"));
    CHECK(!strcmp(ROS_COMBO_NAMES[60], "DOME_RIGHT_L2_CIRCLE"));
    for (int i = 0; i < ROS_COMBO_COUNT; i++)
    {
        CHECK(comboFromName(ROS_COMBO_NAMES[i], strlen(ROS_COMBO_NAMES[i])) == i);
        for (int j = i + 1; j < ROS_COMBO_COUNT; j++) CHECK(strcmp(ROS_COMBO_NAMES[i], ROS_COMBO_NAMES[j]) != 0);
    }
    CHECK(comboFromName("DOME_LEFT_L2_CIRCLE", 19) >= 0);
    CHECK(comboFromName("DOME_PS_L2", 10) == -1);
    CHECK(comboFromName("FOOT_UP_PS", 5) == -1);      // length counts
}

static void mode_names()
{
    CHECK(!strcmp(modeName(MODE_GOTO), "goto"));
    CHECK(!strcmp(modeName(MODE_FORWARD), "forward"));
    CHECK(!strcmp(modeName(MODE_PATROL), "patrol"));
    CHECK(!strcmp(modeName(MODE_FOLLOW), "follow"));
    CHECK(!strcmp(modeName(MODE_NONE), "none"));
    CHECK(modeFromName("follow", 6) == MODE_FOLLOW);
    CHECK(modeFromName("none", 4) == MODE_NONE);
    CHECK(modeFromName("fly", 3) == MODE_NONE);
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

static void silent_controller_counts_as_lost()
{
    CHECK(!footControllerSilent(1000, 900, 300));        // 100 ms since the last report
    CHECK(footControllerSilent(1000, 600, 300));         // 400 ms: the link is gone
    CHECK(!footControllerSilent(0x00000050UL, 0xFFFFFFF0UL, 300));   // 96 ms across the wrap
    CHECK(footControllerSilent(0x00000200UL, 0xFFFFFFF0UL, 300));    // 528 ms across the wrap
}

static void combo_held_toggles_once()
{
    ComboEdge edge;
    CHECK(edge.update(true));
    for (int i = 0; i < 10; i++) CHECK(!edge.update(true));
    CHECK(!edge.update(false));
    CHECK(edge.update(true));
}

// --- dome tracker mode (separate from the feet) ---------------------------------------------

static void dome_track_starts_off_from_boot()
{
    DomeTrack dt;
    CHECK(!dt.on());
    CHECK(dt.lastOffReason() == FA_BOOT);
}

static void dome_track_toggle_turns_on_and_off_with_button_reason()
{
    DomeTrack dt;
    dt.toggle();
    CHECK(dt.on());
    dt.toggle();
    CHECK(!dt.on());
    CHECK(dt.lastOffReason() == FA_BUTTON);
}

static void dome_track_turn_off_keeps_the_first_reason()
{
    DomeTrack dt;
    dt.toggle();
    CHECK(dt.turnOff(FA_CONTROLLER));
    CHECK(!dt.turnOff(FA_PI));
    CHECK(dt.lastOffReason() == FA_CONTROLLER);
}

static void dome_track_is_independent_of_the_feet()
{
    FootAuto fa = makeOn();
    DomeTrack dt;
    CHECK(fa.on() && !dt.on());
    dt.toggle();
    fa.turnOff(FA_STICK);
    CHECK(!fa.on() && dt.on());
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

static void parses_bind()
{
    RosCmd c = parseRosCommand("ROS:BIND:FOOT_UP_PS,follow");
    CHECK(c.kind == ROS_BIND);
    CHECK(c.a == comboFromName("FOOT_UP_PS", 10) && c.a > 0);
    CHECK(c.b == MODE_FOLLOW);
    c = parseRosCommand("ROS:BIND:DOME_RIGHT_L2_CIRCLE,none");
    CHECK(c.kind == ROS_BIND && c.b == MODE_NONE && c.a == 60);
    CHECK(parseRosCommand("ROS:BIND:CLEAR").kind == ROS_BIND_CLEAR);
    CHECK(parseRosCommand("ROS:BIND:RESET").kind == ROS_BIND_RESET);
}

static void bind_errors()
{
    RosCmd c = parseRosCommand("ROS:BIND:FOO,follow");
    CHECK(c.kind == ROS_ERROR && c.error && !strcmp(c.error, "unknown combo"));
    c = parseRosCommand("ROS:BIND:FOOT_UP_PS,fly");
    CHECK(c.kind == ROS_ERROR && c.error && !strcmp(c.error, "unknown mode"));
    const char* bad[] = {"ROS:BIND:FOOT_UP_PS", "ROS:BIND:FOOT_UP_PS,follow ", "ROS:BIND:FOOT_UP_PS,follow,x",
                         "ROS:BIND:", "ROS:BIND:CLEAR ", "ROS:BIND:FOOT_UP_PS,"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        c = parseRosCommand(bad[i]);
        if (c.kind != ROS_ERROR) printf("  not an error: %s\n", bad[i]);
        CHECK(c.kind == ROS_ERROR && c.error);
    }
}

static void parses_track()
{
    CHECK(parseRosCommand("ROS:TRACK:0").kind == ROS_TRACK_OFF);
    CHECK(parseRosCommand("ROS:TRACK:?").kind == ROS_TRACK_QUERY);
}

static void track_on_from_pi_is_an_error()
{
    const char* bad[] = {"ROS:TRACK:1", "ROS:TRACK:", "ROS:TRACK:0 "};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        RosCmd c = parseRosCommand(bad[i]);
        CHECK(c.kind == ROS_ERROR && c.error && !strcmp(c.error, "ROS:TRACK only 0 or ?"));
    }
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

static void combo_latch_fires_once_per_press()
{
    ComboLatch l;
    CHECK(l.press(5));                                // FOOT_UP_PS pressed
    for (int i = 0; i < 4; i++) CHECK(!l.press(5));   // held: the ~1 s repeats are ignored
    l.release();                                      // arrow up
    CHECK(l.press(5));                                // pressed again: fires
    CHECK(l.press(3));                                // another combo with the arrow still down
    CHECK(!l.press(3));
    CHECK(l.press(5));
}

static void first_dome_combo_is_dome_up()
{
    CHECK(!strcmp(ROS_COMBO_NAMES[ROS_FIRST_DOME_COMBO], "DOME_UP"));
    CHECK(!strncmp(ROS_COMBO_NAMES[ROS_FIRST_DOME_COMBO - 1], "FOOT_", 5));
}

int main()
{
    combo_latch_fires_once_per_press();
    first_dome_combo_is_dome_up();
    parses_foot();
    parses_auto_and_dome();
    parses_track();
    parses_bind();
    bind_errors();
    track_on_from_pi_is_an_error();
    auto_on_from_pi_is_an_error();
    malformed_foot_lines_are_errors();
    unknown_ros_command_is_an_error();
    non_ros_lines_pass_through();
    reason_names();
    rate_limit_once_per_second();

    starts_off_from_boot();
    press_turns_on_with_mode();
    press_same_mode_turns_off();
    switch_mode_stops_feet();
    default_binding_is_ps_l2_goto();
    bind_replace_free_and_full();
    combo_names();
    mode_names();
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
    silent_controller_counts_as_lost();

    dome_track_starts_off_from_boot();
    dome_track_toggle_turns_on_and_off_with_button_reason();
    dome_track_turn_off_keeps_the_first_reason();
    dome_track_is_independent_of_the_feet();

    if (failures) { printf("%d FAILED\n", failures); return 1; }
    printf("ALL PASS\n");
    return 0;
}
