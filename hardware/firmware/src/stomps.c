// ============================================================================
//  stomps.c
// ============================================================================
#include "stomps.h"
#include "board.h"
#include "tunables.h"
#include "tapers.h"

#include "pico/stdlib.h"
#include <math.h>

// ---------------------------------------------------------------------------
//  Debounce: an integrator rather than a timer. Contact bounce has to sustain
//  for stomp_debounce_ms before we believe it, which is what makes a worn
//  3PDT still feel decisive instead of double-triggering.
// ---------------------------------------------------------------------------
typedef struct {
    uint  pin;
    float integ;        // 0..1, where 1 == pressed
    bool  state;        // debounced
    bool  prev;
    float held_sec;
    bool  hold_fired;
    bool  consumed;     // this press already became part of a both-gesture
} Sw;

static Sw s_sw[2];

static bool s_both_active   = false;
static float s_both_timer   = 0.0f;

void gw_stomps_init (void)
{
    const uint pins[2] = { PIN_STOMP1, PIN_STOMP2 };
    for (int i = 0; i < 2; ++i)
    {
        gpio_init (pins[i]);
        gpio_set_dir (pins[i], GPIO_IN);
        gpio_pull_up (pins[i]);          // belt and braces; the board has 10k too
        s_sw[i].pin        = pins[i];
        s_sw[i].integ      = 0.0f;
        s_sw[i].state      = false;
        s_sw[i].prev       = false;
        s_sw[i].held_sec   = 0.0f;
        s_sw[i].hold_fired = false;
        s_sw[i].consumed   = false;
    }
    s_both_active = false;
    s_both_timer  = 0.0f;
}

bool gw_stomp_raw (int which)
{
    if (which < 1 || which > 2) return false;
    return ! gpio_get (s_sw[which - 1].pin);      // active LOW
}

static void debounce (Sw* sw, float dt)
{
    const bool raw = ! gpio_get (sw->pin);
    const float c  = gw_onepole_coeff (gwt.stomp_debounce_ms,
                                       dt > 1e-9f ? 1.0f / dt : 1000.0f);
    sw->integ += c * ((raw ? 1.0f : 0.0f) - sw->integ);

    sw->prev = sw->state;
    if (! sw->state && sw->integ > 0.75f) sw->state = true;
    if (  sw->state && sw->integ < 0.25f) sw->state = false;

    if (sw->state) sw->held_sec += dt;
    else         { sw->held_sec = 0.0f; sw->hold_fired = false; }
}

GwStompEvents gw_stomps_tick (float dt)
{
    GwStompEvents ev = { 0 };

    debounce (&s_sw[0], dt);
    debounce (&s_sw[1], dt);

    const float hold_sec = (float) gwt.stomp_hold_ms * 0.001f;
    const float both_sec = (float) gwt.stomp_both_ms * 0.001f;

    // ---- both-held gesture wins over everything else ----------------------
    if (s_sw[0].state && s_sw[1].state)
    {
        s_both_timer += dt;
        if (! s_both_active && s_both_timer >= both_sec)
        {
            s_both_active = true;
            ev.both_start = true;
            // Neither switch gets to fire its own tap/hold after this.
            s_sw[0].consumed = s_sw[1].consumed = true;
            s_sw[0].hold_fired = s_sw[1].hold_fired = true;
        }
    }
    else
    {
        s_both_timer = 0.0f;
        if (s_both_active && ! s_sw[0].state && ! s_sw[1].state)
        {
            s_both_active = false;
            ev.both_end   = true;
        }
    }
    ev.both_active = s_both_active;

    // While a both-gesture is running or unwinding, suppress single events.
    const bool suppress = s_both_active;

    for (int i = 0; i < 2; ++i)
    {
        Sw* sw = &s_sw[i];

        // hold fires the moment the threshold is crossed, foot still down
        if (sw->state && ! sw->hold_fired && sw->held_sec >= hold_sec)
        {
            sw->hold_fired = true;
            if (! suppress && ! sw->consumed)
            {
                if (i == 0) ev.s1_hold = true; else ev.s2_hold = true;
            }
        }

        // tap fires on release, only if it never became a hold
        if (sw->prev && ! sw->state)
        {
            const bool was_short = ! sw->hold_fired;
            if (was_short && ! suppress && ! sw->consumed)
            {
                if (i == 0) ev.s1_tap = true; else ev.s2_tap = true;
            }
            sw->consumed   = false;
            sw->hold_fired = false;
        }
    }

    return ev;
}

// ===========================================================================
//  Tap tempo — averages the last few intervals, ignores anything implausible.
// ===========================================================================
static float s_tap_intervals[8];
static int   s_tap_count   = 0;
static float s_since_tap   = 1e9f;
static float s_rate_hz     = 2.0f;
static float s_blink_phase = 0.0f;

bool gw_tap_tick (bool tapped, float dt)
{
    s_since_tap += dt;

    // blink phase always runs so the tempo LED keeps time
    s_blink_phase += s_rate_hz * dt;
    if (s_blink_phase >= 1.0f) s_blink_phase -= floorf (s_blink_phase);

    const float timeout = (float) gwt.tap_timeout_ms * 0.001f;
    if (s_since_tap > timeout) s_tap_count = 0;      // forget a stale sequence

    if (! tapped) return false;

    const float interval = s_since_tap;
    s_since_tap = 0.0f;

    const float tmin = (float) gwt.tap_min_ms * 0.001f;
    const float tmax = (float) gwt.tap_max_ms * 0.001f;

    if (interval < tmin || interval > tmax)
    {
        s_tap_count = 0;                             // first tap of a new sequence
        return false;
    }

    int avg_n = (int) gwt.tap_avg_taps;
    if (avg_n < 2) avg_n = 2;
    if (avg_n > 8) avg_n = 8;
    // Lowering tap_avg_taps mid-session must drop the stale intervals too, or
    // the mean below keeps averaging in taps from the previous tempo.
    if (s_tap_count > avg_n) s_tap_count = avg_n;

    if (s_tap_count < avg_n) ++s_tap_count;
    for (int i = avg_n - 1; i > 0; --i) s_tap_intervals[i] = s_tap_intervals[i - 1];
    s_tap_intervals[0] = interval;

    float sum = 0.0f;
    for (int i = 0; i < s_tap_count; ++i) sum += s_tap_intervals[i];
    const float mean = sum / (float) s_tap_count;

    if (mean > 1e-4f)
    {
        s_rate_hz     = gw_clampf (1.0f / mean, 0.01f, 200.0f);
        s_blink_phase = 0.0f;                        // land the blink on the tap
        return true;
    }
    return false;
}

float gw_tap_rate_hz (void) { return s_rate_hz; }
bool  gw_tap_blink   (void) { return s_blink_phase < 0.15f; }
