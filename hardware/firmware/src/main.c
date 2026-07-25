// ============================================================================
//  main.c — Glitchwave 567 pedal firmware.  Illicit Apothecary.
//
//  Core 0: the control loop. Reads pots, CVs, footswitches; runs the gate, the
//          LFOs and the envelope follower; writes the ten CV duty cycles.
//          Nothing here ever blocks.
//
//  Core 1: the WS2812 chain and the USB console. Both are things that can
//          stall for milliseconds at a time, which is exactly why they don't
//          live on core 0.
// ============================================================================
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/flash.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"

#include "board.h"
#include "tunables.h"
#include "hw_io.h"
#include "control.h"
#include "console.h"
#include "flash_store.h"
#include "mod_system.h"

#include <stdio.h>

// ---------------------------------------------------------------------------
//  Core 1
// ---------------------------------------------------------------------------
static void core1_main (void)
{
    // Lets core 0 park us while it writes flash.
    multicore_lockout_victim_init();

    gw_console_init();

    absolute_time_t next_led = get_absolute_time();

    while (true)
    {
        gw_console_service();

        // The LED state model runs here too, at the frame rate, so core 0
        // never spends time on colour maths.
        if (time_reached (next_led))
        {
            const int fps = (int) gwt.led_fps;
            const uint32_t period_us = (uint32_t) (1000000 / (fps > 0 ? fps : 60));
            next_led = make_timeout_time_us (period_us);
            gw_control_leds ((float) period_us * 1e-6f);
        }

        gw_leds_service();
        tight_loop_contents();
    }
}

// ---------------------------------------------------------------------------
//  Core 0
// ---------------------------------------------------------------------------
int main (void)
{
    // ---- settings first, because the drivers read them as they initialise --
    gw_tunables_defaults (&gwt);

    stdio_init_all();
    gw_store_init();

    const bool had_saved = gw_store_load (&gwt);

    // ---- hardware ----------------------------------------------------------
    gw_pwm_init();          // every CV starts at 0 duty: quiet, not a click
    gw_select_init();       // FREQ_A/B + FMODE_A/B/C, inversion handled inside
    gw_adc_init();
    gw_leds_init();

    gw_control_init();

    multicore_launch_core1 (core1_main);

    // Give USB a moment to come up so the banner isn't lost, but never wait
    // for a host -- the pedal has to work with nothing plugged into USB.
    for (int i = 0; i < 40 && ! stdio_usb_connected(); ++i) sleep_ms (25);

    if (gwt.verbose_boot)
    {
        gw_console_banner();
        printf ("  settings: %s\n", had_saved ? "loaded from flash"
                                              : "shipping defaults (nothing saved yet)");
        printf ("  clock:    %lu Hz\n", (unsigned long) clock_get_hz (clk_sys));
        printf ("  rate:     %d Hz control loop\n", (int) gwt.ctrl_rate_hz);
        if (gwt.bench_mode)
            printf ("\n  *** BENCH MODE *** no PCB expected. Pots and the envelope are\n"
                    "  synthesised so you can watch the LEDs and scope the CV pins.\n"
                    "  Set bench_mode 0 (then save) once the pedal is built.\n");
        if (! gwt.cal_done)
            printf ("\n  Not calibrated yet. Once the board is powered, run:\n"
                    "      cal adc          then    cal va <the volts you measured>\n"
                    "      cal pots         then    save\n");
        printf ("\ngw> ");
        fflush (stdout);
    }

    // ---- the loop ----------------------------------------------------------
    // Absolute-deadline scheduling: each tick is placed at a fixed grid point,
    // so a slow tick borrows from the next one instead of letting the whole
    // loop drift. dt stays constant, which is what every filter coefficient
    // and LFO phase increment in here assumes.
    uint32_t period_us = (uint32_t) (1000000.0f / (float) gwt.ctrl_rate_hz);
    absolute_time_t next = make_timeout_time_us (period_us);
    int32_t         rate_check = 0;

    while (true)
    {
        sleep_until (next);
        next = delayed_by_us (next, period_us);

        // A flash `save` parks this core for tens of milliseconds, which leaves
        // the deadline far in the past. Left alone, the loop would then sprint
        // through dozens of catch-up ticks with a dt that is a lie -- every
        // filter coefficient and LFO phase step in here assumes a fixed dt.
        // Re-anchor instead and count it as one overrun.
        if (absolute_time_diff_us (next, get_absolute_time()) > (int64_t) period_us * 4)
        {
            next = make_timeout_time_us (period_us);
            ++gws.overruns;
        }

        gw_control_tick ((float) period_us * 1e-6f);

        // Let the console change ctrl_rate_hz on the fly without a reboot.
        if (++rate_check >= 256)
        {
            rate_check = 0;
            const uint32_t want = (uint32_t) (1000000.0f / (float) gwt.ctrl_rate_hz);
            if (want != period_us && want >= 250 && want <= 5000)
            {
                period_us = want;
                next = make_timeout_time_us (period_us);
            }
        }
    }
}
