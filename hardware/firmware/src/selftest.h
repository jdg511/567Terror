// ============================================================================
//  selftest.h — bring-up checks and per-unit calibration.
// ============================================================================
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Non-interactive: prints a pass/fail report over USB.
//   - ADC zero from the grounded mux channels C12..C15
//   - VA rail present and plausible
//   - every mux channel reads something (an open channel reads as a floating
//     rail, which is how a broken 4067 or a cold solder joint shows up)
//   - both footswitches read released
//   - each of the ten PWM CVs sweeps (scope-verifiable, milestone fw-0.2)
void gw_selftest_run (void);

// Sweeps one CV slowly so you can put a scope on its RC'd output.
void gw_selftest_sweep_cv (int cv, int seconds);

// Walks all six LEDs through red/green/blue, then white. Verifies the chain
// order matches gen_ctrl.py: section A, B, C, tempo, bypass, gate.
void gw_selftest_leds (void);

// ---- calibration -----------------------------------------------------------
// Measures the ADC zero from the grounded channels and writes cal_adc_offset.
void gw_cal_adc_zero (void);

// Given the real measured rail voltage, back-computes cal_va_scale.
void gw_cal_va (float measured_volts);

// Interactive pot-endpoint learn. Prompts, then samples; call twice.
void gw_cal_pot_endpoints (void);

#ifdef __cplusplus
}
#endif
