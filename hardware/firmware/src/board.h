// ============================================================================
//  board.h — Glitchwave 567 pin map. AUTHORITATIVE.
//
//  Extracted from gen_mcu.py / gen_core567.py / gen_ctrl.py at commit 1949fc3
//  and mirrored from hardware/FIRMWARE_PLAN.md section 1.
//
//  DO NOT EDIT to change pedal behaviour — this file describes the copper.
//  Behaviour lives in tunables.h.
//
//  *** THE INVERTED-BIT TRAP ***
//  FREQ_A/B and FMODE_A/B/C reach their CMOS chips through NPN level shifters
//  (Q2..Q6). Those stages INVERT. Every write to those pins must go through
//  gw_write_shifted() below, never gpio_put() directly.
// ============================================================================
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef PICO_ON_DEVICE
  #include "hardware/gpio.h"
#else
  // Lets the host-side law verifier (test/verify_laws.c) include this file
  // without dragging in the whole SDK.
  typedef unsigned int uint;
  static inline void gpio_put (uint pin, bool v) { (void) pin; (void) v; }
#endif

// ---------------------------------------------------------------- PWM CV out
// GP0..GP9. Note the RP2040 slice pairing: slice = gpio/2, chan = gpio&1.
// GP0..GP9 == slices 0..4, both channels of each. No slice collides with
// anything else on this board.
#define PIN_PWM_FREQ     0   // 567 pitch CV -> 2-pole RC (10k+100n x2) -> OTA Iabc
#define PIN_PWM_DIRT     1   // dirt gain VCA CV
#define PIN_PWM_MIXW     2   // MIX crossfade WET VCA CV
#define PIN_PWM_MIXD     3   // MIX crossfade DRY VCA CV
#define PIN_PWM_SVFF     4   // SVF cutoff CV
#define PIN_PWM_SVFQ     5   // SVF resonance CV
#define PIN_PWM_GATE     6   // Gate x VOL VCA CV (ONE VCA does both -- we
                             //   compute the product here in firmware)
#define PIN_PWM_BYP      7   // buffered-bypass crossfade VCA CV (10 ms fade)
#define PIN_PWM_STARVE   8   // Bazz Fuss rail-servo CV (5 V hardware floor)
#define PIN_CVOUT_PWM    9   // CV OUT jack (2-pole RC + buffer on env sheet)

// Indices into the CV array. Order matches the pins above.
enum GwCv {
    CV_FREQ = 0, CV_DIRT, CV_MIXW, CV_MIXD, CV_SVFF,
    CV_SVFQ, CV_GATE, CV_BYP, CV_STARVE, CV_OUT,
    GW_NUM_CV
};

static const uint8_t GW_CV_PIN[GW_NUM_CV] = {
    PIN_PWM_FREQ, PIN_PWM_DIRT, PIN_PWM_MIXW, PIN_PWM_MIXD, PIN_PWM_SVFF,
    PIN_PWM_SVFQ, PIN_PWM_GATE, PIN_PWM_BYP, PIN_PWM_STARVE, PIN_CVOUT_PWM
};

static const char* const GW_CV_NAME[GW_NUM_CV] = {
    "FREQ", "DIRT", "MIXW", "MIXD", "SVFF",
    "SVFQ", "GATE", "BYP", "STARVE", "CVOUT"
};

// ------------------------------------------------- level-shifted select bits
// ALL FIVE ARE INVERTED BY THEIR NPN SHIFTER.
#define PIN_FREQ_A      10   // CD4052 timing-cap select bit A  (Q2) INVERTED
#define PIN_FREQ_B      11   // CD4052 timing-cap select bit B  (Q3) INVERTED
#define PIN_FMODE_A     12   // SVF mode select A               (Q4) INVERTED
#define PIN_FMODE_B     13   // SVF mode select B               (Q5) INVERTED
#define PIN_FMODE_C     14   // SVF mode select C               (Q6) INVERTED

// The ONLY sanctioned way to drive those five pins. `logical` is the level you
// want to appear at the CMOS input; this flips it for the shifter.
static inline void gw_write_shifted (uint pin, bool logical)
{
    gpio_put (pin, ! logical);
}

// ------------------------------------------------------------------ WS2812
#define PIN_WS_DATA     15   // -> 74AHCT1G125 -> 100R -> header -> 6-LED chain
#define GW_NUM_LEDS      6
// Chain order from gen_ctrl.py. Index 0 is nearest WS_IN.
enum GwLed { LED_SECT_A = 0, LED_SECT_B, LED_SECT_C, LED_TEMPO, LED_BYPASS, LED_GATE };

// ------------------------------------------------------------------ stomps
#define PIN_STOMP1      16   // active-LOW, 10k pullup to 3V3 + 100n
#define PIN_STOMP2      17   // active-LOW

// ------------------------------------------------------------- 74HC4067 mux
#define PIN_MUX_S0      18
#define PIN_MUX_S1      19
#define PIN_MUX_S2      20
#define PIN_MUX_S3      21

#define PIN_SPARE_GP22  22   // expansion room (plan section 9)

// -------------------------------------------------------------------- ADCs
#define PIN_ADC_MUXED   26   // ADC0 - 4067 common, through 1k with 1n to GND
#define PIN_CV1_ADC     27   // ADC1 - CV1 jack (analog rectify + slew front end)
#define PIN_CV2_ADC     28   // ADC2 - CV2 jack (same)
#define ADC_CH_MUXED     0
#define ADC_CH_CV1       1
#define ADC_CH_CV2       2
// ADC_VREF (pin 35) is NC. The onboard 201R / 2.2u filter is authoritative.

// ------------------------------------------------- 4067 channel map (plan 2)
enum GwMuxCh {
    MUX_POT1_FREQ = 0,   // B10k linear
    MUX_POT2_GAIN,
    MUX_POT3_MIX,
    MUX_POT4_FIZZ,
    MUX_POT5_Q,
    MUX_POT6_VOL,
    MUX_ENV,             // analog env follower (Mu-Tron ballistics 4/150 ms)
    MUX_VA_SENSE,        // VA rail divider - measures the 9..18 V supply
    MUX_LOCK_SENSE,      // LM567 lock / output activity
    MUX_TRIM_TH,         // RV1 gate THRESHOLD (internal)
    MUX_TRIM_HO,         // RV2 gate HOLD
    MUX_TRIM_FA,         // RV3 gate FADE
    MUX_GND_12,          // grounded spares -> ADC offset self-test
    MUX_GND_13,
    MUX_GND_14,
    MUX_GND_15,
    GW_NUM_MUX           // 16
};

// We only scan the 12 live channels every tick; C12..C15 are read by selftest.
#define GW_MUX_LIVE     12

static const char* const GW_MUX_NAME[GW_NUM_MUX] = {
    "POT1_FREQ", "POT2_GAIN", "POT3_MIX", "POT4_FIZZ", "POT5_Q", "POT6_VOL",
    "ENV", "VA_SENSE", "LOCK_SENSE", "TRIM_TH", "TRIM_HO", "TRIM_FA",
    "GND12", "GND13", "GND14", "GND15"
};

// The six panel pots, in mux order, as knob indices.
enum GwPot { POT_FREQ = 0, POT_GAIN, POT_MIX, POT_FIZZ, POT_Q, POT_VOL, GW_NUM_POTS };

// ------------------------------------------------------- CD4052 timing caps
// FREQ_A/B pick the LM567 timing cap. Bits are INVERTED in hardware --
// gw_write_shifted() handles that, so these values are LOGICAL.
enum GwFreqRange { FRANGE_47N = 0, FRANGE_1U, FRANGE_22U, FRANGE_470U, GW_NUM_FRANGE };

// ---------------------------------------------------------------- SVF modes
enum GwSvfMode { SVF_LP = 0, SVF_BP, SVF_HP, SVF_NOTCH, GW_NUM_SVF_MODE };
static const char* const GW_SVF_NAME[GW_NUM_SVF_MODE] = { "LP", "BP", "HP", "NOTCH" };

// FMODE_A/B/C truth table, logical levels (the shifter inverts on the way out).
static const uint8_t GW_SVF_BITS[GW_NUM_SVF_MODE] = {
    0b000,  // LP
    0b001,  // BP
    0b010,  // HP
    0b011   // Notch
};
