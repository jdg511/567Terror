# WTF -- backlog

Things decided but not yet built.

---

## Open

### The Pico firmware needs the v0.50 press classifier

v0.50 changed what a stomp press means in the plugin: under 333 ms is a
flick, 333 ms to 3 s kills that stomp's circuit, past 3 s is a hold. The
firmware still reads the stomps the old way, so the sim and the pedal now
disagree about the same gesture. The firmware reads them on GP16/GP17 (plus
a third pin once stomp C is on the board) and needs the same duration test,
fired on release.

### Is 1/3 of a second long enough?

Shipped at 333 ms because that is what was asked for. A tempo tap on a real
switch runs 80 to 200 ms, so there is margin, but a boot lingering on a soft
switch can sit at 300 ms without meaning to, and that would kill the fuzz
mid-song. If it misfires under a foot, 400 to 500 ms is the obvious next
stop and still nowhere near the 3 second hold. The constant is
`kStompMediumMs` at the top of `src/PluginEditor.h`, one number.

### Stomp C's LED has no job

A's LED blinks the tempo and B's shows bypass. C's has been dark since it
was added. Not a bug, just an unused indicator, and it was left alone in
v0.50 on purpose (the LEDs were reverted to their pre-v0.47 behaviour).
Bypass state, env-filter state or the envelope's own level would all suit it.

### Hardware, not software

- Size the pad between the JFET and the Bazz Fuss on the breadboard.
- Consider running the LM567 at 5 V off an LDO with R16 pulled up to VA.
  Pin 8's absolute max (15 V) is independent of V+, so the chip can sit at
  its datasheet sweet spot while the Q node still swings to the full rail.
- Test LM567 unit-to-unit variance before the next fab run.

---

## Done

- **v0.50** stomp gesture rework: circuit kills on a medium press, C steps
  MIX on 3 taps, hold indication moved from the LEDs to the stomp ring.
