// ============================================================================
//  control.c — the 1 kHz control loop.
//
//  Order of business each tick:
//     1. read every ADC channel
//     2. condition the pots (moving average + deadband)
//     3. read the footswitches, resolve gestures
//     4. run the gate
//     5. run the modulation layer over the knob positions
//     6. turn knob positions into the ten CV duty cycles
//     7. hand the LEDs a frame
// ============================================================================
#include "control.h"
#include "hw_io.h"
#include "stomps.h"
#include "tapers.h"
#include "tunables.h"

#include "pico/stdlib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

GwState gws;

// ---------------------------------------------------------------------------
//  Cross-core handshakes. Both are single-writer flags, so plain volatile is
//  enough on an RP2040 (aligned 32-bit accesses are atomic) -- no lock needed,
//  and no lock is WANTED on the control loop's hot path.
// ---------------------------------------------------------------------------
static volatile bool s_standdown_req  = false;   // core 1 asks
static volatile bool s_standdown_ack  = false;   // core 0 confirms it has let go

static volatile bool s_settings_req   = false;
static GwTunables    s_settings_stage;

void gw_control_standdown (void)
{
    s_standdown_req = true;
    // Wait for core 0 to finish whatever tick it is in the middle of. A few
    // tick periods is plenty; the timeout means a stalled core 0 can't hang the
    // console forever.
    for (int i = 0; i < 200 && ! s_standdown_ack; ++i)
        busy_wait_us_32 (500);
}

void gw_control_resume (void)
{
    s_standdown_req = false;
    s_standdown_ack = false;
}

bool gw_control_is_stood_down (void) { return s_standdown_ack; }

void gw_control_request_settings (const GwTunables* incoming)
{
    s_settings_stage = *incoming;
    s_settings_req   = true;
    for (int i = 0; i < 200 && s_settings_req; ++i)
        busy_wait_us_32 (500);
}

// ---------------------------------------------------------------------------
//  Pot conditioning. A moving average kills ADC dither; a deadband on top of
//  it stops the last bit from ever twitching, which is what "zipper noise"
//  actually is. Wide enough to be silent, narrow enough that slow knob moves
//  still feel continuous.
// ---------------------------------------------------------------------------
#define GW_AVG_MAX 64

typedef struct {
    uint16_t hist[GW_AVG_MAX];
    uint32_t sum;
    int      n, idx, len;
    uint16_t held;          // the value we report
    bool     primed;
} PotFilter;

static PotFilter s_pf[GW_NUM_MUX];

static void pot_filter_reset (PotFilter* f)
{
    memset (f, 0, sizeof (*f));
}

static uint16_t pot_filter_push (PotFilter* f, uint16_t v)
{
    int len = (int) gwt.pot_avg_len;
    if (len < 1) len = 1;
    if (len > GW_AVG_MAX) len = GW_AVG_MAX;

    if (len != f->len) { pot_filter_reset (f); f->len = len; }

    if (! f->primed)
    {
        for (int i = 0; i < len; ++i) f->hist[i] = v;
        f->sum = (uint32_t) v * (uint32_t) len;
        f->n = len; f->idx = 0; f->held = v; f->primed = true;
        return v;
    }

    f->sum -= f->hist[f->idx];
    f->hist[f->idx] = v;
    f->sum += v;
    f->idx = (f->idx + 1) % len;

    const uint16_t avg = (uint16_t) (f->sum / (uint32_t) len);
    const int      hy  = (int) gwt.pot_hyst_lsb;
    if (abs ((int) avg - (int) f->held) > hy) f->held = avg;
    return f->held;
}

// Raw counts -> 0..1, using the calibrated endpoints so a pot that doesn't
// quite reach the rails still gets a full sweep.
static float pot_norm (uint16_t raw)
{
    // cal_pot_lo/hi are learned from RAW counts, so they already contain the
    // ADC zero. Adding cal_adc_offset again here shifted the window a second
    // time and lopped the top off every knob.
    float lo = (float) gwt.cal_pot_lo;
    float hi = (float) gwt.cal_pot_hi;
    if (hi - lo < 100.0f) { lo = 0.0f; hi = 4095.0f; }
    return gw_clampf (((float) raw - lo) / (hi - lo), 0.0f, 1.0f);
}

// Sense channels are read straight, only the ADC zero is removed.
static float sense_norm (uint16_t raw)
{
    const float v = (float) raw - (float) gwt.cal_adc_offset;
    return gw_clampf (v / 4095.0f, 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
//  Gate — threshold / hold / fade, all three live on the internal trimpots.
// ---------------------------------------------------------------------------
typedef enum { GATE_CLOSED, GATE_OPEN, GATE_HOLDING, GATE_FADING } GatePhase;

static GatePhase s_gate_phase = GATE_CLOSED;
static float     s_gate_level = 0.0f;
static float     s_hold_left  = 0.0f;

static void gate_tick (float env, float dt, float rate_hz)
{
    if (! gwt.gate_enabled)
    {
        s_gate_level = 1.0f;
        s_gate_phase = GATE_OPEN;
        gws.gate_open = true;
        gws.gate_env  = 1.0f;
        return;
    }

    const float open_th  = gw_trim_threshold (gws.trim_th);
    const float close_th = open_th * (1.0f - gwt.gate_hyst);
    const float hold_s   = gw_trim_hold_ms (gws.trim_ho) * 0.001f;
    const float fade_ms  = gw_trim_fade_ms (gws.trim_fa);

    if (env >= open_th)
    {
        s_gate_phase = GATE_OPEN;
        s_hold_left  = hold_s;
    }
    else
    {
        // Dropping below the CLOSE threshold starts the hold. Note the hold then
        // keeps counting down even if the envelope drifts back up into the
        // hysteresis band between close and open -- only a genuine re-trigger
        // above OPEN restarts it. Gating that countdown on `env < close_th`
        // instead leaves the gate stuck open indefinitely at any signal level
        // that sits in the band, which is exactly where a decaying note sits.
        if (s_gate_phase == GATE_OPEN && env < close_th)
        {
            s_gate_phase = GATE_HOLDING;
            s_hold_left  = hold_s;
        }
        if (s_gate_phase == GATE_HOLDING)
        {
            s_hold_left -= dt;
            if (s_hold_left <= 0.0f) s_gate_phase = GATE_FADING;
        }
    }

    if (s_gate_phase == GATE_OPEN)
    {
        const float c = gw_onepole_coeff (gwt.gate_attack_ms, rate_hz);
        s_gate_level += c * (1.0f - s_gate_level);
    }
    else if (s_gate_phase == GATE_FADING)
    {
        const float c = gw_onepole_coeff (fade_ms, rate_hz);
        s_gate_level += c * (0.0f - s_gate_level);
        if (s_gate_level < 0.0005f) { s_gate_level = 0.0f; s_gate_phase = GATE_CLOSED; }
    }
    // GATE_HOLDING sustains the current level, which is the whole point.

    gws.gate_env  = gw_clampf (s_gate_level, 0.0f, 1.0f);
    gws.gate_open = (s_gate_phase == GATE_OPEN || s_gate_phase == GATE_HOLDING);
}

// ---------------------------------------------------------------------------
//  init
// ---------------------------------------------------------------------------
void gw_control_init (void)
{
    memset (&gws, 0, sizeof (gws));
    for (int i = 0; i < GW_NUM_MUX; ++i) pot_filter_reset (&s_pf[i]);

    gws.engaged  = gwt.bypass_boot_engaged ? true : false;
    gws.bypass   = gws.engaged ? 1.0f : 0.0f;
    gws.svf_mode = (int) gwt.svf_mode_boot;
    gws.freq_range = FRANGE_47N;
    gws.starve   = 0.0f;

    gw_set_svf_mode (gws.svf_mode);
    gw_mod_init();
    gw_stomps_init();
}

void gw_control_set_engaged  (bool on)  { gws.engaged = on; }
void gw_control_set_svf_mode (int mode)
{
    if (mode < 0 || mode >= GW_NUM_SVF_MODE) return;
    gws.svf_mode = mode;
    gw_set_svf_mode (mode);
}
void gw_control_reset_stats (void) { gws.loop_us_max = 0; gws.overruns = 0; }

// ---------------------------------------------------------------------------
//  Bench mode: no PCB attached, so synthesise something moving. Lets you
//  flash a bare Pico on the desk and watch the LEDs and scope the CV pins
//  before the boards ever arrive.
// ---------------------------------------------------------------------------
static void bench_fake (float dt)
{
    static float t = 0.0f;
    t += dt;
    for (int i = 0; i < GW_NUM_POTS; ++i)
        gws.pot[i] = 0.5f + 0.5f * sinf (6.2831853f * (0.05f + 0.02f * (float) i) * t);
    gws.trim_th = 0.25f; gws.trim_ho = 0.4f; gws.trim_fa = 0.3f;
    gws.env_analog = 0.5f + 0.5f * sinf (6.2831853f * 0.7f * t);
    gws.lock_sense = 0.5f + 0.5f * sinf (6.2831853f * 1.3f * t);
    // VA_SENSE is a normalised 0..1 reading, so pretend 9 V through the divider
    // rather than storing volts here -- otherwise `status` prints nonsense.
    gws.va_sense   = gw_clampf (9.0f / (gwt.cal_va_scale > 0.1f ? gwt.cal_va_scale : 9.9f),
                                0.0f, 1.0f);
    gws.va_volts   = 9.0f;
}

// ---------------------------------------------------------------------------
//  One tick
// ---------------------------------------------------------------------------
void gw_control_tick (float dt)
{
    // Swap in a whole new settings struct between ticks, never mid-tick.
    if (s_settings_req)
    {
        gwt = s_settings_stage;
        s_settings_req = false;
    }

    // Hands off the ADC, the mux, the PWMs and the select bits while a bring-up
    // tool on core 1 is using them. CVs hold their last value.
    if (s_standdown_req)
    {
        s_standdown_ack = true;
        return;
    }
    s_standdown_ack = false;

    const uint32_t t0 = time_us_32();
    const float rate_hz = dt > 1e-9f ? 1.0f / dt : 1000.0f;

    // ---- 1. read everything ------------------------------------------------
    if (gwt.bench_mode)
    {
        bench_fake (dt);
    }
    else
    {
        gw_adc_scan (gws.raw, &gws.raw_cv1, &gws.raw_cv2);

        for (int i = 0; i < GW_NUM_POTS; ++i)
            gws.pot[i] = pot_norm (pot_filter_push (&s_pf[i], gws.raw[i]));

        gws.env_analog = sense_norm (pot_filter_push (&s_pf[MUX_ENV],        gws.raw[MUX_ENV]));
        gws.va_sense   = sense_norm (pot_filter_push (&s_pf[MUX_VA_SENSE],   gws.raw[MUX_VA_SENSE]));
        gws.lock_sense = sense_norm (pot_filter_push (&s_pf[MUX_LOCK_SENSE], gws.raw[MUX_LOCK_SENSE]));
        gws.trim_th    = pot_norm   (pot_filter_push (&s_pf[MUX_TRIM_TH],    gws.raw[MUX_TRIM_TH]));
        gws.trim_ho    = pot_norm   (pot_filter_push (&s_pf[MUX_TRIM_HO],    gws.raw[MUX_TRIM_HO]));
        gws.trim_fa    = pot_norm   (pot_filter_push (&s_pf[MUX_TRIM_FA],    gws.raw[MUX_TRIM_FA]));
        gws.va_volts   = gw_rail_volts (gws.va_sense);
    }

    const float cv1 = gwt.bench_mode ? 0.0f : sense_norm (gws.raw_cv1);
    const float cv2 = gwt.bench_mode ? 0.0f : sense_norm (gws.raw_cv2);

    // ---- 2. footswitches ---------------------------------------------------
    const GwStompEvents ev = gw_stomps_tick (dt);

    if (ev.s1_tap) gws.engaged = ! gws.engaged;          // bypass

    if (ev.s2_hold)                                       // cycle SVF mode
        gw_control_set_svf_mode ((gws.svf_mode + 1) % GW_NUM_SVF_MODE);

    // tap tempo -> whichever LFO tap_target_lfo points at
    if (gw_tap_tick (ev.s2_tap, dt))
    {
        if (gwt.tap_target_lfo == 2) { gwt.lfo2_rate_hz = gw_tap_rate_hz(); gw_mod_retrigger_lfo2(); }
        else                          { gwt.lfo1_rate_hz = gw_tap_rate_hz(); gw_mod_retrigger_lfo1(); }
    }

    // STARVE while both are held
    gws.starving = ev.both_active;
    {
        const float target = ev.both_active ? gwt.starve_depth : 0.0f;
        const float ms = ev.both_active ? gwt.starve_attack_ms : gwt.starve_release_ms;
        const float c  = gw_onepole_coeff (ms, rate_hz);
        gws.starve += c * (target - gws.starve);
        gws.starve = gw_clampf (gws.starve, 0.0f, 1.0f);
    }

    // bypass crossfade
    {
        const float c = gw_onepole_coeff (gwt.bypass_fade_ms, rate_hz);
        gws.bypass += c * ((gws.engaged ? 1.0f : 0.0f) - gws.bypass);
        gws.bypass = gw_clampf (gws.bypass, 0.0f, 1.0f);
    }

    // ---- 3. gate -----------------------------------------------------------
    gate_tick (gws.env_analog, dt, rate_hz);

    // ---- 4. modulation over the raw knob positions -------------------------
    gw_mod_tick (gws.env_analog, cv1, cv2, rate_hz);

    GwKnobs base;
    base.freq = gws.pot[POT_FREQ];
    base.gain = gws.pot[POT_GAIN];
    base.mix  = gws.pot[POT_MIX];
    base.fizz = gws.pot[POT_FIZZ];
    base.q    = gws.pot[POT_Q];
    base.vol  = gws.pot[POT_VOL];

    gws.knobs = gw_mod_compute (&base, dt);

    // ---- 5. knob positions -> the ten CVs ---------------------------------
    const float rail = gw_rail_scale (gws.va_volts);

    const GwFreqResult fr = gw_law_freq (gws.knobs.freq, (GW_NUM_FRANGE - 1) - gws.freq_range);
    gws.freq_hz    = fr.f_hz;
    if (fr.range != gws.freq_range)
    {
        gws.freq_range = fr.range;
        gw_set_freq_range (fr.range);        // gw_write_shifted handles the inversion
    }
    gws.cv[CV_FREQ] = fr.cv;

    gws.gain_x       = gw_law_gain_x (gws.knobs.gain);
    gws.cv[CV_DIRT]  = gw_law_gain_cv (gws.knobs.gain) * rail;

    gw_law_mix (gws.knobs.mix, &gws.cv[CV_MIXW], &gws.cv[CV_MIXD]);

    gws.svf_fc_hz    = gw_law_svf_fc_hz (gws.knobs.fizz);
    gws.svf_q        = gw_law_svf_q (gws.knobs.q);
    gws.cv[CV_SVFF]  = gw_law_svf_fc_cv (gws.knobs.fizz);
    gws.cv[CV_SVFQ]  = gw_law_svf_q_cv (gws.knobs.q);

    // ONE VCA carries the gate and the volume, so we send the product.
    gws.cv[CV_GATE]  = gw_law_gate_vol_cv (gws.knobs.vol, gws.gate_env);

    gws.cv[CV_BYP]    = gws.bypass;
    gws.cv[CV_STARVE] = gws.starve;

    // ---- CV OUT jack -------------------------------------------------------
    {
        float src = 0.0f;
        switch ((int) gwt.cvout_source)
        {
            case 1: src = gw_mod_lfo1(); break;
            case 2: src = 0.5f * (gw_mod_lfo2() + 1.0f); break;   // bipolar -> 0..1
            case 3: src = gw_mod_env();  break;
            case 4: src = gws.gate_env;  break;
            case 5: src = gw_mod_cv1();  break;
            case 6: src = gw_mod_cv2();  break;
            default: src = 0.0f;         break;
        }
        gws.cv[CV_OUT] = gw_clampf (src * gwt.cvout_scale + gwt.cvout_offset, 0.0f, 1.0f);
    }

    for (int i = 0; i < GW_NUM_CV; ++i)
        gw_pwm_write (i, gws.cv[i]);

    // ---- stats -------------------------------------------------------------
    ++gws.tick;
    gws.loop_us_last = time_us_32() - t0;
    if (gws.loop_us_last > gws.loop_us_max) gws.loop_us_max = gws.loop_us_last;
    {
        const uint32_t period_us = (uint32_t) (1000000.0f / (float) gwt.ctrl_rate_hz);
        if (gws.loop_us_last > period_us) ++gws.overruns;
    }
}

// ===========================================================================
//  LEDs — boot sweep, then the running state model.
//    A / B / C : the three circuit sections, env-reactive
//    TEMPO     : blinks at the tap rate
//    BYPASS    : solid when the effect is in
//    GATE      : brightness follows the gate VCA
// ===========================================================================
static float s_react_a, s_react_b, s_react_c;
static float s_boot_t     = 0.0f;
static int   s_mode_flash = 0;          // frames left of the SVF-mode flash
static int   s_last_mode  = -1;         // seeded on the first frame, see below

static GwRgb rgb (float r, float g, float b)
{
    GwRgb c;
    c.r = (uint8_t) (gw_clampf (r, 0.0f, 1.0f) * 255.0f);
    c.g = (uint8_t) (gw_clampf (g, 0.0f, 1.0f) * 255.0f);
    c.b = (uint8_t) (gw_clampf (b, 0.0f, 1.0f) * 255.0f);
    return c;
}

// hue 0..1 -> rgb, full saturation and value
static GwRgb hue (float h, float v)
{
    h = h - floorf (h);
    const float x = h * 6.0f;
    const int   i = (int) x;
    const float f = x - (float) i;
    switch (i % 6)
    {
        case 0: return rgb (v, v * f, 0);
        case 1: return rgb (v * (1 - f), v, 0);
        case 2: return rgb (0, v, v * f);
        case 3: return rgb (0, v * (1 - f), v);
        case 4: return rgb (v * f, 0, v);
        default:return rgb (v, 0, v * (1 - f));
    }
}

// One colour per SVF mode, so a glance tells you where you are.
static GwRgb mode_colour (int mode, float v)
{
    switch (mode)
    {
        case SVF_LP:    return rgb (0.15f * v, 0.55f * v, v);        // cool blue
        case SVF_BP:    return rgb (0.20f * v, v,        0.35f * v); // green
        case SVF_HP:    return rgb (v,        0.65f * v, 0.10f * v); // amber
        default:        return rgb (v,        0.15f * v, 0.85f * v); // magenta = notch
    }
}

void gw_control_leds (float dt)
{
    GwRgb px[GW_NUM_LEDS];

    // ---- boot rainbow sweep ------------------------------------------------
    if (gwt.led_boot_sweep && s_boot_t < 1.6f)
    {
        s_boot_t += dt;
        const float p = s_boot_t / 1.6f;
        for (int i = 0; i < GW_NUM_LEDS; ++i)
        {
            const float pos = (float) i / (float) (GW_NUM_LEDS - 1);
            float v = 1.0f - fabsf (p * 1.25f - pos) * 3.0f;   // a travelling bump
            if (v < 0.0f) v = 0.0f;
            px[i] = hue (pos * 0.8f + p * 0.4f, v);
        }
        gw_leds_set (px);
        return;
    }

    // ---- section reactivity ------------------------------------------------
    const float c = gw_onepole_coeff (gwt.led_react_decay_ms,
                                      dt > 1e-9f ? 1.0f / dt : 60.0f);
    const float a_src = gws.lock_sense;                              // the 567 core
    const float b_src = gw_clampf (gws.env_analog * gws.knobs.gain * 2.0f, 0, 1); // dirt
    const float c_src = gw_clampf (gws.env_analog * 1.4f, 0, 1);     // filter section

    s_react_a += (a_src > s_react_a ? 0.5f : c) * (a_src - s_react_a);
    s_react_b += (b_src > s_react_b ? 0.5f : c) * (b_src - s_react_b);
    s_react_c += (c_src > s_react_c ? 0.5f : c) * (c_src - s_react_c);

    // Changing the SVF mode flashes all three sections in that mode's colour.
    // The first frame after the boot sweep only seeds the comparison -- without
    // this every power-up ends with a spurious "mode changed" flash.
    if (s_last_mode < 0) s_last_mode = gws.svf_mode;
    else if (gws.svf_mode != s_last_mode) { s_last_mode = gws.svf_mode; s_mode_flash = 40; }

    if (s_mode_flash > 0)
    {
        --s_mode_flash;
        const float v = (s_mode_flash % 12) < 6 ? 1.0f : 0.15f;
        px[LED_SECT_A] = px[LED_SECT_B] = px[LED_SECT_C] = mode_colour (gws.svf_mode, v);
    }
    else
    {
        px[LED_SECT_A] = rgb (0.10f + 0.90f * s_react_a, 0.05f, 0.35f * s_react_a);  // 567: red/violet
        px[LED_SECT_B] = rgb (0.85f * s_react_b, 0.30f * s_react_b, 0.02f);          // dirt: amber
        px[LED_SECT_C] = mode_colour (gws.svf_mode, 0.12f + 0.88f * s_react_c);      // filter
    }

    // While starving, everything sags too — you can see the rail dropping.
    const float sag = 1.0f - 0.75f * gws.starve;

    px[LED_TEMPO]  = gw_tap_blink() ? rgb (0.0f, 0.85f, 0.95f) : rgb (0.0f, 0.05f, 0.08f);
    px[LED_BYPASS] = gws.engaged ? rgb (0.95f * gws.bypass, 0.10f, 0.75f * gws.bypass)
                                 : rgb (0.03f, 0.03f, 0.03f);
    px[LED_GATE]   = gws.gate_env > 0.002f
                   ? rgb (0.20f * gws.gate_env, 0.95f * gws.gate_env, 0.25f * gws.gate_env)
                   : rgb (0.04f, 0.0f, 0.0f);

    if (gws.starve > 0.001f)
        for (int i = 0; i < GW_NUM_LEDS; ++i)
        {
            px[i].r = (uint8_t) ((float) px[i].r * sag);
            px[i].g = (uint8_t) ((float) px[i].g * sag);
            px[i].b = (uint8_t) ((float) px[i].b * sag);
        }

    gw_leds_set (px);
}
