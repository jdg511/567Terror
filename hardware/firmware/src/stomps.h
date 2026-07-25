// ============================================================================
//  stomps.h — two footswitches, five gestures.
//
//    STOMP1 tap        -> bypass toggle (10 ms equal-power crossfade)
//    STOMP1 hold       -> (reserved; currently reports as a hold event)
//    STOMP2 taps       -> tap tempo, sets the LFO rate
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
    bool s1_tap;        // one clean short press of STOMP1
    bool s1_hold;       // STOMP1 crossed the hold threshold
    bool s2_tap;
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
