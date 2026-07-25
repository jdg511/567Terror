// ============================================================================
//  hw_io.h — the three hardware drivers: PWM CV out, muxed ADC in, WS2812.
// ============================================================================
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------------------------------------------------ PWM CV
// 10-bit resolution at 122.07 kHz (125 MHz / 1024) — three octaves above the
// slowest RC pole on the board (160 Hz), so nothing audible gets through.
#define GW_PWM_WRAP     1023u

void gw_pwm_init  (void);
void gw_pwm_write (int cv, float value01);   // clamps, applies nothing else
void gw_pwm_raw   (int cv, uint16_t level);  // 0..GW_PWM_WRAP, for self-test
float gw_pwm_last (int cv);

// ------------------------------------------------------------- select bits
void gw_select_init      (void);
void gw_set_freq_range   (int range);     // GwFreqRange, handles the inverted bits
void gw_set_svf_mode     (int mode);      // GwSvfMode,   handles the inverted bits
int  gw_get_freq_range   (void);
int  gw_get_svf_mode     (void);

// ------------------------------------------------------------------- ADC in
void gw_adc_init (void);

// Full blocking scan of the 12 live mux channels plus CV1/CV2 direct.
// Fills raw[] with 0..4095 counts. Costs roughly 350 us at default tunables.
void gw_adc_scan (uint16_t raw[GW_NUM_MUX], uint16_t* cv1, uint16_t* cv2);

// Read one mux channel on demand (used by selftest / calibration).
uint16_t gw_adc_read_mux (int channel);

// ------------------------------------------------------------------ WS2812
typedef struct { uint8_t r, g, b; } GwRgb;

void gw_leds_init  (void);
// Publish a frame. Safe to call from core 0; core 1 does the actual sending.
void gw_leds_set   (const GwRgb px[GW_NUM_LEDS]);
// Core 1 calls this in its loop.
void gw_leds_service (void);

#ifdef __cplusplus
}
#endif
