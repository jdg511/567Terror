// ============================================================================
//  mod_system.h — the modulation layer, ported from the plugin's ModSystem.h
//  so a setting that sounded right in the VST transfers straight to the pedal.
//
//  Ported to plain C and single-precision (the RP2040 has hardware-assisted
//  float but soft doubles). Every law and coefficient is numerically the same;
//  only the chaos generators (18..23) diverge from the plugin bit-for-bit, and
//  they diverge from THEMSELVES on every run by design.
//
//  LFO 1 is always unipolar-up 0..1.  LFO 2 is always bipolar +-1.
//  CV 1 is hardwired to LFO 1's depth as a VCA, CV 2 to LFO 2's — normalled,
//  so an empty jack means the LFO just runs at its depth knob.
// ============================================================================
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Same numbering as the plugin (append-only, so saved settings stay valid).
typedef enum {
    GW_SHAPE_SINE = 0, GW_SHAPE_TRIANGLE, GW_SHAPE_SQUARE, GW_SHAPE_SAMPLEHOLD,
    GW_SHAPE_WHITENOISE, GW_SHAPE_PINKNOISE, GW_SHAPE_RAMPUP, GW_SHAPE_RAMPDOWN,
    GW_SHAPE_SWEEP, GW_SHAPE_LUMPS,
    GW_SHAPE_RAMPOCT, GW_SHAPE_QUADRAMP, GW_SHAPE_QUADPULSE, GW_SHAPE_TRISTEP,
    GW_SHAPE_SINEOCT, GW_SHAPE_SINE3RD, GW_SHAPE_SINE4TH, GW_SHAPE_RANDSLOPES,
    GW_SHAPE_LORENZ, GW_SHAPE_ROSSLER, GW_SHAPE_DRUNKWALK, GW_SHAPE_PERLINDRIFT,
    GW_SHAPE_WOBBLE, GW_SHAPE_GLITCH,
    GW_NUM_SHAPES
} GwLfoShape;

typedef enum {
    GW_TGT_OFF = 0,
    GW_TGT_FREQ, GW_TGT_FIZZ, GW_TGT_MIX, GW_TGT_VOL, GW_TGT_TRIM,
    GW_TGT_LFO1RATE, GW_TGT_LFO1DEPTH, GW_TGT_LFO2RATE, GW_TGT_LFO2DEPTH,
    GW_TGT_ENVAMOUNT,
    GW_TGT_RESV11, GW_TGT_RESV12,
    GW_TGT_SVFQ, GW_TGT_RESV14,
    GW_TGT_GAIN,
    GW_TGT_ENVLEVEL,
    GW_NUM_TARGETS
} GwModTarget;

extern const char* const gw_shape_names[GW_NUM_SHAPES];
extern const char* const gw_target_names[GW_NUM_TARGETS];

// False for the reserved slots that exist only to keep the plugin's numbering
// stable. The console refuses to select these rather than silently doing nothing.
bool gw_target_implemented (int target);

// The six panel-knob positions, 0..1, after modulation.
typedef struct GwKnobs {
    float freq, gain, mix, fizz, q, vol;
} GwKnobs;

void  gw_mod_init   (void);
void  gw_mod_reset  (void);

// Called once per control tick, BEFORE compute. Inputs are 0..1 levels taken
// straight off the analog env follower and the two CV front ends.
void  gw_mod_tick   (float env_analog, float cv1, float cv2, float rate_hz);

// Applies LFO1, LFO2 and the envelope follower to the raw knob positions.
GwKnobs gw_mod_compute (const GwKnobs* base, float dt);

// A tap on the tap-tempo switch re-seeds the non-periodic generators so they
// feel synced under your foot. Harmless for the periodic shapes.
void  gw_mod_retrigger_lfo1 (void);
void  gw_mod_retrigger_lfo2 (void);

// For the LEDs and the CV OUT jack.
float gw_mod_lfo1 (void);       // 0..1
float gw_mod_lfo2 (void);       // -1..1
float gw_mod_env  (void);       // 0..1, post gain and level
float gw_mod_cv1  (void);
float gw_mod_cv2  (void);
bool  gw_mod_cv1_plugged (void);
bool  gw_mod_cv2_plugged (void);

#ifdef __cplusplus
}
#endif
