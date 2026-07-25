// Glitchwave 567 — fw-0.1 bring-up skeleton
// Illicit Apothecary — Raspberry Pi Pico (RP2040), pico-sdk
//
// Scope (matches FIRST_ARTICLE.md Stage 2/3):
//   - all 9 CV PWMs + CVOUT at ~122 kHz / 10-bit, console-settable
//   - 74HC4067 ADC scan (12 live channels + grounded self-test spares)
//   - stomp reading with debounce, printed on change
//   - WS2812 6-LED chain via PIO: boot rainbow, then activity display
//   - USB serial console: scan / cv / fmode / freqrange / led / help
//
// NOT in fw-0.1: tapers, gate logic, LFOs, matrix, flash persistence.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "pins.h"
#include "tunables.h"

// ---------------------------------------------------------------- PWM CVs --
static const uint pwm_pins[10] = {
    PIN_PWM_FREQ, PIN_PWM_DIRT, PIN_PWM_MIXW, PIN_PWM_MIXD, PIN_PWM_SVFF,
    PIN_PWM_SVFQ, PIN_PWM_GATE, PIN_PWM_BYP, PIN_PWM_STARVE, PIN_PWM_CVOUT,
};
static const char *pwm_names[10] = {
    "freq", "dirt", "mixw", "mixd", "svff",
    "svfq", "gate", "byp", "starve", "cvout",
};
static uint16_t pwm_level[10];   // 0..PWM_WRAP

static void pwm_cv_init(void) {
    for (int i = 0; i < 10; i++) {
        gpio_set_function(pwm_pins[i], GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(pwm_pins[i]);
        pwm_set_wrap(slice, PWM_WRAP);
        pwm_set_enabled(slice, true);
        pwm_set_gpio_level(pwm_pins[i], 0);
    }
}

static void pwm_cv_set(int idx, uint16_t level) {
    if (idx < 0 || idx >= 10) return;
    if (level > PWM_WRAP) level = PWM_WRAP;
    pwm_level[idx] = level;
    pwm_set_gpio_level(pwm_pins[idx], level);
}

// ------------------------------------------- inverted selects (FREQ/FMODE) --
// MMBT3904 level shifters INVERT: GPIO high -> shifted line LOW.
static void freq_range_set(uint bits2) {          // 0..3 logical cap select
    gpio_put(PIN_FREQ_A, !(bits2 & 1));
    gpio_put(PIN_FREQ_B, !(bits2 & 2));
}
static void fmode_set(uint bits3) {               // logical SVF mode bits
    gpio_put(PIN_FMODE_A, !(bits3 & 1));
    gpio_put(PIN_FMODE_B, !(bits3 & 2));
    gpio_put(PIN_FMODE_C, !(bits3 & 4));
}

// -------------------------------------------------------------- ADC scan ----
static uint16_t mux_val[MUX_NCHAN];
static uint16_t cv_val[2];

static uint16_t adc_read_median4(void) {
    uint16_t s[ADC_OVERSAMPLE];
    for (int i = 0; i < ADC_OVERSAMPLE; i++) s[i] = adc_read();
    // insertion sort, return mid-pair average
    for (int i = 1; i < ADC_OVERSAMPLE; i++)
        for (int j = i; j > 0 && s[j-1] > s[j]; j--) {
            uint16_t t = s[j]; s[j] = s[j-1]; s[j-1] = t;
        }
    return (s[ADC_OVERSAMPLE/2 - 1] + s[ADC_OVERSAMPLE/2]) / 2;
}

static void scan_all(void) {
    adc_select_input(0);                      // ADC0 = mux common
    for (uint ch = 0; ch < MUX_NCHAN; ch++) {
        gpio_put(PIN_MUX_S0, ch & 1);
        gpio_put(PIN_MUX_S1, ch & 2);
        gpio_put(PIN_MUX_S2, ch & 4);
        gpio_put(PIN_MUX_S3, ch & 8);
        busy_wait_us(MUX_SETTLE_US);
        mux_val[ch] = adc_read_median4();
    }
    adc_select_input(1); cv_val[0] = adc_read_median4();
    adc_select_input(2); cv_val[1] = adc_read_median4();
}

// --------------------------------------------------------------- WS2812 -----
static PIO ws_pio;
static uint ws_sm;
static uint32_t led_grb[LED_COUNT];

static void ws2812_setup(void) {
    ws_pio = pio0;
    uint offset = pio_add_program(ws_pio, &ws2812_program);
    ws_sm = pio_claim_unused_sm(ws_pio, true);
    ws2812_program_init(ws_pio, ws_sm, offset, PIN_WS2812, 800000, false);
}

static void led_set(int i, uint8_t r, uint8_t g, uint8_t b) {
    if (i < 0 || i >= LED_COUNT) return;
    uint32_t s = LED_BRIGHTNESS;
    led_grb[i] = ((g*s/255) << 16) | ((r*s/255) << 8) | (b*s/255);
}

static void led_show(void) {
    for (int i = 0; i < LED_COUNT; i++)
        pio_sm_put_blocking(ws_pio, ws_sm, led_grb[i] << 8u);
}

static void boot_rainbow(void) {
    for (int step = 0; step < LED_COUNT; step++) {
        for (int i = 0; i < LED_COUNT; i++) led_set(i, 0, 0, 0);
        led_set(step, (step*40)%256, 255-(step*40)%256, 128);
        led_show();
        sleep_ms(150);
    }
    for (int i = 0; i < LED_COUNT; i++) led_set(i, 0, 0, 0);
    led_show();
}

// --------------------------------------------------------------- stomps -----
static bool stomp_state[2];       // debounced, true = pressed
static uint8_t stomp_integ[2];

static void stomps_poll(void) {   // call at CTRL_RATE_HZ
    const uint pins[2] = { PIN_STOMP1, PIN_STOMP2 };
    for (int i = 0; i < 2; i++) {
        bool raw = !gpio_get(pins[i]);           // active low
        if (raw && stomp_integ[i] < STOMP_DEBOUNCE_MS) stomp_integ[i]++;
        if (!raw && stomp_integ[i] > 0) stomp_integ[i]--;
        bool st = stomp_integ[i] >= STOMP_DEBOUNCE_MS ? true
                : stomp_integ[i] == 0 ? false : stomp_state[i];
        if (st != stomp_state[i]) {
            stomp_state[i] = st;
            printf("stomp%d %s\n", i+1, st ? "DOWN" : "UP");
            led_set(i == 0 ? LED_BYPASS : LED_TEMPO, st ? 255 : 0, 0, st ? 0 : 0);
            led_show();
        }
    }
}

// --------------------------------------------------------------- console ----
static const char *mux_names[MUX_NCHAN] = {
    "FREQ", "GAIN", "MIX", "FIZZ", "Q", "VOL", "ENV", "VA_SENSE",
    "LOCK", "TRIM_TH", "TRIM_HO", "TRIM_FA", "GND12", "GND13", "GND14", "GND15",
};

static void cmd_scan(void) {
    scan_all();
    for (int ch = 0; ch < MUX_NCHAN; ch++)
        printf("%-8s %4u\n", mux_names[ch], mux_val[ch]);
    printf("CV1      %4u\nCV2      %4u\n", cv_val[0], cv_val[1]);
}

static void cmd_help(void) {
    printf("glitchwave567 fw-0.1 (Illicit Apothecary)\n"
           "  scan                 read all ADC channels\n"
           "  cv <name> <0-1023>   set a CV PWM (");
    for (int i = 0; i < 10; i++) printf("%s%s", pwm_names[i], i < 9 ? "," : ")\n");
    printf("  cv defaults          bench defaults (mix 50/50, gate open, fx on)\n"
           "  fmode <0-7>          SVF mode bits (logical; inversion handled)\n"
           "  freqrange <0-3>      timing cap 47n/1u/22u/470u\n"
           "  led <i> <r> <g> <b>  drive one WS2812\n");
}

static void cv_defaults(void) {
    pwm_cv_set(2, 512); pwm_cv_set(3, 512);   // mix 50/50
    pwm_cv_set(6, 1023);                       // gate/VOL open
    pwm_cv_set(7, 1023);                       // bypass = effect on
    pwm_cv_set(1, 512);                        // dirt mid
    pwm_cv_set(4, 512); pwm_cv_set(5, 200);    // SVF mid cutoff, mild Q
    pwm_cv_set(8, 0);                          // no starve
    printf("bench defaults set\n");
}

static void console_poll(void) {
    static char line[64];
    static int len = 0;
    int c = getchar_timeout_us(0);
    while (c != PICO_ERROR_TIMEOUT) {
        if (c == '\r' || c == '\n') {
            line[len] = 0; len = 0;
            char *tok = strtok(line, " ");
            if (!tok) { }
            else if (!strcmp(tok, "scan")) cmd_scan();
            else if (!strcmp(tok, "help")) cmd_help();
            else if (!strcmp(tok, "cv")) {
                char *name = strtok(NULL, " ");
                if (name && !strcmp(name, "defaults")) cv_defaults();
                else {
                    char *val = strtok(NULL, " ");
                    int idx = -1;
                    for (int i = 0; i < 10; i++)
                        if (name && !strcmp(name, pwm_names[i])) idx = i;
                    if (idx >= 0 && val) {
                        pwm_cv_set(idx, atoi(val));
                        printf("cv %s = %u\n", pwm_names[idx], pwm_level[idx]);
                    } else printf("? try: cv gate 1023\n");
                }
            }
            else if (!strcmp(tok, "fmode")) {
                char *v = strtok(NULL, " ");
                if (v) { fmode_set(atoi(v) & 7); printf("fmode %d\n", atoi(v) & 7); }
            }
            else if (!strcmp(tok, "freqrange")) {
                char *v = strtok(NULL, " ");
                if (v) { freq_range_set(atoi(v) & 3); printf("freqrange %d\n", atoi(v) & 3); }
            }
            else if (!strcmp(tok, "led")) {
                char *i = strtok(NULL, " "), *r = strtok(NULL, " ");
                char *g = strtok(NULL, " "), *b = strtok(NULL, " ");
                if (i && r && g && b) {
                    led_set(atoi(i), atoi(r), atoi(g), atoi(b));
                    led_show();
                }
            }
            else printf("? 'help'\n");
        } else if (len < 63) line[len++] = (char)c;
        c = getchar_timeout_us(0);
    }
}

// ------------------------------------------------------------------ main ----
int main(void) {
    stdio_init_all();

    // discrete outputs
    const uint outs[] = { PIN_FREQ_A, PIN_FREQ_B, PIN_FMODE_A, PIN_FMODE_B,
                          PIN_FMODE_C, PIN_MUX_S0, PIN_MUX_S1, PIN_MUX_S2,
                          PIN_MUX_S3 };
    for (unsigned i = 0; i < sizeof(outs)/sizeof(outs[0]); i++) {
        gpio_init(outs[i]);
        gpio_set_dir(outs[i], GPIO_OUT);
    }
    freq_range_set(1);    // 1u default cap — mid range
    fmode_set(0);         // LP

    // stomps
    gpio_init(PIN_STOMP1); gpio_set_dir(PIN_STOMP1, GPIO_IN);
    gpio_init(PIN_STOMP2); gpio_set_dir(PIN_STOMP2, GPIO_IN);
    // (board has 10k pullups; no internal pulls needed)

    // ADC
    adc_init();
    adc_gpio_init(PIN_ADC_MUXED);
    adc_gpio_init(PIN_ADC_CV1);
    adc_gpio_init(PIN_ADC_CV2);

    pwm_cv_init();
    ws2812_setup();
    boot_rainbow();

    printf("\nglitchwave567 fw-0.1 — 'help' for commands\n");

    absolute_time_t next = make_timeout_time_ms(1);
    uint32_t tick = 0;
    while (true) {
        sleep_until(next);
        next = delayed_by_ms(next, 1000 / CTRL_RATE_HZ);
        stomps_poll();
        console_poll();
        if (++tick % 100 == 0) {          // 10 Hz background scan
            scan_all();
            // env-reactive section LEDs (simple bring-up display)
            uint8_t v = (uint8_t)(mux_val[MUX_ENV] >> 4);
            led_set(LED_SECT_A, 0, v, v/4);
            led_set(LED_GATE, v, 0, 0);
            led_show();
        }
    }
}
