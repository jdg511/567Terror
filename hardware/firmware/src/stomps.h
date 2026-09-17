// ============================================================================
//  stomps.h — two footswitches, five gestures.
//
//  v0.51: the press LENGTH decides what a press was, matching the plugin:
//
//      under stomp_medium_ms (400 ms)          a flick   -> tap
//      400 ms .. stomp_hold_ms (1.2 s)                   -> MEDIUM press
//      past stomp_hold_ms                                -> hold
//
//  Taps and mediums both fire on RELEASE, since only then is the duration
//  known; the hold fires the moment it is crossed so you can lift your foot.
//
//    STOMP1 tap        -> bypass toggle (10 ms equal-power crossfade)
//    STOMP1 medium     -> (free; the plugin uses it for the fuzz kill)
//    STOMP1 hold       -> (reserved; currently reports as a hold event)
//    STOMP2 taps       -> tap tempo, sets the LFO rate
//    STOMP2 medium     -> (free; the plugin uses it for the 567 kill)
//    STOMP2 hold       -> cycle the SVF mode LP -> BP -> HP -> Notch
//    BOTH held         -> STARVE: sag the Bazz Fuss rail toward the 5 V floor
//                         while held, recover on release
//
//  Both switches are active-LOW with a 10k pullup and a 100n cap on the board.
// ============================================================================
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GwStompEvents {
    bool s1_tap;        // one clean short press of STOMP1 (< stomp_medium_ms)
    bool s1_medium;     // v0.51: held past medium, released before hold
    bool s1_hold;       // STOMP1 crossed the hold threshold
    bool s2_tap;
    bool s2_medium;     // v0.51
    bool s2_hold;
    bool both_start;    // both went down together
    bool both_end;      // ...and have now been released
    bool both_active;   // still held
} GwStompEvents;

void gw_stomps_init (void);

// Call once per control tick. dt in seconds.
GwStompEvents gw_stomps_tick (float dt);

bool gw_stomp_raw (int which);      // 1 or 2, true = pressed (already inverted)

// ---- tap tempo -------------------------------------------------------------
// Feed it the tap events; it returns true when a new tempo has been committed.
bool  gw_tap_tick   (bool tapped, float dt);
float gw_tap_rate_hz (void);        // last committed rate
bool  gw_tap_blink   (void);        // a blink phase at the tap rate, for the LED

#ifdef __cplusplus
}
#endif
