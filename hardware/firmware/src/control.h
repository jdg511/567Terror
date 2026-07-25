// ============================================================================
//  control.h — the 1 kHz control loop. This is the pedal's brain.
// ============================================================================
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "board.h"
#include "mod_system.h"
#include "tunables.h"

#ifdef __cplusplus
extern "C" {
#endif

// Everything the console and the LEDs want to look at, in one place.
typedef struct GwState {
    uint16_t raw[GW_NUM_MUX];       // last ADC scan, 0..4095
    uint16_t raw_cv1, raw_cv2;

    float    pot[GW_NUM_POTS];      // conditioned 0..1, in GwPot order
    float    trim_th, trim_ho, trim_fa;
    float    env_analog;            // mux ch 6, 0..1
    float    lock_sense;            // mux ch 8, 0..1
    float    va_sense;              // mux ch 7, 0..1
    float    va_volts;              // scaled through cal_va_scale

    GwKnobs  knobs;                 // post-modulation knob positions

    float    freq_hz;               // what FREQ is currently asking for
    int      freq_range;            // GwFreqRange on FREQ_A/B
    float    gain_x;                // dirt multiplier
    float    svf_fc_hz, svf_q;
    int      svf_mode;

    float    gate_env;              // 0..1, what the gate VCA is doing
    bool     gate_open;
    float    bypass;                // 0 = bypassed, 1 = effect in
    bool     engaged;               // the logical bypass state
    float    starve;                // 0 = full rail, 1 = sagged to the 5 V floor
    bool     starving;

    float    cv[GW_NUM_CV];         // what we actually wrote to the PWMs

    uint32_t tick;
    uint32_t loop_us_last;
    uint32_t loop_us_max;
    uint32_t overruns;              // ticks that took longer than the period
} GwState;

extern GwState gws;

void gw_control_init (void);
void gw_control_tick (float dt);        // one control tick
void gw_control_leds (float dt);        // build and publish the LED frame

// Console hooks.
void gw_control_set_engaged  (bool on);
void gw_control_set_svf_mode (int mode);
void gw_control_reset_stats  (void);

// ---- bring-up stand-down ---------------------------------------------------
// The console (core 1) and the control loop (core 0) both want the ADC, the
// 4067 address lines, the PWM slices and the select bits. There is exactly one
// of each, so before a bring-up tool touches them core 0 has to let go.
//
//   gw_control_standdown()  parks core 0 and waits for the in-flight tick to
//                           finish, so the caller then owns the hardware.
//   gw_control_resume()     hands it back.
//
// Without this, `selftest`, `sweep` and every `cal` command read a mux channel
// core 0 has already moved on from, and their PWM writes are overwritten within
// a millisecond -- the tools would report confident nonsense.
void gw_control_standdown (void);
void gw_control_resume    (void);
bool gw_control_is_stood_down (void);

// ---- settings handshake ----------------------------------------------------
// `defaults` and `load` rewrite the whole settings struct. Doing that from
// core 1 while core 0 is midway through reading it can hand the laws a garbage
// value for one tick. Core 1 stages the new struct and core 0 swaps it in
// between ticks instead.
void gw_control_request_settings (const GwTunables* incoming);

#ifdef __cplusplus
}
#endif
