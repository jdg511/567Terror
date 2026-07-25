// Glitchwave 567 — pin map (fw-0.1)
// AUTHORITATIVE SOURCE: hardware/FIRMWARE_PLAN.md §1 (from gen_mcu.py netlist)
// Vendor: Illicit Apothecary
#pragma once

// ---- PWM control voltages (RC-filtered, 125 kHz / 10-bit) ----
#define PIN_PWM_FREQ    0   // 567 pitch CV (double-pole RC)
#define PIN_PWM_DIRT    1   // dirt gain VCA
#define PIN_PWM_MIXW    2   // mix wet VCA
#define PIN_PWM_MIXD    3   // mix dry VCA
#define PIN_PWM_SVFF    4   // SVF cutoff
#define PIN_PWM_SVFQ    5   // SVF resonance
#define PIN_PWM_GATE    6   // gate x VOL VCA (one VCA, product computed here)
#define PIN_PWM_BYP     7   // bypass crossfade VCA
#define PIN_PWM_STARVE  8   // Bazz Fuss rail servo (5 V hardware floor)
#define PIN_PWM_CVOUT   9   // CV OUT jack

// ---- discrete selects — INVERTED by MMBT3904 level shifters! ----
// Writing 1 to the GPIO pulls the shifted line LOW. Use the *_set() helpers.
#define PIN_FREQ_A     10   // CD4052 timing-cap select A
#define PIN_FREQ_B     11   // CD4052 timing-cap select B
#define PIN_FMODE_A    12   // SVF mode select A
#define PIN_FMODE_B    13   // SVF mode select B
#define PIN_FMODE_C    14   // SVF mode select C

// ---- LEDs / stomps ----
#define PIN_WS2812     15   // -> 74AHCT1G125 -> 100R -> control board chain
#define PIN_STOMP1     16   // active LOW (10k pullup + 100n on board)
#define PIN_STOMP2     17   // active LOW

// ---- ADC mux (74HC4067) ----
#define PIN_MUX_S0     18
#define PIN_MUX_S1     19
#define PIN_MUX_S2     20
#define PIN_MUX_S3     21

#define PIN_SPARE      22

// ---- ADC inputs ----
#define PIN_ADC_MUXED  26   // ADC0: 4067 common (1k + 1n)
#define PIN_ADC_CV1    27   // ADC1: CV1 jack front end
#define PIN_ADC_CV2    28   // ADC2: CV2 jack front end

// ---- 74HC4067 channel map (FIRMWARE_PLAN.md §2) ----
enum mux_channel {
    MUX_POT_FREQ = 0,   // POT1_W
    MUX_POT_GAIN = 1,   // POT2_W
    MUX_POT_MIX  = 2,   // POT3_W
    MUX_POT_FIZZ = 3,   // POT4_W
    MUX_POT_Q    = 4,   // POT5_W
    MUX_POT_VOL  = 5,   // POT6_W
    MUX_ENV      = 6,   // analog env follower
    MUX_VA_SENSE = 7,   // supply rail divider
    MUX_LOCK     = 8,   // LM567 lock/activity sense
    MUX_TRIM_TH  = 9,   // gate threshold trim RV1
    MUX_TRIM_HO  = 10,  // gate hold trim RV2
    MUX_TRIM_FA  = 11,  // gate fade trim RV3
    MUX_GND_C12  = 12,  // grounded spares: ADC zero self-test
    MUX_GND_C13  = 13,
    MUX_GND_C14  = 14,
    MUX_GND_C15  = 15,
    MUX_NCHAN    = 16,
};

// ---- WS2812 chain order (gen_ctrl.py) ----
enum led_index {
    LED_SECT_A = 0,
    LED_SECT_B = 1,
    LED_SECT_C = 2,
    LED_TEMPO  = 3,
    LED_BYPASS = 4,
    LED_GATE   = 5,
    LED_COUNT  = 6,
};
