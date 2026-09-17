# WTF -- backlog

Things decided but not yet built. Nothing in here has been compiled.

---

## v0.50 (next) -- rework the stomp gestures

Jason, 2026-09-17. **Do not build until told.**

### 1. Circuit kills move from a double tap to a MEDIUM PRESS

A double tap is no longer how you kill a circuit. Instead: **one press, held
longer than 1/3 second and released before the 3 second hold takes.**

| stomp | medium press (0.33 s .. 3 s) |
|---|---|
| A | fuzz on/off |
| B | 567 on/off |
| C | envelope follower + filter on/off |

The point is certainty. A tap-tempo tap is a flick, a layer hold is three
full seconds, and a circuit kill is the deliberate press in between. No
window to wait out, no counting, no chance of the pedal guessing wrong.

**It must fire on RELEASE, not at the 0.33 s mark.** If it fired the moment
the timer passed, then holding A for three seconds to reach Layer X would
kill the fuzz on the way there every single time. Firing on release means
the press duration is already known: 0.33 s to 3 s toggles the circuit,
past 3 s is a hold and toggles nothing.

### 2. Stomp C steps MIX on 3 presses, not 4

Currently C x4+ steps MIX 0/25/50/75/100. Make it **x3**, and step on the
third press the way the tempo taps fire on theirs.

### 3. What this frees up

The double tap has no job left, so the burst counter only has to tell a
tempo/MIX gesture (3 short taps) from nothing. `n == 2` stops being a
special case, which takes a whole branch out of `serviceStomps()`.

### Unchanged

- A x3 = LFO 1 tap tempo, B x3 = LFO 2 tap tempo
- Hold 3 s: A = Layer X, B = Layer Y, A+B+C = Layer Z, C = bypass
- A+B = preset save (CW ring), B+C = preset recall (CCW ring)
- Right-click still latches a stomp instantly in the plugin

### Two things to decide when we build it

1. **Is 1/3 second long enough?** A tempo tap on a real switch is usually
   80 to 200 ms, so 333 ms leaves decent margin. But a boot lingering on a
   soft switch can sit there 300 ms without meaning to, and that would kill
   the fuzz mid-song. Suggest building it at 333 ms since that is what you
   asked for, then trying it with a foot; if it misfires, 400 to 500 ms is
   the obvious next stop and still nowhere near the 3 s hold.
2. **The same change belongs in the Pico firmware**, not just the plugin,
   or the sim and the pedal stop agreeing. The firmware reads the two (soon
   three) stomps on GP16/GP17 and would need the same press-duration
   classifier.

### Files this touches

- `src/PluginEditor.cpp` -- `stompTapped()`, `serviceStomps()`, the
  `onTap` / `onPress` / `onRelease` lambdas for the three stomps
- `src/PluginEditor.h` -- gesture constants (`kTapWinMin`, `kTapWinMax`,
  `kTapWinScale` mostly retire; add a medium-press floor), and the hint
  line text
- `docs/MODS.md` -- changelog entry
