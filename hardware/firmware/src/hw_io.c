// ============================================================================
//  hw_io.c — PWM CV out, muxed ADC in, WS2812 chain.
// ============================================================================
#include "hw_io.h"
#include "tunables.h"

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/sync.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

#include <math.h>
#include <string.h>

// A NaN/Inf test the optimiser cannot fold away, whatever the flags. Belt and
// braces alongside -fno-finite-math-only: if a CV ever goes non-finite we want
// silence, not full scale.
static inline bool gw_is_bad (float f)
{
    uint32_t u;
    memcpy (&u, &f, sizeof (u));
    return (u & 0x7F800000u) == 0x7F800000u;
}

// ===========================================================================
//  PWM CV
// ===========================================================================
static float s_cv_last[GW_NUM_CV];

void gw_pwm_init (void)
{
    for (int i = 0; i < GW_NUM_CV; ++i)
    {
        const uint pin = GW_CV_PIN[i];
        if (pin > 29u) continue;
        const uint slice = pwm_gpio_to_slice_num (pin);

        gpio_set_function (pin, GPIO_FUNC_PWM);
        pwm_set_wrap (slice, GW_PWM_WRAP);
        pwm_set_clkdiv (slice, 1.0f);            // 125 MHz / 1024 = 122.07 kHz
        pwm_set_gpio_level (pin, 0);
        pwm_set_enabled (slice, true);
        s_cv_last[i] = 0.0f;
    }
}

void gw_pwm_write (int cv, float v)
{
    if (cv < 0 || cv >= GW_NUM_CV) return;
    const uint pin = GW_CV_PIN[cv];
    if (pin > 29u) return;                       // lets the compiler bound the slice
    if (gw_is_bad (v)) v = 0.0f;                 // never latch a NaN or an Inf
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    s_cv_last[cv] = v;
    // Level GW_PWM_WRAP+1 is permanently high on the RP2040, which is how you
    // get a genuine 100 % duty out of a wrap-1023 slice. Stopping at WRAP would
    // cap every CV at 1023/1024.
    pwm_set_gpio_level (pin, (uint16_t) (v * (float) (GW_PWM_WRAP + 1u) + 0.5f));
}

void gw_pwm_raw (int cv, uint16_t level)
{
    if (cv < 0 || cv >= GW_NUM_CV) return;
    const uint pin = GW_CV_PIN[cv];
    if (pin > 29u) return;
    if (level > GW_PWM_WRAP) level = GW_PWM_WRAP;
    s_cv_last[cv] = (float) level / (float) GW_PWM_WRAP;
    pwm_set_gpio_level (pin, level);
}

float gw_pwm_last (int cv)
{
    return (cv >= 0 && cv < GW_NUM_CV) ? s_cv_last[cv] : 0.0f;
}

// ===========================================================================
//  Level-shifted select bits.  EVERY write goes through gw_write_shifted().
// ===========================================================================
static int s_freq_range = FRANGE_47N;
static int s_svf_mode   = SVF_LP;

void gw_select_init (void)
{
    const uint pins[5] = { PIN_FREQ_A, PIN_FREQ_B, PIN_FMODE_A, PIN_FMODE_B, PIN_FMODE_C };
    for (int i = 0; i < 5; ++i)
    {
        gpio_init (pins[i]);
        gpio_set_dir (pins[i], GPIO_OUT);
    }
    gw_set_freq_range (FRANGE_47N);
    gw_set_svf_mode ((int) gwt.svf_mode_boot);
}

void gw_set_freq_range (int range)
{
    if (range < 0 || range >= GW_NUM_FRANGE) return;
    s_freq_range = range;
    gw_write_shifted (PIN_FREQ_A, (range & 0x1) != 0);
    gw_write_shifted (PIN_FREQ_B, (range & 0x2) != 0);
}

void gw_set_svf_mode (int mode)
{
    if (mode < 0 || mode >= GW_NUM_SVF_MODE) return;
    s_svf_mode = mode;
    const uint8_t bits = GW_SVF_BITS[mode];
    gw_write_shifted (PIN_FMODE_A, (bits & 0x1) != 0);
    gw_write_shifted (PIN_FMODE_B, (bits & 0x2) != 0);
    gw_write_shifted (PIN_FMODE_C, (bits & 0x4) != 0);
}

int gw_get_freq_range (void) { return s_freq_range; }
int gw_get_svf_mode   (void) { return s_svf_mode; }

// ===========================================================================
//  ADC — 74HC4067 scan
// ===========================================================================
void gw_adc_init (void)
{
    adc_init();
    adc_gpio_init (PIN_ADC_MUXED);
    adc_gpio_init (PIN_CV1_ADC);
    adc_gpio_init (PIN_CV2_ADC);

    const uint sel[4] = { PIN_MUX_S0, PIN_MUX_S1, PIN_MUX_S2, PIN_MUX_S3 };
    for (int i = 0; i < 4; ++i)
    {
        gpio_init (sel[i]);
        gpio_set_dir (sel[i], GPIO_OUT);
        gpio_put (sel[i], 0);
    }
}

static inline void mux_address (int ch)
{
    // Written as one masked set so all four address lines change together —
    // avoids transient addresses that would briefly select a wrong channel.
    const uint32_t mask = (1u << PIN_MUX_S0) | (1u << PIN_MUX_S1)
                        | (1u << PIN_MUX_S2) | (1u << PIN_MUX_S3);
    uint32_t val = 0;
    if (ch & 0x1) val |= 1u << PIN_MUX_S0;
    if (ch & 0x2) val |= 1u << PIN_MUX_S1;
    if (ch & 0x4) val |= 1u << PIN_MUX_S2;
    if (ch & 0x8) val |= 1u << PIN_MUX_S3;
    gpio_put_masked (mask, val);
}

// Median of up to 16 samples. Insertion sort — n is tiny, this beats anything
// clever, and it is branch-predictable so the loop time stays flat.
static uint16_t median_read (int n)
{
    uint16_t s[16];
    if (n < 1)  n = 1;
    if (n > 16) n = 16;
    for (int i = 0; i < n; ++i)
    {
        const uint16_t v = adc_read();
        int j = i;
        while (j > 0 && s[j - 1] > v) { s[j] = s[j - 1]; --j; }
        s[j] = v;
    }
    return (n & 1) ? s[n / 2]
                   : (uint16_t) (((uint32_t) s[n / 2 - 1] + s[n / 2]) / 2u);
}

uint16_t gw_adc_read_mux (int channel)
{
    if (channel < 0 || channel >= GW_NUM_MUX) return 0;
    adc_select_input (ADC_CH_MUXED);
    mux_address (channel);
    busy_wait_us_32 ((uint32_t) gwt.mux_settle_us);
    return median_read ((int) gwt.adc_oversample);
}

void gw_adc_scan (uint16_t raw[GW_NUM_MUX], uint16_t* cv1, uint16_t* cv2)
{
    adc_select_input (ADC_CH_MUXED);
    for (int ch = 0; ch < GW_MUX_LIVE; ++ch)
    {
        mux_address (ch);
        busy_wait_us_32 ((uint32_t) gwt.mux_settle_us);
        raw[ch] = median_read ((int) gwt.adc_oversample);
    }
    for (int ch = GW_MUX_LIVE; ch < GW_NUM_MUX; ++ch)
        raw[ch] = 0;

    adc_select_input (ADC_CH_CV1);
    *cv1 = median_read ((int) gwt.adc_oversample);
    adc_select_input (ADC_CH_CV2);
    *cv2 = median_read ((int) gwt.adc_oversample);
}

// ===========================================================================
//  WS2812 — chain order is LED_SECT_A .. LED_GATE, nearest WS_IN first.
//  Core 0 publishes a frame; core 1 sends it. A spin lock keeps the handoff
//  atomic without either core ever blocking for long.
// ===========================================================================
static PIO      s_pio    = pio0;
static uint     s_sm     = 0;
static uint     s_offset = 0;
static bool     s_leds_ready = false;

static spin_lock_t* s_led_lock = NULL;
static GwRgb        s_led_front[GW_NUM_LEDS];   // written by core 0
static GwRgb        s_led_back[GW_NUM_LEDS];    // read by core 1
static volatile bool s_led_dirty = false;

static uint8_t s_gamma[256];
static float   s_gamma_built_for = -1.0f;

static void build_gamma (float g)
{
    for (int i = 0; i < 256; ++i)
    {
        const float x = (float) i / 255.0f;
        s_gamma[i] = (uint8_t) (powf (x, g) * 255.0f + 0.5f);
    }
    s_gamma_built_for = g;
}

void gw_leds_init (void)
{
    s_led_lock = spin_lock_instance (spin_lock_claim_unused (true));

    s_offset = pio_add_program (s_pio, &ws2812_program);
    s_sm     = (uint) pio_claim_unused_sm (s_pio, true);
    ws2812_program_init (s_pio, s_sm, s_offset, PIN_WS_DATA, 800000.0f);

    build_gamma (gwt.led_gamma);
    memset (s_led_front, 0, sizeof (s_led_front));
    memset (s_led_back,  0, sizeof (s_led_back));
    s_led_dirty  = true;
    s_leds_ready = true;
}

void gw_leds_set (const GwRgb px[GW_NUM_LEDS])
{
    if (! s_leds_ready) return;
    const uint32_t save = spin_lock_blocking (s_led_lock);
    memcpy (s_led_front, px, sizeof (s_led_front));
    s_led_dirty = true;
    spin_unlock (s_led_lock, save);
}

void gw_leds_service (void)
{
    if (! s_leds_ready) return;

    const int fps = (int) gwt.led_fps;
    const uint32_t period_us = (uint32_t) (1000000 / (fps > 0 ? fps : 60));
    static absolute_time_t next = { 0 };
    if (! time_reached (next)) return;
    next = make_timeout_time_us (period_us);

    if (! s_led_dirty) return;

    const uint32_t save = spin_lock_blocking (s_led_lock);
    memcpy (s_led_back, s_led_front, sizeof (s_led_back));
    s_led_dirty = false;
    spin_unlock (s_led_lock, save);

    if (gwt.led_gamma != s_gamma_built_for)
        build_gamma (gwt.led_gamma);

    float bright = gwt.led_brightness;
    if (bright < 0.0f) bright = 0.0f;
    if (bright > 1.0f) bright = 1.0f;

    for (int i = 0; i < GW_NUM_LEDS; ++i)
    {
        const uint8_t r = (uint8_t) (s_gamma[s_led_back[i].r] * bright);
        const uint8_t g = (uint8_t) (s_gamma[s_led_back[i].g] * bright);
        const uint8_t b = (uint8_t) (s_gamma[s_led_back[i].b] * bright);
        // WS2812 wants GRB, left-aligned in the 32-bit word (autopull is 24).
        const uint32_t grb = ((uint32_t) g << 16) | ((uint32_t) r << 8) | b;
        pio_sm_put_blocking (s_pio, s_sm, grb << 8u);
    }
}
