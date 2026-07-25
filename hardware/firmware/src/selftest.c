// ============================================================================
//  selftest.c
// ============================================================================
#include "selftest.h"
#include "board.h"
#include "hw_io.h"
#include "control.h"
#include "stomps.h"
#include "tunables.h"
#include "tapers.h"

#include "pico/stdlib.h"
#include <stdio.h>
#include <math.h>

static int s_fails = 0;

static void check (const char* what, bool ok, const char* detail)
{
    printf ("  [%s] %-26s %s\n", ok ? "PASS" : "FAIL", what, detail ? detail : "");
    if (! ok) ++s_fails;
}

void gw_selftest_run (void)
{
    char buf[96];
    s_fails = 0;
    printf ("\n=== Glitchwave 567 self-test ===\n");

    // Take the hardware off core 0 for the duration, or every reading below is
    // whatever channel the control loop happened to leave the mux on.
    gw_control_standdown();
    if (! gw_control_is_stood_down())
        printf ("  WARNING: the control loop did not stand down. Results are suspect.\n");

    if (gwt.bench_mode)
        printf ("  NOTE: bench_mode is 1, so the analog checks below are meaningless.\n"
                "        Set bench_mode 0 once the pedal is assembled.\n");

    // ---- 1. ADC zero on the grounded spares -------------------------------
    int sum = 0;
    int worst = 0;
    for (int ch = MUX_GND_12; ch <= MUX_GND_15; ++ch)
    {
        const int v = (int) gw_adc_read_mux (ch);
        sum += v;
        if (v > worst) worst = v;
    }
    const int zero = sum / 4;
    snprintf (buf, sizeof (buf), "mean %d counts (worst %d)", zero, worst);
    check ("ADC zero (C12-C15)", worst < 60, buf);

    // ---- 2. rail present ---------------------------------------------------
    const float va01 = (float) gw_adc_read_mux (MUX_VA_SENSE) / 4095.0f;
    const float va   = va01 * gwt.cal_va_scale;
    snprintf (buf, sizeof (buf), "%.2f V (VA_SENSE %.3f)", (double) va, (double) va01);
    check ("VA rail", va > 7.5f && va < 20.0f, buf);

    // ---- 3. every mux channel answers -------------------------------------
    bool all_live = true;
    for (int ch = 0; ch < GW_MUX_LIVE; ++ch)
    {
        const uint16_t v = gw_adc_read_mux (ch);
        // A channel stuck hard at either rail on EVERY read is the signature of
        // an open 4067 pin or a cold joint. Pots legitimately sit at the rails,
        // so this is a soft warning for the pot channels only.
        const bool suspicious = (v < 3 || v > 4092);
        const bool is_pot = (ch <= MUX_POT6_VOL) || (ch >= MUX_TRIM_TH);
        printf ("      C%-2d %-11s %4u%s\n", ch, GW_MUX_NAME[ch], v,
                suspicious ? (is_pot ? "   (at a rail - fine if the knob is)" : "   <-- SUSPICIOUS") : "");
        if (suspicious && ! is_pot) all_live = false;
    }
    check ("mux channels", all_live, "sense channels all off the rails");

    // ---- 4. footswitches ---------------------------------------------------
    const bool s1 = gw_stomp_raw (1);
    const bool s2 = gw_stomp_raw (2);
    snprintf (buf, sizeof (buf), "STOMP1 %s  STOMP2 %s",
              s1 ? "DOWN" : "up", s2 ? "DOWN" : "up");
    check ("footswitches released", !s1 && !s2, buf);

    // ---- 5. select bits ----------------------------------------------------
    // Nothing to measure without a scope, but exercise them so a stuck shifter
    // shows up as an audible cap swap / filter jump while you watch.
    for (int r = 0; r < GW_NUM_FRANGE; ++r) { gw_set_freq_range (r); sleep_ms (60); }
    gw_set_freq_range (gws.freq_range);
    for (int m = 0; m < GW_NUM_SVF_MODE; ++m) { gw_set_svf_mode (m); sleep_ms (120); }
    gw_set_svf_mode (gws.svf_mode);
    check ("select bits exercised", true, "FREQ_A/B and FMODE_A/B/C toggled (inverted)");

    // ---- 6. PWM CVs --------------------------------------------------------
    // Quick triangle on each so a scope on the RC'd node shows a clean ramp.
    for (int i = 0; i < GW_NUM_CV; ++i)
    {
        for (int k = 0; k <= 20; ++k) { gw_pwm_write (i, (float) k / 20.0f); sleep_ms (4); }
        gw_pwm_write (i, gws.cv[i]);
    }
    check ("PWM CV sweep", true, "all 10 ramped 0->100% (scope the RC'd side)");

    // ---- 7. loop timing ----------------------------------------------------
    const uint32_t period = (uint32_t) (1000000.0f / (float) gwt.ctrl_rate_hz);
    snprintf (buf, sizeof (buf), "last %lu us, max %lu us, budget %lu us, %lu overruns",
              (unsigned long) gws.loop_us_last, (unsigned long) gws.loop_us_max,
              (unsigned long) period, (unsigned long) gws.overruns);
    check ("control loop timing", gws.loop_us_max < period, buf);

    gw_control_resume();

    printf ("=== %s (%d failure%s) ===\n\n",
            s_fails ? "PROBLEMS FOUND" : "ALL GOOD", s_fails, s_fails == 1 ? "" : "s");
}

void gw_selftest_sweep_cv (int cv, int seconds)
{
    if (cv < 0 || cv >= GW_NUM_CV) { printf ("no such CV. 0..%d\n", GW_NUM_CV - 1); return; }
    if (seconds < 1) seconds = 1;
    if (seconds > 60) seconds = 60;

    printf ("sweeping %s (GP%d) for %d s -- scope the filtered side.\n",
            GW_CV_NAME[cv], GW_CV_PIN[cv], seconds);

    gw_control_standdown();          // otherwise the loop overwrites every step

    const int steps = seconds * 100;
    for (int k = 0; k < steps; ++k)
    {
        const float p = (float) k / (float) steps;
        const float tri = p < 0.5f ? p * 2.0f : 2.0f - p * 2.0f;
        gw_pwm_write (cv, tri);
        sleep_ms (10);
    }
    gw_pwm_write (cv, gws.cv[cv]);
    gw_control_resume();
    printf ("done, %s handed back to the control loop.\n", GW_CV_NAME[cv]);
}

void gw_selftest_leds (void)
{
    static const char* const names[GW_NUM_LEDS] =
        { "1 section A", "2 section B", "3 section C", "4 tempo", "5 bypass", "6 gate" };

    printf ("LED walk. Chain order should be: A, B, C, tempo, bypass, gate.\n");
    for (int i = 0; i < GW_NUM_LEDS; ++i)
    {
        printf ("  LED %s ... red, green, blue\n", names[i]);
        for (int c = 0; c < 3; ++c)
        {
            GwRgb px[GW_NUM_LEDS] = { 0 };
            if (c == 0) px[i].r = 255;
            if (c == 1) px[i].g = 255;
            if (c == 2) px[i].b = 255;
            gw_leds_set (px);
            // We are running inside the console, which lives in the same core 1
            // loop that normally calls gw_leds_service(). Nothing would ever be
            // sent while this command runs unless we push the frame ourselves.
            gw_leds_service();
            sleep_ms (350);
        }
    }
    GwRgb all[GW_NUM_LEDS];
    for (int i = 0; i < GW_NUM_LEDS; ++i) { all[i].r = all[i].g = all[i].b = 160; }
    gw_leds_set (all);
    gw_leds_service();
    sleep_ms (600);
    printf ("If any LED lit out of order, the chain is wired differently than\n"
            "gen_ctrl.py says -- fix the header, not the firmware.\n");
}

// ===========================================================================
//  Calibration
// ===========================================================================
void gw_cal_adc_zero (void)
{
    gw_control_standdown();
    int sum = 0;
    const int n = 16;
    for (int k = 0; k < n; ++k)
        for (int ch = MUX_GND_12; ch <= MUX_GND_15; ++ch)
            sum += (int) gw_adc_read_mux (ch);

    gw_control_resume();

    const int zero = sum / (n * 4);
    const int idx  = gw_tunable_find ("cal_adc_offset");
    gw_tunable_set (&gwt, idx, (float) zero);
    printf ("cal_adc_offset = %d counts. `save` to keep it.\n", zero);
}

void gw_cal_va (float measured_volts)
{
    if (measured_volts < 5.0f || measured_volts > 24.0f)
    {
        printf ("that doesn't look like a pedal rail. Give me 5..24.\n");
        return;
    }
    gw_control_standdown();
    float acc = 0.0f;
    for (int k = 0; k < 32; ++k) acc += (float) gw_adc_read_mux (MUX_VA_SENSE);
    gw_control_resume();
    const float va01 = (acc / 32.0f) / 4095.0f;
    if (va01 < 0.02f)
    {
        printf ("VA_SENSE reads ~0. Is the pedal actually powered?\n");
        return;
    }
    const float scale = measured_volts / va01;
    gw_tunable_set (&gwt, gw_tunable_find ("cal_va_scale"),   scale);
    gw_tunable_set (&gwt, gw_tunable_find ("cal_va_nominal"), measured_volts);
    printf ("cal_va_scale = %.3f  (VA_SENSE %.4f == %.2f V)\n",
            (double) scale, (double) va01, (double) measured_volts);
    printf ("cal_va_nominal = %.2f -- the laws are now referenced to this rail.\n"
            "`save` to keep it.\n", (double) measured_volts);
}

void gw_cal_pot_endpoints (void)
{
    printf ("\nPot endpoint learn.\n"
            "  1. Turn ALL SIX knobs fully counter-clockwise, then press Enter.\n");
    while (getchar_timeout_us (0) != PICO_ERROR_TIMEOUT) { }        // drain
    while (true)
    {
        const int ch = getchar_timeout_us (100000);
        if (ch == '\r' || ch == '\n') break;
    }

    // The window has to be the range EVERY pot can reach, so take the HIGHEST
    // of the fully-CCW readings and (below) the LOWEST of the fully-CW ones.
    // Taking the extremes the other way round makes every pot's real travel a
    // subset of the window, so no knob ever quite reaches 0 or 1.
    gw_control_standdown();
    int lo = 0;
    for (int p = 0; p < GW_NUM_POTS; ++p)
    {
        const int v = (int) gw_adc_read_mux (p);
        printf ("     %-10s %4d\n", GW_MUX_NAME[p], v);
        if (v > lo) lo = v;
    }
    gw_control_resume();

    printf ("  2. Now turn ALL SIX fully clockwise, then press Enter.\n");
    while (true)
    {
        const int ch = getchar_timeout_us (100000);
        if (ch == '\r' || ch == '\n') break;
    }

    gw_control_standdown();
    int hi = 4095;
    for (int p = 0; p < GW_NUM_POTS; ++p)
    {
        const int v = (int) gw_adc_read_mux (p);
        printf ("     %-10s %4d\n", GW_MUX_NAME[p], v);
        if (v < hi) hi = v;
    }
    gw_control_resume();

    if (hi - lo < 500)
    {
        printf ("Only %d counts between the ends -- that's not a working pot sweep.\n"
                "Nothing saved. Check the 4067 wiring.\n", hi - lo);
        return;
    }

    gw_tunable_set (&gwt, gw_tunable_find ("cal_pot_lo"), (float) lo);
    gw_tunable_set (&gwt, gw_tunable_find ("cal_pot_hi"), (float) hi);
    gw_tunable_set (&gwt, gw_tunable_find ("cal_done"),   1.0f);
    printf ("cal_pot_lo = %d, cal_pot_hi = %d. `save` to keep it.\n", lo, hi);
}
