# WTF hardware handoff: what the plugin (v0.60) says the pedal must do

Source of truth for the next hardware session. The plugin is the spec. Where this
file and the current KiCad files disagree, the plugin wins, and Jason has said
v0.60 is correct. Nothing in `hardware/` was changed for v0.56 to v0.60.

Plugin repo tag: `v0.60` (commit 5ef91e2). Signal flow drawing: `docs/WTF_signal_flow_v0.60.drawio`.

## 1. LM567 supply rail (sim v0.56, hardware NOT done)

- Sim models a fixed LM2937-8.0 (TI LM2937IMP-8.0): 8.0 V out, about 0.15 V dropout at about 15 mA, no D105 in the path.
- Jason wants a FIXED regulator (no adjustable parts), as close to 9 V as possible without going over. LM567 absolute max is 9 V. The 8.5 V fixed parts are out of stock. A linear regulator cannot exceed Vin minus dropout.
- He picked the LM2937 over the LK112M80TR because of its reverse-polarity protection.
- Hardware change to make: replace U18 (78L09) with the SOT-223 LM2937-8.0 and delete D105. The LM2937 needs a 10 uF output cap with ESR between 10 mOhm and 3 Ohm.
- Layout warning: the SOT-223 courtyard is about 8.9 x 7.3 mm and does not fit next to U18 without rework. Jason is willing to change the layout for the better part.
- The board already has upstream P-FET reverse-polarity protection.
- LM567 detector goes deaf below 4.75 V in the sim (STARVE sags every rail).
- Also update BOM.xlsx, then rerun DRC/ERC. Baselines: MAIN 6 errors / 67 warnings, CTRL 0 errors / 2 warnings. Rebuild the fab package with `hardware/review/tools/make_fab.ps1`. Read `fab_release` memory note before any PCBWay order.

## 2. CV1 and CV2 (sim v0.60, final)

Jason: the sim must only do what the hardware can do. Dropped for good: the
"assign each CV to any knob" dropdown, the strength and slew mini knobs, and
audio players 2 and 3 as CV sources.

Both CV inputs are switched (jack-detect) inputs. Plugged means a plug is in the
jack, regardless of whether any signal is present. There is NO fallback when
plugged, even if silent.

| jack | unplugged | plugged |
|---|---|---|
| CV1 | LFO 1 depth is full (knob value) | LFO 1 depth is multiplied by a VCA driven by the CV1 level. Silent CV1 means depth 0 |
| CV2 | envelope follower listens to the raw input as it enters the pedal | envelope follower listens to CV2 only. Silent CV2 means the follower is dead |

- CV2 no longer drives LFO 2 depth. That older v0.13 plan is superseded.
- Sim CV1 detail: rectified CV1 smoothed at 15 ms, VCA gain = clamp(2 x level, 0, 1). Treat the 2x as a starting point to tune on the bench.
- In the plugin, plugged = the host sidechain bus is active. The sidechain bus is OFF by default, so an unrouted DAW track counts as unplugged. Stereo sidechain: left = CV1, right = CV2. A mono sidechain feeds both.
- Hardware implication: CV2 needs a switched jack whose normal (unplugged) path is the follower's existing input net. On the real board the follower rectifier input (R130) sits on net DRY_CLEAN, the buffered clean input, so that normalling point already exists. CV1 needs a switched jack that enables the depth VCA (unplugged = full depth). Jason expects two or more side 1/4" jacks for control voltages.

## 3. Envelope follower taps the raw input

- The follower (and the gate follower) must react only to the raw audio as it enters the pedal, never to the LM567 frequency or anything later in the chain. Earlier it behaved like a slow tremolo at the LM567 frequency, which is wrong.
- Mu-Tron style numbers in the sim: envelope cap attack 1.551 ms, decay 158.7 ms; vactrol lag 2.5 ms on, 35 ms off; then threshold, ratio, shape (kVacShape).

## 4. Audio path order in the sim (for the schematic cross-check)

Host input (plus Audio Player 1 demo clip) -> raw copy -> jack network / 40 Hz high-pass ->
input buffer (clean DRY tap) -> LM567 branch (+15 dB trim, gain stage, PLL/VCO/detector, Q node)
in parallel with the fuzz branch (optional J201 JFET, Bazz Fuss dirt) -> parallel MIX ->
envelope filter (Off / LP / BP / HP / Notch) -> output voicing (C8 DC block, 60 Hz high-pass,
+3 dB bell at 800 Hz) -> rail clip -> gate gain -> bypass crossfade against the raw copy -> out.

## 5. Other open hardware items (from earlier sessions)

- Firmware: Pico needs the v0.50 press classifier (400 ms kill, 1.2 s hold) and third-stomp parity. See `docs/BACKLOG.md`.
- Size the pad between the JFET and the Bazz Fuss on the breadboard.
- Test LM567 unit-to-unit variance before the next fab run.
- Product: "Where The Fuzz Meets The Funk", vendor Illicit Apothecary, category Filter. KiCad projects and fab package still carry the old Glitchwave 567 name until the boards are ordered.
