// ============================================================================
//  tapers.h — the knob laws, mirrored from the plugin so the pedal feels like
//  the sim you signed off on (v0.32 behaviour).
//
//  Every law reads its numbers from gwt (tunables.h), so the console can
//  reshape any of them live.
// ============================================================================
#pragma once

#include <math.h>
#include "tunables.h"
#include "board.h"

static inline float gw_clampf (float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// A curve knob: 1.0 passes straight through, >1 gives finer control low down,
// <1 finer control up top. Used on every CV whose OTA transfer is unmeasured.
static inline float gw_curve (float x, float c)
{
    x = gw_clampf (x, 0.0f, 1.0f);
    if (c > 0.999f && c < 1.001f) return x;
    return powf (x, 1.0f / c);
}

// ---------------------------------------------------------------------------
//  VOL — audio taper on a LINEAR B10k pot.
//      a = (base^x - 1) / (base - 1)
//  base 81 is the plugin's exact law: 10% output at half rotation.
// ---------------------------------------------------------------------------
static inline float gw_law_vol (float x)
{
    x = gw_clampf (x, 0.0f, 1.0f);
    const float b = gwt.vol_taper_base;
    return (powf (b, x) - 1.0f) / (b - 1.0f);
}

// ---------------------------------------------------------------------------
//  FREQ — the 567 pitch, 0.2 Hz .. 6 kHz exponential (plugin span).
//
//  The LM567 gets there in four bites: FREQ_A/B pick one of four CD4052 timing
//  caps (47n / 1u / 22u / 470u) and the OTA sweeps LINEARLY in current, hence
//  roughly linearly in frequency, inside that cap's window.
//
//  So: pot -> the frequency you WANT (exponential), then pick the cap whose
//  window contains it, then the CV is that frequency's LINEAR position in the
//  window. Physically correct, and it puts the exponential feel in software
//  where it belongs.
//
//  Bigger cap = lower frequency, so band 0 (lowest) uses FRANGE_470U.
// ---------------------------------------------------------------------------
typedef struct GwFreqResult {
    float f_hz;      // the pitch this pot position asks for
    int   range;     // GwFreqRange to drive onto FREQ_A/B (logical, pre-invert)
    float cv;        // 0..1 for PWM_FREQ
} GwFreqResult;

// Geometric ratio of one cap band, before overlap. Four bands over the span.
static inline float gw_freq_band_ratio (void)
{
    return powf (gwt.freq_hi_hz / gwt.freq_lo_hz, 1.0f / (float) GW_NUM_FRANGE);
}

// The USABLE limits of one cap band -- the range its OTA sweep actually
// covers. Neighbouring bands deliberately overlap, and that overlap IS the
// hysteresis: inside it either cap can make the note, so the firmware can
// stay on the one it is already using instead of swapping back and forth.
// freq_range_hyst just widens the overlap further. Both must go into the SAME
// limits used for the CV mapping -- if the leave-test used a wider window than
// the CV mapping, the CV would sit pegged at 1.0 across the difference and the
// knob would have a dead spot where the pitch stops rising.
static inline void gw_freq_band_limits (int band, float* lo, float* hi)
{
    const float r  = gw_freq_band_ratio();
    const float ov = gwt.freq_range_overlap + gwt.freq_range_hyst;
    float l = gwt.freq_lo_hz * powf (r, (float) band - ov);
    float h = gwt.freq_lo_hz * powf (r, (float) band + 1.0f + ov);
    if (band == 0)                  l = gwt.freq_lo_hz;
    if (band == GW_NUM_FRANGE - 1)  h = gwt.freq_hi_hz;
    *lo = l; *hi = h;
}

// `cur_band` is the band we are already in; passing it in is what gives the
// hysteresis. Cap swapping makes a click — and on this pedal clicks ARE part
// of the voice, so the only real sin is oscillating between two caps.
static inline GwFreqResult gw_law_freq (float x, int cur_band)
{
    GwFreqResult out;
    x = gw_clampf (x, 0.0f, 1.0f);

    const float f = gwt.freq_lo_hz * powf (gwt.freq_hi_hz / gwt.freq_lo_hz, x);
    out.f_hz = f;

    if (cur_band < 0 || cur_band >= GW_NUM_FRANGE) cur_band = 0;

    float lo, hi;
    gw_freq_band_limits (cur_band, &lo, &hi);

    // Only leave the current band once the note is genuinely outside what this
    // cap can reach. Everywhere inside the overlap we stay put.
    int band = cur_band;
    if (f < lo)
    {
        while (band > 0)
        {
            --band;
            gw_freq_band_limits (band, &lo, &hi);
            if (f >= lo) break;
        }
    }
    else if (f > hi)
    {
        while (band < GW_NUM_FRANGE - 1)
        {
            ++band;
            gw_freq_band_limits (band, &lo, &hi);
            if (f <= hi) break;
        }
    }

    gw_freq_band_limits (band, &lo, &hi);
    const float span = (hi - lo) > 1e-9f ? (hi - lo) : 1e-9f;
    out.cv    = gw_curve (gw_clampf ((f - lo) / span, 0.0f, 1.0f), gwt.freq_cv_curve);
    out.range = (GW_NUM_FRANGE - 1) - band;    // biggest cap for the lowest band
    return out;
}

// ---------------------------------------------------------------------------
//  GAIN — the dirt, x1.1 .. x300 log (plugin v0.19+ law).
//  The VCA attenuator sits before the fixed x300 stage.
// ---------------------------------------------------------------------------
static inline float gw_law_gain_x (float x)      // the actual multiplier
{
    x = gw_clampf (x, 0.0f, 1.0f);
    return gwt.gain_min * powf (gwt.gain_max / gwt.gain_min, x);
}

static inline float gw_law_gain_cv (float x)     // what PWM_DIRT gets
{
    return gw_curve (gw_clampf (x, 0.0f, 1.0f), gwt.dirt_cv_curve);
}

// ---------------------------------------------------------------------------
//  MIX — constant-power crossfade across the two VCAs. Both at 0.707 in the
//  middle, so passing through centre doesn't dip.
// ---------------------------------------------------------------------------
static inline void gw_law_mix (float x, float* wet_cv, float* dry_cv)
{
    x = gw_clampf (x, 0.0f, 1.0f);
    float w, d;
    if (gwt.mix_const_power)
    {
        const float a = x * 1.5707963f;          // 0 .. pi/2
        w = sinf (a);
        d = cosf (a);
    }
    else
    {
        w = x;
        d = 1.0f - x;
    }
    *wet_cv = gw_clampf (w * gwt.mix_wet_trim, 0.0f, 1.0f);
    *dry_cv = gw_clampf (d * gwt.mix_dry_trim, 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
//  FIZZ / Q — the CV-controlled SVF that replaced the plugin's dual-gang
//  Sallen-Key. Cutoff 20 Hz .. 8.8 kHz exponential, Q 0.25 .. 8 exponential.
// ---------------------------------------------------------------------------
static inline float gw_law_svf_fc_hz (float x)
{
    x = gw_clampf (x, 0.0f, 1.0f);
    return gwt.svf_lo_hz * powf (gwt.svf_hi_hz / gwt.svf_lo_hz, x);
}

static inline float gw_law_svf_fc_cv (float x)
{
    return gw_curve (gw_clampf (x, 0.0f, 1.0f), gwt.svff_cv_curve);
}

static inline float gw_law_svf_q (float x)
{
    x = gw_clampf (x, 0.0f, 1.0f);
    return gwt.svf_q_lo * powf (gwt.svf_q_hi / gwt.svf_q_lo, x);
}

static inline float gw_law_svf_q_cv (float x)
{
    return gw_curve (gw_clampf (x, 0.0f, 1.0f), gwt.svfq_cv_curve);
}

// ---------------------------------------------------------------------------
//  GATE x VOL — ONE VCA does both jobs on this board, so the product is
//  computed here in firmware and sent as a single CV.
// ---------------------------------------------------------------------------
static inline float gw_law_gate_vol_cv (float vol_pos, float gate_env)
{
    const float a = gw_law_vol (vol_pos) * gw_clampf (gate_env, 0.0f, 1.0f);
    return gw_curve (gw_clampf (a * gwt.vol_max, 0.0f, 1.0f), gwt.gate_cv_curve);
}

// ---------------------------------------------------------------------------
//  Trim pots (RV1/RV2/RV3) -> gate parameters, each over its own range.
// ---------------------------------------------------------------------------
static inline float gw_lerp (float a, float b, float x) { return a + (b - a) * gw_clampf (x, 0.0f, 1.0f); }

static inline float gw_trim_threshold (float x) { return gw_lerp (gwt.gate_thresh_lo,  gwt.gate_thresh_hi,  x); }
static inline float gw_trim_hold_ms   (float x) { return gw_lerp (gwt.gate_hold_lo_ms, gwt.gate_hold_hi_ms, x); }
static inline float gw_trim_fade_ms   (float x) { return gw_lerp (gwt.gate_fade_lo_ms, gwt.gate_fade_hi_ms, x); }

// ---------------------------------------------------------------------------
//  Rail awareness. VA_SENSE tells us whether we're on 9 V or 18 V; the laws
//  above were dialled in at cal_va_nominal. Anything headroom-dependent gets
//  scaled by this so a 12 V adapter doesn't quietly change the voicing.
// ---------------------------------------------------------------------------
static inline float gw_rail_volts (float va_sense_01)
{
    return gw_clampf (va_sense_01, 0.0f, 1.0f) * gwt.cal_va_scale;
}

static inline float gw_rail_scale (float va_volts)
{
    if (! gwt.cal_va_compensate) return 1.0f;
    if (va_volts < 4.0f) return 1.0f;                    // not powered / bogus read
    return gw_clampf (gwt.cal_va_nominal / va_volts, 0.25f, 2.0f);
}

// ---------------------------------------------------------------------------
//  One-pole coefficient from a time constant in milliseconds.
// ---------------------------------------------------------------------------
static inline float gw_onepole_coeff (float ms, float rate_hz)
{
    if (ms < 0.01f) ms = 0.01f;
    return 1.0f - expf (-1.0f / (ms * 0.001f * rate_hz));
}
