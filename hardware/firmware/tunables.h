// ============================================================================
//  tunables.h  —  THE ONLY FILE YOU EVER NEED TO OPEN.
//
//  Glitchwave 567 pedal firmware, Illicit Apothecary.
//
//  Every number that decides how the pedal FEELS lives here. Nothing else in
//  this project needs editing to change the voice, the knob laws, the gate
//  ballistics, the LED brightness, or the stomp gesture timing.
//
//  ---------------------------------------------------------------------------
//  THREE WAYS TO CHANGE A VALUE
//  ---------------------------------------------------------------------------
//  1. LIVE, no rebuild, no computer skills needed:
//        plug the pedal into USB, open a serial terminal, type
//            set vol_taper_base 61
//            save
//        The pedal changes as you type. `save` makes it survive a power cycle.
//        Type `help` for the whole command list, `dump` to see everything.
//
//  2. Change the shipping default: edit the number in this file, rebuild,
//        drag the new .uf2 onto the pedal through the back plate.
//
//  3. `defaults` on the console throws away every saved change and goes back
//        to exactly what is written below.
//
//  ---------------------------------------------------------------------------
//  HOW TO READ A LINE
//  ---------------------------------------------------------------------------
//      FLT( name ,  default , min , max , "what it does" )
//       ^     ^        ^        ^    ^
//       |     |        |        |    +-- console refuses anything outside
//       |     |        |        +------- min/max guard rails
//       |     |        +---------------- the shipping value
//       |     +------------------------- the name you type on the console
//       +------------------------------- FLT = decimal, INT = whole number,
//                                        BOL = 0 or 1 (off/on)
//
//  Anything marked (CAL) is written by the calibration routine and is specific
//  to YOUR board. Don't hand-edit those unless you're re-zeroing on purpose.
// ============================================================================
#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// The list. Add a line here and it automatically appears in the struct, in the
// console, in `dump`, and in the flash-saved settings. Nothing else to touch.
// ---------------------------------------------------------------------------
#define GW_TUNABLE_LIST(FLT, INT, BOL)                                                                 \
                                                                                                       \
/* ==== CONTROL LOOP ====================================================== */                         \
INT(ctrl_rate_hz,        1000,     200,   4000, "control loop ticks per second")                        \
INT(pot_avg_len,           16,       1,     64, "pot moving-average window (samples)")                  \
INT(pot_hyst_lsb,           8,       0,    128, "pot deadband in ADC counts - kills zipper noise")      \
INT(adc_oversample,         4,       1,     16, "ADC reads per mux channel, median taken")              \
INT(mux_settle_us,         20,       2,    500, "wait after changing mux address (us)")                 \
                                                                                                       \
/* ==== FREQ  (POT1) — the 567 pitch ====================================== */                          \
/* Plugin v0.32 spans 0.2 Hz .. 6 kHz. The pedal gets there by picking one of  */                       \
/* four CD4052 timing caps and then sweeping the OTA inside that cap's range.  */                       \
FLT(freq_lo_hz,          0.2f,   0.02f,  50.0f, "FREQ fully CCW (Hz) - matches the plugin")             \
FLT(freq_hi_hz,        6000.0f,  50.0f, 20000.f, "FREQ fully CW (Hz) - matches the plugin")             \
FLT(freq_range_overlap,  0.18f,   0.0f,   0.5f, "how much neighbouring cap ranges overlap - THIS is the")\
/*                                                 hysteresis that stops the caps chattering back and forth */ \
FLT(freq_range_hyst,     0.10f,   0.0f,   0.4f, "extra overlap on top, if the caps still chatter")       \
FLT(freq_cv_curve,       1.00f,   0.25f,  4.0f, "bends pitch CV vs pot. 1 = pure exponential")          \
                                                                                                       \
/* ---- BRING-UP CURVES ---------------------------------------------------- */                        \
/* The exact CV-voltage -> parameter transfer of each OTA/VCA is not known    */                       \
/* until the real board is on the bench (milestone fw-0.2, scope on the RC'd  */                       \
/* CV lines). These four bend each CV until the knob FEELS like the plugin,   */                       \
/* live from the console, no rebuild. 1.0 = straight through.                 */                       \
FLT(dirt_cv_curve,       1.00f,   0.25f,  4.0f, "bends the GAIN VCA CV")                                \
FLT(svff_cv_curve,       1.00f,   0.25f,  4.0f, "bends the SVF cutoff CV")                              \
FLT(svfq_cv_curve,       1.00f,   0.25f,  4.0f, "bends the SVF resonance CV")                           \
FLT(gate_cv_curve,       1.00f,   0.25f,  4.0f, "bends the GATExVOL VCA CV")                            \
                                                                                                       \
/* ==== GAIN  (POT2) — the dirt ============================================ */                         \
FLT(gain_min,            1.10f,   1.0f,   50.0f, "gain fully CCW (x) - plugin uses x1.1")               \
FLT(gain_max,          300.0f,   10.0f, 2000.0f, "gain fully CW (x) - plugin uses x300")                \
                                                                                                       \
/* ==== MIX   (POT3) ====================================================== */                          \
BOL(mix_const_power,        1,                   "1 = constant-power crossfade, 0 = straight linear")   \
FLT(mix_dry_trim,        1.00f,   0.0f,   2.0f, "extra trim on the DRY VCA only")                       \
FLT(mix_wet_trim,        1.00f,   0.0f,   2.0f, "extra trim on the WET VCA only")                       \
                                                                                                       \
/* ==== FIZZ  (POT4) + Q (POT5) — the SVF ================================== */                          \
/* On the PCB a CV-controlled SVF replaces the plugin's dual-gang Sallen-Key,  */                       \
/* so FIZZ has a wider range than the sim did. Defaults map the plugin's       */                       \
/* 8.1 kHz -> 1.34 kHz feel onto it.                                          */                        \
FLT(svf_lo_hz,          20.0f,    5.0f,  500.0f, "SVF cutoff fully CCW (Hz)")                           \
FLT(svf_hi_hz,        8800.0f,  500.0f, 20000.f, "SVF cutoff fully CW (Hz)")                            \
FLT(svf_q_lo,            0.25f,  0.05f,   2.0f, "Q fully CCW - plugin uses 0.25")                       \
FLT(svf_q_hi,            8.00f,   1.0f,  30.0f, "Q fully CW - plugin uses 8")                           \
INT(svf_mode_boot,          0,       0,      3, "mode at power-up: 0 LP, 1 BP, 2 HP, 3 Notch")          \
                                                                                                       \
/* ==== VOL   (POT6) ====================================================== */                          \
/* Linear B10k pot given an audio taper in software: a = (base^x - 1)/(base-1) */                       \
/* base 81 == the plugin's exact law, ~10% at half rotation.                   */                       \
FLT(vol_taper_base,     81.0f,    2.0f,  400.0f, "audio-taper steepness. 81 = plugin exact")           \
FLT(vol_max,             1.00f,   0.1f,   1.0f, "ceiling on the VOL/GATE VCA CV")                       \
                                                                                                       \
/* ==== GATE — trims RV1/RV2/RV3 live inside the pedal ==================== */                          \
/* The trimpots set these live; the numbers below are the range each trim     */                        \
/* sweeps, plus behaviour the trims don't cover.                              */                        \
FLT(gate_thresh_lo,      0.01f,   0.0f,   1.0f, "RV1 fully CCW threshold")                              \
FLT(gate_thresh_hi,      0.60f,   0.0f,   1.0f, "RV1 fully CW threshold")                               \
FLT(gate_hold_lo_ms,     0.0f,    0.0f, 2000.0f, "RV2 fully CCW hold (ms)")                             \
FLT(gate_hold_hi_ms,   500.0f,    1.0f, 5000.0f, "RV2 fully CW hold (ms)")                              \
FLT(gate_fade_lo_ms,     1.0f,    0.1f,  500.0f, "RV3 fully CCW fade (ms)")                             \
FLT(gate_fade_hi_ms,   300.0f,    1.0f, 5000.0f, "RV3 fully CW fade (ms)")                              \
FLT(gate_attack_ms,      3.0f,    0.1f,  200.0f, "how fast the gate OPENS")                             \
FLT(gate_hyst,           0.15f,   0.0f,   0.9f, "re-close threshold as a fraction of open (chatter)")   \
BOL(gate_enabled,           1,                   "0 = gate always open (VOL still works)")              \
                                                                                                       \
/* ==== BYPASS ============================================================ */                          \
FLT(bypass_fade_ms,     10.0f,    0.5f,  500.0f, "buffered-bypass crossfade time")                      \
BOL(bypass_boot_engaged,    1,                   "1 = effect ON at power-up, 0 = bypassed")             \
                                                                                                       \
/* ==== STOMP FEEL ======================================================== */                          \
FLT(stomp_debounce_ms,   5.0f,    0.5f,   50.0f, "contact-bounce integrator per switch")                \
INT(stomp_hold_ms,        400,      80,   2000, "press shorter than this = tap, longer = hold")         \
INT(stomp_both_ms,         60,      10,    500, "window for 'both pressed together'")                   \
INT(tap_timeout_ms,      3000,     500,  10000, "forget the tap sequence after this long")              \
INT(tap_min_ms,            80,      20,   1000, "fastest accepted tap interval")                        \
INT(tap_max_ms,          2500,    200,  10000, "slowest accepted tap interval")                         \
INT(tap_avg_taps,           4,       2,      8, "average this many intervals for the tempo")            \
INT(tap_target_lfo,          1,       1,      2, "which LFO tap tempo sets (1 or 2)")                    \
                                                                                                       \
/* ==== STARVE  (both stomps held) ======================================== */                          \
/* Hardware floor is 5 V; the servo cannot sag below it no matter what.       */                        \
FLT(starve_attack_ms,   900.0f,   10.0f, 8000.0f, "how long the sag takes while held")                  \
FLT(starve_release_ms , 600.0f,   10.0f, 8000.0f, "recovery after you let go")                          \
FLT(starve_depth,        1.00f,   0.0f,   1.0f, "1 = all the way to the 5 V floor")                     \
                                                                                                       \
/* ==== LFO 1 — always unipolar-up 0..1 (mirrors the plugin) ============== */                           \
FLT(lfo1_rate_hz,        2.00f,  0.01f, 200.0f, "LFO1 rate")                                            \
FLT(lfo1_depth,          0.00f,   0.0f,   1.0f, "LFO1 depth (0 = off)")                                 \
INT(lfo1_shape,             1,       0,     23, "see SHAPES table below")                               \
INT(lfo1_target,            1,       0,     16, "see TARGETS table below")                               \
                                                                                                       \
/* ==== LFO 2 — always bipolar +-1 ======================================== */                           \
FLT(lfo2_rate_hz,        0.25f,  0.01f, 200.0f, "LFO2 rate")                                            \
FLT(lfo2_depth,          0.00f,   0.0f,   1.0f, "LFO2 depth (0 = off)")                                 \
INT(lfo2_shape,             0,       0,     23, "see SHAPES table below")                               \
INT(lfo2_target,            6,       0,     16, "default 6 = LFO1 rate")                                \
                                                                                                       \
/* ==== ENVELOPE FOLLOWER ================================================= */                          \
/* The analog follower on the board already has Mu-Tron ballistics (4/150 ms) */                        \
/* and arrives on mux channel 6. These shape what firmware does with it.      */                        \
FLT(env_gain,            4.00f,  0.125f,  40.0f, "env sensitivity (x)")                                  \
BOL(env_drive_up,           1,                   "1 = louder opens up, 0 = louder closes down")         \
INT(env_target,             2,       0,     16, "default 2 = FIZZ / SVF cutoff")                        \
FLT(env_extra_smooth_ms , 0.0f,    0.0f,  200.0f, "extra firmware smoothing on top of the analog")      \
                                                                                                       \
/* ==== CV IN 1 & 2 ======================================================= */                          \
/* Normalled-jack behaviour: no signal for a few seconds and the depth VCA    */                        \
/* opens all the way, so the LFO just runs at its knob. Same as the plugin.   */                        \
FLT(cv_smooth_ms,       10.6f,    0.5f,  500.0f, "CV input slew (10.6 ms == plugin's 15 Hz)")           \
FLT(cv_gain,             2.00f,   0.1f,  20.0f, "CV input scaling into the depth VCA")                  \
FLT(cv_unplug_sec,       3.00f,   0.2f,   30.0f, "silence this long = treat the jack as empty")          \
FLT(cv_detect_level,    0.001f, 0.0001f,  0.5f, "above this counts as 'a cable is plugged in'")         \
                                                                                                       \
/* ==== CV OUT jack ======================================================= */                          \
INT(cvout_source,           1,       0,      6, "0 off, 1 LFO1, 2 LFO2, 3 ENV, 4 GATE, 5 CV1, 6 CV2")   \
FLT(cvout_scale,         1.00f,   0.0f,   1.0f, "output level into the 0..5 V jack")                     \
FLT(cvout_offset,        0.00f,  -1.0f,   1.0f, "shifts the CV OUT up or down")                          \
                                                                                                       \
/* ==== LEDs ============================================================== */                          \
FLT(led_brightness,      0.28f,  0.01f,   1.0f, "master brightness - low is pedalboard-friendly")       \
FLT(led_gamma,           2.20f,   1.0f,   3.5f, "perceptual curve. 2.2 looks linear to the eye")        \
INT(led_fps,               60,      10,    200, "WS2812 refresh rate")                                  \
BOL(led_boot_sweep,         1,                   "1 = rainbow sweep at power-up")                       \
FLT(led_react_decay_ms ,120.0f,   10.0f, 2000.0f, "how fast the section LEDs fall back")                \
                                                                                                       \
/* ==== CALIBRATION (CAL) — your board's own numbers ====================== */                          \
INT(cal_adc_offset,         0,   -200,    200, "(CAL) ADC zero from grounded mux channels")             \
FLT(cal_va_scale,       9.90f,    1.0f,  60.0f, "(CAL) volts at VA per 1.0 of VA_SENSE")                \
FLT(cal_va_nominal,     9.00f,    5.0f,  24.0f, "(CAL) rail the laws were tuned at")                    \
BOL(cal_va_compensate,      1,                   "1 = rescale laws when you run 12/15/18 V")            \
INT(cal_pot_lo,            12,       0,   1000, "(CAL) ADC count at a pot's full CCW")                  \
INT(cal_pot_hi,          4080,    3000,   4095, "(CAL) ADC count at a pot's full CW")                   \
BOL(cal_done,               0,                   "(CAL) 1 once calibration has been run and saved")     \
                                                                                                       \
/* ==== BENCH / DEBUG ===================================================== */                          \
BOL(bench_mode,             0,                   "1 = bare Pico on the desk, no PCB attached")          \
BOL(verbose_boot,           1,                   "print the banner and pin map over USB at power-up")   \
INT(stats_period_ms,        0,       0,  10000, "0 = quiet. Otherwise auto-print loop stats this often") \
/* end of list */

// ---------------------------------------------------------------------------
//  SHAPES  (lfo1_shape / lfo2_shape)   — same numbering as the plugin, so a
//  setting that sounded right in the VST transfers straight across.
//
//     0 Sine          1 Triangle      2 Square/Pulse   3 Sample&Hold
//     4 WhiteNoise    5 PinkNoise     6 RampUp         7 RampDown
//     8 Sweep         9 Lumps        10 RampOct       11 QuadRamp
//    12 QuadPulse    13 TriStep      14 SineOct       15 Sine3rd
//    16 Sine4th      17 RandSlopes   18 Lorenz        19 Rossler
//    20 DrunkWalk    21 PerlinDrift  22 Wobble        23 Glitch
//
//  For the noise shapes, "rate" means the noise low-pass cutoff.
//  For 18..23, "rate" means how fast the generator wanders.
//
//  TARGETS (lfo1_target / lfo2_target / env_target)
//
//     0 Off           1 FREQ          2 FIZZ (SVF cutoff)  3 MIX
//     4 VOL           5 (reserved)    6 LFO1 rate          7 LFO1 depth
//     8 LFO2 rate     9 LFO2 depth   10 env amount        11 (reserved)
//    12 (reserved)   13 SVF Q        14 (reserved)        15 GAIN
//    16 env level
// ---------------------------------------------------------------------------

// ===========================================================================
//  Machinery below. You do not need to read any of it.
// ===========================================================================

#define GW_T_F(n, d, lo, hi, h)  float   n;
#define GW_T_I(n, d, lo, hi, h)  int32_t n;
#define GW_T_B(n, d, h)          uint8_t n;

typedef struct GwTunables {
    GW_TUNABLE_LIST (GW_T_F, GW_T_I, GW_T_B)
} GwTunables;

#undef GW_T_F
#undef GW_T_I
#undef GW_T_B

typedef enum { GW_KIND_FLOAT, GW_KIND_INT, GW_KIND_BOOL } GwKind;

typedef struct GwTunableInfo {
    const char* name;
    GwKind      kind;
    uint16_t    offset;
    float       defval;
    float       minv;
    float       maxv;
    const char* help;
} GwTunableInfo;

extern const GwTunableInfo  gw_tunable_info[];
extern const int            gw_tunable_count;
extern GwTunables           gwt;      // the live settings

void  gw_tunables_defaults (GwTunables* t);
int   gw_tunable_find      (const char* name);              // -1 if unknown
float gw_tunable_get       (const GwTunables* t, int idx);
int   gw_tunable_set       (GwTunables* t, int idx, float v); // 0 = out of range
