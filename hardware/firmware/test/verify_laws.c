// ============================================================================
//  verify_laws.c — host-side check that the firmware's knob laws match the
//  plugin's, and that the FREQ cap-range machinery behaves.
//
//  Build and run on a PC (no Pico needed):
//      cc -O2 -I.. -I../src -Itest test/verify_laws.c src/tunables.c -lm \
//         -o /tmp/verify && /tmp/verify
// ============================================================================
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "tunables.h"
#include "tapers.h"

static int fails = 0;

static void ck (const char* what, double got, double want, double tol)
{
    const double err = fabs (got - want);
    const int ok = err <= tol;
    printf ("  [%s] %-42s got %-12.6g want %-12.6g\n",
            ok ? "PASS" : "FAIL", what, got, want);
    if (! ok) ++fails;
}

static void ckb (const char* what, int ok, const char* detail)
{
    printf ("  [%s] %-42s %s\n", ok ? "PASS" : "FAIL", what, detail ? detail : "");
    if (! ok) ++fails;
}

// ---- the plugin's own laws, transcribed straight from Glitchwave567.h ------
static double plugin_audio_taper (double x) { return (pow (2.0, x * 6.33985) - 1.0) / 80.0; }
static double plugin_f0          (double x) { return 0.2  * pow (30000.0,  x); }
static double plugin_dirt        (double x) { return 1.1  * pow (272.727,  x); }
static double plugin_fizz_hi     (double x) { return 44.2 * pow (200.0,    x); }
static double plugin_q           (double x) { return 0.25 * pow (32.0,     x); }

int main (void)
{
    gw_tunables_defaults (&gwt);

    printf ("\n=== Glitchwave 567 firmware law verification ===\n");
    printf ("sizeof(GwTunables) = %zu bytes, %d settings\n\n",
            sizeof (GwTunables), gw_tunable_count);

    // ---- 1. VOL audio taper is the plugin's exact law ----------------------
    printf ("VOL audio taper -- (81^x - 1)/80\n");
    ck ("vol(0.0)", gw_law_vol (0.0f), 0.0,  1e-6);
    ck ("vol(0.5) = 10%% at half rotation", gw_law_vol (0.5f), 0.1, 1e-5);
    ck ("vol(1.0)", gw_law_vol (1.0f), 1.0,  1e-6);
    for (int i = 0; i <= 10; ++i)
    {
        const float x = (float) i / 10.0f;
        char n[64]; snprintf (n, sizeof n, "vol(%.1f) vs plugin", (double) x);
        ck (n, gw_law_vol (x), plugin_audio_taper (x), 2e-5);
    }

    // ---- 2. FREQ span matches the plugin -----------------------------------
    printf ("\nFREQ -- plugin span 0.2 Hz .. 6 kHz\n");
    for (int i = 0; i <= 10; ++i)
    {
        const float x = (float) i / 10.0f;
        const GwFreqResult r = gw_law_freq (x, 0);
        char n[64]; snprintf (n, sizeof n, "freq(%.1f) Hz vs plugin", (double) x);
        ck (n, r.f_hz, plugin_f0 (x), plugin_f0 (x) * 1e-4);
    }

    // ---- 3. FREQ cap ranges: full coverage, no dead CV ---------------------
    printf ("\nFREQ cap-range machinery\n");
    {
        int band = 0, worst_range = -1;
        int cv_pegged_lo = 0, cv_pegged_hi = 0, bad = 0;
        int range_seen[GW_NUM_FRANGE] = { 0 };
        float prev_f = -1.0f;
        int   monotonic = 1;

        for (int i = 0; i <= 4000; ++i)
        {
            const float x = (float) i / 4000.0f;
            const GwFreqResult r = gw_law_freq (x, band);
            band = (GW_NUM_FRANGE - 1) - r.range;

            if (r.f_hz < prev_f) monotonic = 0;
            prev_f = r.f_hz;

            if (r.cv < 0.0f || r.cv > 1.0f) ++bad;
            if (r.cv <= 0.0f) ++cv_pegged_lo;
            if (r.cv >= 1.0f) ++cv_pegged_hi;
            if (r.range < 0 || r.range >= GW_NUM_FRANGE) worst_range = r.range;
            else range_seen[r.range] = 1;
        }
        ckb ("frequency is monotonic across the sweep", monotonic, NULL);
        ckb ("CV always inside 0..1", bad == 0, NULL);
        ckb ("no range index out of bounds", worst_range < 0, NULL);
        {
            int all = 1;
            for (int r = 0; r < GW_NUM_FRANGE; ++r) if (! range_seen[r]) all = 0;
            char d[96];
            snprintf (d, sizeof d, "470u:%d 22u:%d 1u:%d 47n:%d",
                      range_seen[FRANGE_470U], range_seen[FRANGE_22U],
                      range_seen[FRANGE_1U],   range_seen[FRANGE_47N]);
            ckb ("all four timing caps get used", all, d);
        }
        {
            // A CV pegged at a rail for a long stretch means a dead zone in the
            // knob -- the band limits and the requested frequency disagree.
            char d[96];
            snprintf (d, sizeof d, "%d ticks at 0, %d at 1, of 4001",
                      cv_pegged_lo, cv_pegged_hi);
            ckb ("CV not pegged for long stretches",
                 cv_pegged_lo < 60 && cv_pegged_hi < 60, d);
        }
    }

    // ---- 4. FREQ hysteresis: no oscillation while dithering on a boundary --
    printf ("\nFREQ hysteresis (the cap-swap click test)\n");
    {
        // Find a boundary, then jitter the knob across it and count swaps.
        // Without hysteresis this chatters; with it, at most one swap.
        float lo, hi;
        gw_freq_band_limits (1, &lo, &hi);
        const float f_edge = lo;
        const float x_edge = logf (f_edge / gwt.freq_lo_hz)
                           / logf (gwt.freq_hi_hz / gwt.freq_lo_hz);

        int band = 1, swaps = 0, last = 1;
        for (int i = 0; i < 400; ++i)
        {
            const float jitter = ((i % 2) ? 1.0f : -1.0f) * 0.0015f;
            const GwFreqResult r = gw_law_freq (x_edge + jitter, band);
            band = (GW_NUM_FRANGE - 1) - r.range;
            if (band != last) { ++swaps; last = band; }
        }
        char d[64]; snprintf (d, sizeof d, "%d swaps over 400 jitters", swaps);
        ckb ("hysteresis stops cap chatter", swaps <= 1, d);
    }

    // ---- 5. GAIN -----------------------------------------------------------
    printf ("\nGAIN -- plugin x1.1 .. x300\n");
    ck ("gain(0.0)", gw_law_gain_x (0.0f), 1.1,   1e-4);
    ck ("gain(1.0)", gw_law_gain_x (1.0f), 300.0, 0.05);
    for (int i = 1; i < 10; ++i)
    {
        const float x = (float) i / 10.0f;
        char n[64]; snprintf (n, sizeof n, "gain(%.1f) vs plugin", (double) x);
        ck (n, gw_law_gain_x (x), plugin_dirt (x), plugin_dirt (x) * 2e-4);
    }

    // ---- 6. SVF ------------------------------------------------------------
    printf ("\nSVF cutoff and Q\n");
    ck ("svf_fc(0.0) Hz", gw_law_svf_fc_hz (0.0f), 20.0,   1e-3);
    ck ("svf_fc(1.0) Hz", gw_law_svf_fc_hz (1.0f), 8800.0, 0.5);
    ck ("svf_q(0.0)",     gw_law_svf_q (0.0f),     0.25,   1e-5);
    ck ("svf_q(1.0)",     gw_law_svf_q (1.0f),     8.0,    1e-3);
    for (int i = 0; i <= 10; ++i)
    {
        const float x = (float) i / 10.0f;
        char n[64]; snprintf (n, sizeof n, "svf_q(%.1f) vs plugin", (double) x);
        ck (n, gw_law_svf_q (x), plugin_q (x), plugin_q (x) * 2e-4);
    }
    // The plugin's Hi-range fizz sweep, for reference against the wider SVF.
    printf ("    (plugin Hi-range fizz for reference: %.0f Hz at 0, %.0f at 0.65, %.0f at 1)\n",
            plugin_fizz_hi (0.0), plugin_fizz_hi (0.65), plugin_fizz_hi (1.0));

    // ---- 7. MIX constant power --------------------------------------------
    printf ("\nMIX constant-power crossfade\n");
    {
        float w, d;
        gw_law_mix (0.5f, &w, &d);
        ck ("mix(0.5) wet", w, 0.70710678, 1e-5);
        ck ("mix(0.5) dry", d, 0.70710678, 1e-5);
        gw_law_mix (0.0f, &w, &d);
        ck ("mix(0.0) wet", w, 0.0, 1e-6);
        ck ("mix(0.0) dry", d, 1.0, 1e-6);
        gw_law_mix (1.0f, &w, &d);
        ck ("mix(1.0) wet", w, 1.0, 1e-6);
        ck ("mix(1.0) dry", d, 0.0, 1e-6);

        int flat = 1;
        double worst = 0.0;
        for (int i = 0; i <= 100; ++i)
        {
            gw_law_mix ((float) i / 100.0f, &w, &d);
            const double p = (double) w * w + (double) d * d;
            if (fabs (p - 1.0) > worst) worst = fabs (p - 1.0);
            if (fabs (p - 1.0) > 1e-5) flat = 0;
        }
        char det[64]; snprintf (det, sizeof det, "worst power error %.2e", worst);
        ckb ("power constant across the whole sweep", flat, det);
    }

    // ---- 8. GATE x VOL product -------------------------------------------
    printf ("\nGATE x VOL share one VCA\n");
    ck ("vol 1.0, gate closed", gw_law_gate_vol_cv (1.0f, 0.0f), 0.0, 1e-6);
    ck ("vol 1.0, gate open",   gw_law_gate_vol_cv (1.0f, 1.0f), 1.0, 1e-6);
    ck ("vol 0.5, gate open",   gw_law_gate_vol_cv (0.5f, 1.0f), 0.1, 1e-5);
    ck ("vol 0.5, gate half",   gw_law_gate_vol_cv (0.5f, 0.5f), 0.05, 1e-5);

    // ---- 9. trims ---------------------------------------------------------
    printf ("\nGate trims RV1/RV2/RV3\n");
    ck ("RV1 CCW threshold", gw_trim_threshold (0.0f), 0.01,  1e-6);
    ck ("RV1 CW  threshold", gw_trim_threshold (1.0f), 0.60,  1e-6);
    ck ("RV2 CCW hold ms",   gw_trim_hold_ms   (0.0f), 0.0,   1e-6);
    ck ("RV2 CW  hold ms",   gw_trim_hold_ms   (1.0f), 500.0, 1e-3);
    ck ("RV3 CCW fade ms",   gw_trim_fade_ms   (0.0f), 1.0,   1e-6);
    ck ("RV3 CW  fade ms",   gw_trim_fade_ms   (1.0f), 300.0, 1e-3);

    // ---- 10. rail compensation -------------------------------------------
    printf ("\nRail awareness (VA_SENSE)\n");
    ck ("scale at nominal 9 V",  gw_rail_scale (9.0f),  1.0,      1e-6);
    ck ("scale at 18 V",         gw_rail_scale (18.0f), 0.5,      1e-6);
    ck ("scale at 12 V",         gw_rail_scale (12.0f), 0.75,     1e-6);
    ck ("scale when unpowered",  gw_rail_scale (0.0f),  1.0,      1e-6);

    // ---- 11. one-pole coefficients ---------------------------------------
    printf ("\nOne-pole time constants at 1 kHz\n");
    ck ("10 ms coeff", gw_onepole_coeff (10.0f, 1000.0f), 1.0 - exp (-0.1), 1e-6);
    ck ("1 ms coeff",  gw_onepole_coeff (1.0f,  1000.0f), 1.0 - exp (-1.0), 1e-6);

    // ---- 12. the tunables table itself ------------------------------------
    printf ("\nSettings table integrity\n");
    {
        int dup = 0, out_of_range = 0;
        for (int i = 0; i < gw_tunable_count; ++i)
        {
            for (int j = i + 1; j < gw_tunable_count; ++j)
                if (strcmp (gw_tunable_info[i].name, gw_tunable_info[j].name) == 0) ++dup;
            const float d = gw_tunable_info[i].defval;
            if (d < gw_tunable_info[i].minv || d > gw_tunable_info[i].maxv)
            {
                printf ("        %s default %g outside %g..%g\n",
                        gw_tunable_info[i].name, (double) d,
                        (double) gw_tunable_info[i].minv,
                        (double) gw_tunable_info[i].maxv);
                ++out_of_range;
            }
        }
        ckb ("no duplicate setting names", dup == 0, NULL);
        ckb ("every default is inside its own range", out_of_range == 0, NULL);

        // every default must survive a defaults -> get round trip
        int mismatch = 0;
        for (int i = 0; i < gw_tunable_count; ++i)
        {
            const float got  = gw_tunable_get (&gwt, i);
            const float want = gw_tunable_info[i].defval;
            const float tol  = (gw_tunable_info[i].kind == GW_KIND_FLOAT)
                             ? fabsf (want) * 1e-6f + 1e-9f : 0.001f;
            if (fabsf (got - want) > tol)
            {
                printf ("        %s got %g want %g\n",
                        gw_tunable_info[i].name, (double) got, (double) want);
                ++mismatch;
            }
        }
        ckb ("defaults round-trip through get/set", mismatch == 0, NULL);

        // out-of-range writes must be refused, not clamped silently
        const int vi = gw_tunable_find ("led_brightness");
        const float before = gw_tunable_get (&gwt, vi);
        const int refused = ! gw_tunable_set (&gwt, vi, 9.0f);
        ckb ("out-of-range set is refused",
             refused && gw_tunable_get (&gwt, vi) == before, NULL);
        ckb ("unknown setting name returns -1", gw_tunable_find ("nope") == -1, NULL);
    }

    // ---- 13. PWM slice allocation: no two CVs may share a slice+channel ---
    printf ("\nPWM slice allocation (RP2040: slice = gpio/2, chan = gpio&1)\n");
    {
        int used[8][2];
        memset (used, 0, sizeof (used));
        int clash = 0;
        for (int i = 0; i < GW_NUM_CV; ++i)
        {
            const int pin = GW_CV_PIN[i];
            const int sl  = (pin >> 1) & 7;
            const int chn = pin & 1;
            if (used[sl][chn]) ++clash;
            used[sl][chn] = 1;
        }
        char d[64]; snprintf (d, sizeof d, "%d CV pins across slices 0-4", GW_NUM_CV);
        ckb ("no two CVs collide on a slice channel", clash == 0, d);
    }

    // ---- 14. the inverted-bit truth table --------------------------------
    printf ("\nLevel-shifted select bits\n");
    {
        // gw_write_shifted inverts. Verify the SVF truth table is 0..3 and the
        // FREQ range mapping puts the biggest cap on the lowest band.
        int ok = 1;
        for (int m = 0; m < GW_NUM_SVF_MODE; ++m)
            if (GW_SVF_BITS[m] != (uint8_t) m) ok = 0;
        ckb ("FMODE bits are the mode index", ok, "LP=000 BP=001 HP=010 Notch=011");

        const GwFreqResult lowest  = gw_law_freq (0.0f, 0);
        const GwFreqResult highest = gw_law_freq (1.0f, GW_NUM_FRANGE - 1);
        ckb ("lowest pitch selects the biggest cap",
             lowest.range == FRANGE_470U, "470u");
        ckb ("highest pitch selects the smallest cap",
             highest.range == FRANGE_47N, "47n");
    }

    // ---- 15. NaN must never pass the guard rails -------------------------
    // A person can type `set vol_taper_base nan` and strtof will parse it.
    // NaN fails every comparison, so a naive `v < min || v > max` check lets it
    // straight through -- and then a NaN clamps to the UPPER bound, which on a
    // CV means 100 % duty: full volume with the gate forced open.
    printf ("\nNaN and Inf rejection\n");
    {
        const float nan_v = (float) NAN;
        const float inf_v = (float) INFINITY;
        const int   vi    = gw_tunable_find ("vol_taper_base");
        const float keep  = gw_tunable_get (&gwt, vi);

        ckb ("set NaN is refused",       ! gw_tunable_set (&gwt, vi, nan_v), NULL);
        ckb ("value unchanged after NaN", gw_tunable_get (&gwt, vi) == keep, NULL);
        ckb ("set +Inf is refused",      ! gw_tunable_set (&gwt, vi, inf_v), NULL);
        ckb ("set -Inf is refused",      ! gw_tunable_set (&gwt, vi, -inf_v), NULL);

        // And prove the clamp direction, because -ffast-math silently inverts
        // it: gw_clampf(NaN, 0, 1) must NOT come out as 1.0.
        const float c = gw_clampf (nan_v, 0.0f, 1.0f);
        char d[64]; snprintf (d, sizeof d, "gw_clampf(NaN,0,1) = %g", (double) c);
        ckb ("clamping a NaN does not yield full scale", !(c >= 1.0f), d);
    }

    printf ("\n=== %s: %d failure%s ===\n\n",
            fails ? "PROBLEMS" : "ALL LAWS VERIFIED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
