// Glitchwave 567 — tunables (fw-0.1)
// The single place Jason (or Claude) tweaks feel. Mirrors the plugin's
// Tunables struct philosophy: change numbers here, rebuild, drag UF2.
#pragma once

// Control loop
#define CTRL_RATE_HZ        1000
#define MUX_SETTLE_US       20      // 1k + 1n + 4067 Ron settle
#define ADC_OVERSAMPLE      4       // median-of-4 per channel
#define POT_AVG_LEN         16      // moving average
#define POT_HYST_LSB        8       // anti-zipper hysteresis (12-bit)

// PWM CVs
#define PWM_WRAP            1023    // 10-bit @ ~122 kHz (125 MHz / 1024)

// VOL audio taper: a = (81^x - 1) / 80  (plugin-exact; 10% at half turn)
#define VOL_TAPER_BASE      81.0f

// Gate (trim pots scale within these ranges)
#define GATE_TH_MIN         0.002f  // env fraction of full scale
#define GATE_TH_MAX         0.20f
#define GATE_HOLD_MIN_MS    0
#define GATE_HOLD_MAX_MS    500
#define GATE_FADE_MIN_MS    1
#define GATE_FADE_MAX_MS    300

// Bypass crossfade
#define BYPASS_FADE_MS      10      // the sim's silent switch

// Stomp gestures
#define STOMP_DEBOUNCE_MS   5
#define STOMP_HOLD_MS       400     // < = tap, >= = hold
#define STARVE_ATTACK_MS    120     // both-held rail sag
#define STARVE_RELEASE_MS   250

// FREQ range switching (CD4052 caps 47n / 1u / 22u / 470u)
#define FREQ_RANGE_HYST     0.10f   // 10% past boundary before swapping caps

// WS2812
#define LED_BRIGHTNESS      40      // 0-255 default (pedalboard-friendly)
