# Glitchwave 567 — Step 2 Mods (v0.2 … v0.31)

## v0.31 — selector knobs re-sync on layer entry (Jason's diagnosis)

Jason pinned it: values changed correctly in Y/Z, but re-entering the layer
left the selector knobs (Shape/Target/Mode/DRV-RNG) showing the X position —
they're zone-driven with no attachment, so nothing repositioned them, and
the first touch teleported the selection to the stale position's zone. (A
was immune because Starve is a real attachment — the tell.) Now every layer
entry parks each selector knob at the centre of its current selection's
zone: the position always tells the truth, and a small turn steps to the
neighbouring choice instead of jumping.

---



## v0.30 — Jason's X/Y/Z/A layer spec + permanent key readout

Fresh layer layout per Jason's table (X = plain knobs, exactly the v0.23
feel; every control audited — nothing homeless):

| knob      | X        | Y (TAP/INS)   | Z (BYP/DEL)  | A (both) |
|-----------|----------|---------------|--------------|----------|
| Freq      | Freq     | Gain (dirt)   | LFO1 Depth   | dead     |
| LPF       | LPF      | Res           | LFO2 Depth   | dead     |
| Mix       | Mix      | Vol           | DRV/RNG      | STARVE   |
| LFO1 Rate | Rate     | LFO1 Shape    | LFO1 Target  | dead     |
| LFO2 Rate | Rate     | LFO2 Shape    | LFO2 Target  | dead     |
| Env Gain  | Gain     | Mode          | Env Target   | dead     |

Keys: **INS = TAP held (Y), DEL = BYPASS held (Z), both = A** (Jason's
mapping — note reversed vs v0.29). Right-click latch stays. Stomps now
light while their key is held (indicators) and remain clickable. Tap tempo
unchanged: rolling 4-tap average, LFO1 (BYPASS held: LFO2), 0.2–20 Hz.

**Permanent key-state readout** in the footswitch strip — raw INS/DEL as
the app sees them + the active layer letter, coloured per layer. Stays on
every version until the control scheme is signed off, so any key problem is
visible in one glance: key held but INS[-] = the app never saw it; INS[#]
but LAYER X = logic bug; both right but knob dead = slider bug.

Kept from the C-scheme era: 8 clip modes A–H, boost 2nd-to-last, LFO2 rate
knob, drag-latch (a knob keeps the function it was grabbed with), velocity
fine-adjust disabled, double-click = default.

---



## v0.29 — sim keys: DELETE = TAP held, INSERT = BYPASS held

Jason's pick after the lone-modifier interceptor saga: ordinary keys that
hotkey utilities leave alone. DELETE held = C2 row, INSERT held = C3 row,
both = Starve. Polled globally (GetAsyncKeyState), works without keyboard
focus. Right-click stomp latch (v0.28) stays as the mouse-only alternative.

---



## v0.28 — right-click latch: the sim no longer depends on modifier keys

Frame analysis of Jason's second video showed both layers working during
long stable key-holds, then the layer state bouncing C3→C1→C3 at a regular
~1.2 s rhythm with the cursor parked — and across v0.24–27 every LONE
modifier (ALT, then CTRL, then SHIFT) misbehaved while every COMBO passed.
That's a system-level lone-modifier interceptor (hotkey utility / keyboard
driver), not the plugin. Fix: **right-click a stomp to LATCH it held**
(accent ring shows the latch; right-click again to release). Latch TAP = C2,
latch BYPASS = C3, latch both = Starve. Pure mouse, host-independent.
CTRL/SHIFT still work as best-effort extras. Bonus: bypass-latch + tap ×4
(LFO2 tempo) is now a one-handed gesture.

---



## v0.27 — bugfix: layer switches no longer bleed into the wrong knob

Releasing (or pressing) a layer key while still mid-drag re-attached the
knob to a different parameter, and the rest of the drag wrote into it —
silently slamming C1 values (VOL to 0% = total silence, FREQ to 1 Hz, both
rates to 20 Hz, MIX to full wet). Jason's video showed the layers themselves
worked; the bleed was the "nothing works" feeling. Now a drag latches the
function it started on and layer swaps wait until every knob is released.
Tip that also came out of this: **double-click any knob (no layer held) to
reset that parameter to its default.**

---



## v0.26 — sim key change: BYPASS hold = SHIFT (was ALT)

Plain ALT+drag never reached the plugin on Jason's machine (Windows / a
window-management utility grabs lone-ALT drags — ALT+CTRL got through, ALT
alone didn't). The C3 sim key is now **SHIFT**; C2 stays CTRL; the secret
starve combo is **CTRL+SHIFT** + Mix knob. Hardware unaffected — the real
pedal uses the physical stomps. Diagram regenerated (v0.26 files).

---



## v0.25 — bugfix: knobs frozen while a layer key is held

Holding BYPASS/ALT (or TAP/CTRL) changed the labels but knob drags did
nothing. Cause: JUCE sliders silently switch a drag into "velocity"
fine-adjust mode when CTRL/ALT is held — the same keys the C2/C3 layers use —
so slow drags produced ~zero movement. Fixed by disabling the modifier-swap
(`setVelocityModeParameters (…, userCanPressKeyToSwapMode = false)`) on all
knobs. No DSP changes.

---



## v0.24 — the C1/C2/C3 layer scheme (no section buttons), clip modes E–H

**Controls completely reworked** (Jason's spec). Six knobs — Freq, LPF, Mix |
LFO1 Rate, LFO2 Rate, Env Gain — each with three layers:

* **C1** (nothing held): Freq · LPF · Mix · Rate · Rate · Gain
* **C2** (TAP stomp held, sim CTRL): Gain · Res · Vol · Target · Target · Target
* **C3** (BYPASS stomp held, sim ALT): **LFO1 Depth · LFO2 Depth · DRV/RNG** ·
  Shape A&B · Shape A&B · Mode
* **BOTH stomps held** (secret/easter egg, unlabeled on the real pedal): Mix
  knob = **STARVE**; every other knob goes **dead**.

All three 2nd-row buttons (LFO1, LFO2, ENV) are **gone**. LFO2 gets its own
RATE knob. Targets/shapes/mode are picked by knob position (8 / 16 / 5 zones);
DRV/RNG rides the Mix knob's quarters in C3.

**Tap tempo** is now a 4-tap average: 1–3 taps arm only, the 4th (and every
tap after — rolling window of the last 4) commits. TAP alone = LFO1 rate;
with BYPASS held = LFO2 rate. Range 0.2–20 Hz, and both rate KNOBS moved to
the same 0.2–20 Hz. A committed tap re-seeds that LFO's chaos generators
(new `retriggerLfo1`). A >5 s gap starts a new chain.

**LEDs fixed + re-timed**: the section LEDs now show the live value colour of
whatever the active layer is editing (the v0.21–23 bug where the colour only
appeared on release is gone). Bank A blinks **2 Hz**, Bank B **5 Hz**,
DRV/RNG is **solid** (no blink), Mode stays 3 Hz, depth = blue @ %.

**Boost clarified**: the switchable +6 dB is the output make-up boost and now
sits literally 2nd-to-last, directly feeding the clip stage (HPF and 800 Hz
bell come before it — same math, matches the hardware order). The **+15 dB
pre-567 trim was never touched** — it feeds the wet branch only; the dry
path never sees it.

**Clip modes E–H added** (the asymmetric ladders from the graphs, by request):
E = −9/rail, F = −9/−3, G = −6/rail, H = −3/rail (positive half first; the
"rail" half runs clean into a hard stop at the rail). New −3 ladder: bands
−3/−2/−1/0, 2 dB knees, unity below −4 dB, rail at +11. CLIP button cycles
all 8. All curves verified from the compiled code (32 checks).

draw.io drawing added: `docs/glitchwave567_v0.24.drawio` (Signal Flow +
Controls pages) with PNG previews.

---



## v0.23 — softer A ladder (−6 onset) + switchable +6 dB boost

* **Clip mode A is now a −6 ladder** (was the −18 v0.21 curve). Same
  2:1/4:1/8:1 output-referred bands, but the onset sits only 6 dB below the
  rail with tighter 4 dB knees: unity up to −8 dB(rail), out(0 dB) = −3.5,
  rail reached at +22 dB in. GR at the rail is just 3.5 dB — the gentlest of
  the ladder pair; B (−9) is unchanged. All anchors, knee continuity, band
  slopes and 18 V rail-tracking re-verified from the compiled code.
* **The +6 dB output boost is now a button** (`boost6`, default **ON**) in the
  FOOTSWITCHES · POWER strip, so the make-up gain can be auditioned separately
  from the clip stages. It still sits where it did in the chain: DC block →
  ±6 dB boost → 60 Hz HPF → +3 dB @ 800 Hz → clip stage. Toggle measured
  +6.02 dB through the full circuit at low drive.
* CLIP button labels renamed accordingly ("A: -6 Ladder"); strip re-laid to
  fit the new +6 dB button.

---


## v0.22 — output clip AUDITION build (pick one, then it gets hardwired)

The CLIP button in the FOOTSWITCHES · POWER strip cycles four output stages
(all rail-referenced, all ride the 9/18 V supply and the starve):

* **A — −18 ladder** (the v0.21 curve): 2:1/4:1/8:1 with onset 18 dB below
  the rail. Strongest; squeezes clean playing (GR 10.5 dB at the rail).
* **B — −9 ladder**: same ratios, onset −9 dB — clean playing untouched,
  GR 5.25 dB at the rail, still 20–40 dB of squash when slammed.
* **D — JFET**: J201 square-law stage (Fetzer-style bias) + output cap.
  Tube-like: curvature everywhere, cutoff side rounds to a zero-slope stop,
  ohmic side corners; **2nd harmonic −18 dB** at moderate drive (measured).
* **D+B — JFET into the −9 ladder**: tube colour + rail safety.

All four verified from the compiled code (curve points, harmonics, unity
floors). Winner gets hardwired next version; in hardware A/B are an op-amp +
diode-ladder stage, D is a J201 + 3 resistors + 2 caps.

---


## v0.21 — one LED per section, ratio soft-clip, 9/18 V power + secret starve

**One LED per section** (LFO 1, LFO 2, ENV — plus the shape banks re-timed):

* LFO idle = **white**, breathing the LFO wave ("white is rate").
* Shape display (after a shape tap, ~1.5 s): hue = slot, **Bank A flashes
  3 Hz, Bank B flashes 6 Hz**.
* Target display: **solid** hue (Off = dim). Depth gestures: blue @ depth %.
* ENV: idle white = **envelope level**; MODE hues flash **3 Hz**; the four
  drive×range combos flash **6 Hz**; target solid.
* ALL LED columns are gone — panels are knob + button + one LED + printed
  legend charts (future silkscreen).

**Target routing changes**: LFO 2 loses Gain. LFO 1 gains **Env Gain** and
**Env Level** (new EnvLevel mod target, output ×1..×3). The env follower
gains **LFO1 Rate / LFO1 Depth** (applied pre-LFO1 each block). All three
lists are 8 entries. New IDs `lfo1target5` / `lfo2target4` / `envtarget5`.

**Output chain reordered + ratio soft clip** (the absolute last thing):
`+6 dB boost → HP 60 Hz + 3 dB bell 800 Hz → rail soft clip`.
The clip is Jason's ladder, referenced to the rail: unity below −18 dB, then
2:1, 4:1, and **8:1 through the last 6 dB before hard clipping**, with 6 dB
quadratic knees for curvature. Numerically verified (continuity, exact
mid-band ratios, monotonic).

**Power (v0.21 sim + hardware spec)**:

* **9–18 V centre-negative**; 18 V = +6 dB analogue headroom (the whole clip
  curve rides the rail). SUPPLY "jack" button in the sim swaps the adapter.
* Hardware notes: series polarity protection (P-FET), RC + ferrite supply
  filtering, ≥25 V caps, 36 V-rated opamps. **The LM567 maxes at ~9 V and the
  Pico at 5 V/3.3 V — both run from their own regulators at any supply.**
* **Secret starve**: hold BYPASS + TAP TEMPO and turn MIX (sim: CTRL + ALT +
  drag MIX). Sags the analogue rail from the supply toward a **5 V floor**
  (never below; digital rails untouched). Starving chokes the Bazz Fuss
  rails, sags its bias (asymmetry), widens its dead zone and adds a
  crossover sputter gate — dying-battery velcro, by design.
* **Bypass stomp** added (buffered bypass in the sim, 10 ms crossfade) with
  a proper green status LED, in a new FOOTSWITCHES · POWER strip.

---


## v0.20 — NeoPixel shape indicator preview

The two 16-row SHAPE LED columns are replaced by **one RGB "NeoPixel" LED per
LFO** (hardware: WS2812s chained on a single Pico pin):

* **Hue = the slot within the bank** (same hue for the A and B shape sharing
  a slot): red = Ramp Up/Lorenz, orange = Ramp Dn/Rossler, yellow =
  Square/Drunk Walk, green = Triangle/Perlin, cyan = Sine/Wobble, blue =
  Sweep/Glitch, violet = Rnd Slope/White Noise, pink = S&H/Pink Noise.
* **Solid = Bank A; flashing at 7 Hz = Bank B.**
* A printed hue chart on each LFO panel maps colour → A/B names (that chart
  becomes enclosure artwork on the real pedal).
* Nothing else changed — tap-cycling through all 16 shapes, targets, and the
  v0.19 control scheme are untouched.

Point of the preview: if this reads well, the pedal drops ~32 panel LEDs and
likely a whole enclosure size (1590DD → 1590XX).

---


## v0.19 — the shift-stomp control scheme (dual-function everything)

**The tap tempo stomp is now the pedal's SHIFT key** (sim: holding CTRL =
holding the stomp):

* **Tap** (release < 750 ms, nothing else used) = tempo tap, 0.02–10 Hz,
  timed from the press instant; re-seeds the chaos waves.
* **Hold alone 750 ms** = depth sweep: LED → 2nd colour (blue), brightness =
  depth %; depth rides the 4 s sine-like traverse from its current % in its
  remembered direction, turning around **instantly** at 0 %/100 % (the 300 ms
  dwells are gone). Release = freeze + LED back to 1st colour.
* **Hold + move a knob** = that knob's 2nd function (and cancels the sweep).
* **Hold + press a section button** = step that section's TARGET one notch
  per press (LEDs show the 2nd colour while shifted).

**Dual-function knobs** (plain = 1st, shifted = 2nd):

| Knob | 1st | 2nd |
|------|-----|-----|
| Pedal 1 | FREQ | GAIN (dirt) |
| Pedal 2 | LPF  | RES |
| Pedal 3 | MIX  | VOL |
| LFO 1   | RATE | DEPTH (also via holding the LFO1 button) |
| ENV     | GAIN | drive×range combo (also via holding the ENV button) |

The pedal row is physically **3 knobs** now and LFO 1's DEPTH knob is gone:
**8 pots total** (3 pedal + LFO1 RATE + ENV GAIN + 3 gate), down from 12.

**Button timing** (all three section buttons): hold threshold **750 ms**,
then the cycle steps every **750 ms**. Tap stays instant-on-release.
The ENV button's hold now cycles its **TARGET** (like the LFOs); drive×range
moved to hold + GAIN knob (knob quarters = up&hi / up&low / down&hi /
down&low). The env TARGET column is no longer directly clickable — every
selector in the plugin is now an indicator.

Knob-shift details: while a knob is shifted, its label swaps to the 2nd name
in the 2nd colour, and the section LED goes blue (LFO1: brightness = depth %).
The 1st-function value is never disturbed — releasing the shift snaps the
knob display back (hardware will do this with soft-takeover on the Pico).

**Bazz Fuss**: its LED is gone (it's always on); GAIN floor lowered to
**×1.1** (was ×2), ceiling stays ×300.

---


## v0.18 — THE waveform set: Bank A classics + Bank B chaos (Jason's plan)

The 16 waveforms Jason chose (from the signal-flow-diagram chat), on both
LFOs. This replaces the v0.17 TAPLFO list (those DSP shapes stay in the code,
just unlisted).

**Bank A — the classics:**
Ramp Up, Ramp Dn, Square, Triangle, Sine, Sweep, Rand Slope, **S&H**.

**Bank B — the fun stuff:**

* **Lorenz** — chaos attractor; swoopy, orbit-like, never repeats.
* **Rossler** — chaos attractor; smoother, spiral-y.
* **Drunk Walk** — brownian wander with momentum; great slow filter drift.
* **Perlin** — 3 octaves of layered smooth randomness; organic.
* **Wobble** — sine whose depth randomly swells and fades (new swell each cycle).
* **Glitch** — mostly calm (tiny ±0.1 wander) with sudden brief chaotic
  flurries (80–250 ms of full-range jumps) — very on-brand.
* **White Noise / Pink Noise** — holding Bank B's two open slots until the
  first batch has been auditioned (swap candidates welcome).

**Design nicety (as planned):** the chaos/drift waves aren't periodic, so on
LFO 2 a tempo tap sets their **time-scale** (how fast the attractor moves) and
**resets their state** — feels like sync under your foot. RATE does the same
time-scaling on LFO 1. New param IDs `lfo1shape5` / `lfo2shape4`.

All six new generators numerically verified: bounded ±1, finite at rate
extremes (0.02–10 Hz, 30 s runs), drunk walk steps < 0.001/sample with full
wander range, wobble peak envelope swings 0.54→0.96, glitch ~15 % loud
samples, retrigger-safe mid-run.

---

## v0.17 — TAPLFO 3D waveforms + dual-hold depth sweep (list superseded)

**The full Electric Druid TAPLFO 3D waveform set on both LFOs** (16 waves,
datasheet order, both wave sets), plus the White/Pink noise LFOs from v0.4:

* Original set: Ramp Up, Ramp Dn, Pulse, Tri, Sine, **Sweep** (smooth scoop),
  **Lumps** (smooth arch), Rand Lvls (S&H).
* Alternate set: **Ramp+Oct**, **Quad Ramp** (4 quick teeth then rest),
  **Quad Pulse** (4 quick pulses then rest), **Tri Step** (4-level staircase),
  **Sine+Oct**, **Sine+3rd**, **Sine+4th** (harmonic sums, normalised),
  **Rand Slope** (lines between random levels).
* White Noise / Pink Noise stay at the end of the list (rate = noise LPF).
* New param IDs `lfo1shape4` / `lfo2shape3` (list grew 8 → 18).

**LFO 2 depth: dual-button hold** (replaces the v0.16 double-tap loop — 100 ms
was too fast to play):

* The button is a plain **rate tap-tempo** button again. LED 1st colour
  (amber), breathing with the LFO. Only two LED colours needed now.
* **Hold BOTH LFO 2 buttons (rate/depth + shape/target) for 750 ms** →
  LED switches to the 2nd colour (blue) and the depth starts riding a
  sine-like wave **from its current % in its remembered direction**:
  0 ↔ 100 % in 4 s per traverse, easing into the extremes, **pausing 300 ms
  at 0 % and at 100 %** before turning around. LED brightness = depth %.
* **Releasing either button freezes the depth** exactly where it is; the wave
  resumes from that point (same direction) next time.
* **In the simulator: hold CTRL + the rate/depth button** to stand in for
  holding both buttons.
* Hardware note: dual-hold must take priority over each button's single-hold
  action in the MCU firmware (tap/hold actions fire only if the other button
  is up).

Also: fixed the garbled "·  ↑ →" characters in the panel printing (UTF-8).

---

## v0.16 — LFO 2 depth control loop (superseded by v0.17)

The LFO 2 button is normally a **rate tap-tempo** button (LED = 1st colour,
amber, breathing with the LFO). The depth is set through a timed control loop,
exactly as specced for the hardware firmware:

* **Double tap** = two presses **≤ 100 ms** apart, then **no press for 0.5 s**.
* Double tap in rate mode → **ARMED**: LED = 2nd colour (blue) at 100 %,
  fading to 0 % over **5 s**.
  * no press in 5 s → back to rate mode (1st colour), nothing changes.
  * single press → back to rate mode, nothing changes.
  * double tap (2nd press ≤ 100 ms) → the loop begins at **point A**.
* **Point A**: LED = 1st colour blinking **20 Hz for 0.5 s**.
  * single tap during the blink → **depth = 0 %**, exit.
  * double tap during the blink → jump to **point B**.
  * untouched → **ramp up**: LED = 3rd colour (white); LED brightness **and**
    depth rise linearly **0 → 100 % in 4 s**. Tap anytime = keep the current
    depth, exit. You hear the depth sweep — tap when it sounds right.
* **Point B** (after the ramp up, or straight from A by double tap):
  LED = 3rd colour blinking 20 Hz for 0.5 s, **depth = 100 %**.
  * tap during the blink → keep 100 %, exit.
  * untouched → **ramp down**: brightness and depth fall **100 → 0 % in 4 s**.
    Tap anytime = keep the current depth, exit.
  * untouched again → the loop starts over at point A.
* Exiting always returns to rate mode + 1st colour; the exit press never
  counts as a tempo tap.

Also: the editor now redraws at 60 fps so the 20 Hz blinks render.
(The v0.15 hold-to-sweep depth behaviour is gone — replaced by this loop.)

---

## v0.15 — LFO 2 rate+depth on one button (first pass, superseded)

* LFO 2's RATE/DEPTH knobs removed; one button: TAP = tap tempo,
  HOLD = depth sweeps up/down 10 %/tick, ping-ponging. Replaced in v0.16 by
  the depth control loop above.
* Live "x.xx Hz · yy %" readout under the button (kept in v0.16).

---

## v0.14 — one-button tap/hold controls + more waveforms

* **LFO waveforms are now 8**: Sine, Triangle, **Ramp Up, Ramp Dn**, Square,
  Rand S&H, White Noise, Pink Noise (both LFOs).
* **Filter gains a Notch mode**: Off / LP / BP / HP / **Notch** (free on a
  hardware SVF).
* **One button per section** (Jason's idea — this is the hardware interaction):
  * **LFO 1 / LFO 2 button**: TAP = next shape. HOLD 600 ms = the target starts
    cycling, one step every 400 ms, continuing from wherever it sits; release
    to lock. LED columns are indicators only.
  * **ENV button**: TAP = next filter mode ("waveform"). HOLD 600 ms = the
    drive×range combo cycles every 400 ms in the ring
    **up&hi → up&low → down&hi → down&low → …**, continuing from the current
    combo; release to lock. (Env TARGET keeps its own tappable LED column.)
* RATE / DEPTH / GAIN stay as physical pots.
* Hardware note: one momentary switch + LED array per section — a small MCU
  scanning buttons and driving LEDs is the clean implementation (to be decided
  in step 3).

---

## v0.12 + v0.13 — THE HARDWARE PLAN (this is what the PCB will be)

**Zero dropdowns.** Every selector is a button-cycled LED column (click the title
to cycle like the pedal's button, or click a row directly).

Control plate:

* **Knobs (12):** FREQ · GAIN · LPF · RES · MIX · VOL (big row) + LFO1 RATE/DEPTH
  + LFO2 RATE/DEPTH + ENV GAIN + (under the gate cover) THRESH/HOLD/FADE.
* **Dirt:** BAZZ FUSS, hardwired, always on (red LED). No selector.
* **LFO 1:** always **UNIPOLAR-UP** — SHAPE (6 LEDs) + TARGET (6 LEDs), amber
  rate LED pulses with the LFO.
* **LFO 2:** always **BIPOLAR** — SHAPE (6 LEDs) + TARGET (9 LEDs incl. LFO1
  Rate/Depth and Env Gain), amber rate LED.
* **Envelope follower:** GAIN knob + TARGET LEDs + DRIVE Up/Down + MODE
  Off/LP/BP/HP + RANGE Lo/Hi. Mode Off = filter bypassed, whole block greys out.
* **CV jacks (v0.13):** CV 1 (sidechain L) is **hardwired to LFO 1's DEPTH** as a
  VCA; CV 2 (sidechain R) to **LFO 2's DEPTH**. No target selectors, no strength
  knobs — sidechain level breathes the LFO's depth, the DEPTH knob sets the max.
  LFOs never grey out. Like a normalled jack: no signal for ~3 s = "unplugged"
  = VCA fully open, LFO runs at its knob. Green activity LEDs.
* **Output gate:** under a clickable cover plate showing the live summary and
  status LED (green open / amber blinking while fading / red closed).
  Defaults −48 dB · 1 s · 30 s.
* **Meters:** IN and OUT PPMs.

---

## v0.10 changes — fixed voicing filters + PCB dirt decision

**Jason's PCB dirt pick: Bazz Fuss** (the dropdown stays in the plugin for A/B-ing).

Three always-on voicing filters were added (no knobs, hardwired):

* Input, before everything: **24 dB/oct Butterworth low-cut @ 40 Hz** (two
  cascaded 2nd-order high-pass sections) — subsonic rumble never reaches the
  dirt or the 567.
* Just before the output: **12 dB/oct low-cut @ 60 Hz** (tames the 567's
  duty-cycle thumps) and a **+3 dB, Q 0.5 peaking bell @ 800 Hz** (mid presence).

Measured response of the chain: −62 dB @ 10 Hz, −3 dB @ 60 Hz, flat 100–300 Hz,
exactly +3.0 dB @ 800 Hz with broad shoulders (+1.9 dB at 400/1600), ~0 dB by 8 kHz.

Hardware cost: two op-amp Sallen-Key HP sections + one SK HP + one gyrator/
bridged-T bell — about one TL074's worth of the budget.

---

## v0.9 changes — always-on dirt in the dry path

A dirt stage now lives permanently in the DRY branch (the 567 side is untouched):
one **GAIN** knob (0 % = slightly dirty ×2, 50 % = OD/distortion ~×24, 100 % = wall
of fuzz ×300, log taper), a **DIRT dropdown** with five models, and **Gain is a
modulation target** for the LFOs, envelope follower, and CV buses.

Each model maps to a genuinely tiny real circuit (pick one for the PCB):

| Model | Hardware | Voice |
|-------|----------|-------|
| Electra Si | 1 Si transistor + 2 clipping diodes | immediate, crunchy, bright |
| Fuzz Face Ge | 2 Ge transistors (bias sag modeled) | warm, woolly, sputters when pushed |
| Bazz Fuss | 1 transistor + 1 diode | gated "velcro" rip, dies abruptly |
| Op-Amp OD | spare TL074 half + 2 feedback diodes | smooth, tight lows (250 Hz HP in) |
| Octave Fuzz | Green Ringer style rectifier | octave-up ring, chaotic with chords |

Notes: asymmetric models generate DC — blocked at 15 Hz after the clipper (a
coupling cap in hardware). The Octave Fuzz's rectifier is AC-coupled before the
clip, same as the Green Ringer's output cap. With MIX at 100 % FX the dirt is
inaudible (dry muted) — that's expected.

---

## v0.8 changes — the big simplification

**New signal path:**

```
In ──── trim (+15 dB, fixed) ── 567 demodulator ──┐
  │                                                ├── MIX ── Envelope Filter ── Gate ── Out
  └───────────────────── Dry ─────────────────────┘
```

1. Envelope-follower panel title cleaned up; **filter Mode gets an "Off" option**
   that bypasses the filter and greys out the whole envelope-filter block
   (LPF, RES, GAIN, DRIVE, RANGE, target).
2. **Filter ranges extended down**: Lo = 20 Hz … 4 kHz, Hi = 44 Hz … 8.8 kHz.
3. **Players 1, 2, 3 removed.** CV 1 = Sidechain Left, CV 2 = Sidechain Right.
4. **Dry>LPF removed, Input Trim knob removed** (trim is fixed at +15 dB and only
   feeds the 567 branch), and there is now **one single LPF/RES filter** placed
   AFTER the mix — the blended dry+FX signal goes through it together.
5. **Grey-out rules**: setting CV 1's target to anything but Off greys out and
   mutes LFO 1 (the CV takes over); same for CV 2 and LFO 2.
6. The raw (unfiltered) 567 square now hits the mixer directly — at MIX > 50 %
   with the filter Off you hear the pedal at its most feral.

Dropdowns lost "Dry>LPF"; routing selections reset from v0.7 sessions.

---

## v0.7 changes — Mu-Tron III envelope + filter

1. **The ADSR is gone; the envelope is now a Mu-Tron III style follower**: your
   picking dynamics drive the sweep continuously. Controls, like the real unit:
   **GAIN** (×0.125 … ×40 — how hard the envelope is driven; high gain pins the
   sweep like a real Mu-tron), **DRIVE Up/Down** (sweep direction — Down starts
   from wherever the LPF knob sits and pulls the filter shut as you dig in).
   Follower ballistics are fixed (≈4 ms attack / 150 ms release), as on the III.
   The target dropdown remains (default: LPF) so the follower can also drive
   Freq/Res/Mix — something the original never dreamed of.
2. **Filter ranges now match the Mu-Tron III**, selected by the new **RANGE**
   switch: **Lo = 40 Hz … 4 kHz**, **Hi = 88 Hz … 8.8 kHz** (derived from the
   III's 1.8 nF vs 1.8+2.2 nF integrator caps; Hi sits 2.21× above Lo). The LPF
   knob readout shows both ranges ("Lo / Hi").
3. **MODE switch: LP / BP / HP** — the III's filter modes, applied identically to
   the FX filter and the Dry>LPF path. BP is where a lot of classic quack lives.
4. LFO 2's "Env Amount" target is now **"Env Gain"** (bends the follower's gain
   ±2 octaves — auto-wah sensitivity that wobbles).

Instant Mu-Tron recipe: MIX 0 %, Dry>LPF 100 %, RANGE Lo, MODE BP or LP,
LPF knob low, RES ~70 %, GAIN to taste (start ×4), DRIVE Up.

---

## v0.6 changes

1. **DRY is now MIX**, a crossfade: 0 % = dry only, 50 % = dry AND 567 FX both at
   100 %, 100 % = FX only. First half of the knob raises the FX, second half fades
   the dry out. VOL remains the master level for the sum. The readout shows both
   levels (e.g. "D100 / FX60").
2. **"Dry" is renamed "Mix"** in every modulation target dropdown (same slot, so
   saved routings keep working).
3. Confirmed (no change needed): the **Dry>LPF filter always tracks the 567 FX
   LPF exactly** — same cutoff and resonance values, computed after LFO 1/2, ADSR
   and CV modulation are applied, in the same coefficient update.

---

## v0.5 changes (signal-flow fixes)

1. **Input Trim only feeds the 567 demodulator branch.** The DRY signal is tapped
   before the trim, so cranking trim to drive tracking no longer boosts (or clips)
   your clean sound. (This was also why Dry>LPF was hard to hear — the boosted,
   clipped dry drowned it out. Verified working: −43 dB on a 5 kHz tone with the
   LPF dark and Dry>LPF at 100 %.)
2. **OUT knob removed** — output level is fixed at 0 dB; the gate only attenuates.
3. **DRY now passes through VOL** — VOL acts as a master for both wet and dry.
4. **The gate also drags FREQ and the LPF down** while it fades the volume
   (sinking pitch + darkening as the sound dies).
5. **Gate threshold listens to the raw live input only** — before trim, players,
   or anything else touches it.
6. **FREQ/LPF snap back instantly** the moment the input crosses the threshold
   again (volume reopens over ~0.25 s to avoid clicks).

---

## v0.4 changes

1. **Vol and Input Trim removed** from every modulation target dropdown (the knobs
   themselves are unchanged).
2. **CV Slew knobs removed** — CV smoothing is fixed at 15 ms internally.
3. **CV 1 & 2 dropdowns slimmed**: they can now only target the sound knobs
   (Freq / LPF / Res / Dry / Dry>LPF), plus CV 2 keeps its "CV1 Strength" extra.
   All LFO/Env entries are gone from the CV lists.
4. **Noise LFO shapes**: LFO 1 and LFO 2 both gained White Noise and Pink Noise.
   For noise shapes the RATE knob becomes a low-pass filter on the noise —
   low rate = slow random drift, high rate = fizzy random jitter.
5. **LFO 2 has the full shape list** (sine / triangle / square / S&H / white / pink).
6. **Polarity switch on both LFOs**: Uni Up (0..+1), Bi (±1), Uni Down (0..−1).
7. **LFO 2 target dropdown**: everything LFO 1 can hit (Freq, LPF, Res, Dry, Dry>LPF)
   plus LFO1 Rate (the classic v0.2/0.3 wiring, still the default), LFO1 Depth, and
   Env Amount (the ADSR's intensity).
8. **OUTPUT GATE** (this kills the idle squeal): OUT is an independent master output
   level after VOL. When the input stays below THRESH (0 … −96 dB) for longer than
   HOLD (0.1 … 10 s), the output fades down to −96 dB over FADE (0.1 … 60 s). Play
   again and it reopens in a quarter second. THRESH at −96 = gate effectively off.

Note: because the routing dropdowns changed, mod-routing selections from v0.3
sessions reset to defaults (knobs, players, and everything else still load fine).

---

## v0.3 changes (Jason's wishlist pass)

* **FREQ** now spans **0.1 Hz … 18 kHz** (log). Below ~20 Hz the 567 chatter becomes
  slow clicky gating; way up high it turns into harsh digital shimmer. (The stock
  RT/CT network only did 304–1148 Hz — hardware in step 3 will need switched timing
  caps to cover this.)
* **FIZZ is renamed LPF**, now **200 Hz … 20 kHz** (log, up = brighter), and has a
  **RES** knob: filter Q from **0.25 … 8** (log). Q > ~4 rings hard on the 567 square
  edges — that's the fun part.
* **Dry>LPF** knob: blends the DRY path through a matched copy of the LPF (same
  cutoff + resonance). 0 % = classic untouched dry, 100 % = dry fully filtered.
* The **envelope is now a real input-triggered ADSR**: Attack/Decay/Sustain/Release
  sliders, a target dropdown (any knob, incl. LPF Q and Dry>LPF), and the bipolar
  Amount knob. Gate opens around −34 dBFS input and closes around −42 dBFS.
* **PPM meters**: input, output, and one per player (instant attack, ~40 dB/s fall).
* CV/LFO target lists gained **LPF Q** and **Dry>LPF** (appended — old sessions load fine).

---

# v0.2 baseline docs

Everything from the faithful v0.1 sim is unchanged — set all mod depths/strengths to 0 and
you have the stock pedal. The mods are layered on top.

## Modulation sources

### LFO stack → any knob
* **LFO 1**: Rate 0.05–20 Hz, Depth 0–1, Shape (Sine / Triangle / Square / Random S&H),
  Target dropdown (Off / Freq / Fizz / Dry / Vol / Input Trim). Default target: Freq.
  Depth 1 sweeps the target knob ±half its rotation around where the knob sits.
* **LFO 2**: Rate 0.02–10 Hz, Depth 0–1. Hard-wired to **LFO 1's rate** — it bends LFO 1
  up/down by up to ±2 octaves. This is the "LFO modulated by another LFO".

### Envelope follower → FREQ
* Follows how hard you play (live input + Player 1, before the circuit).
  Attack ~3 ms, release ~200 ms.
* **Env→Freq** knob is bipolar (−1…+1): positive = playing harder pushes the 567's lock
  frequency up, negative = down. This is the "frequency controlled by input velocity/volume".

### CV buses (sidechain + players)
* **CV 1** source = Sidechain In 1 **+** Player 2 (summed).
* **CV 2** source = Sidechain In 2 **+** Player 3 (summed).
* Each CV bus: audio → rectify → smooth (Slew knob, 1–1000 ms) → 0..1 control signal.
* Per bus: **Target dropdown** (Off / Freq / Fizz / Dry / Vol / Input Trim / LFO1 Rate /
  LFO1 Depth / LFO2 Rate / LFO2 Depth / Env Amount) + mini **Strength** knob (bipolar
  −1…+1) + mini **Slew** knob.
* **CV 2 only** gets two extra dropdown targets: **CV1 Strength** and **CV1 Slew** —
  CV 2 can modulate CV 1's own controls (modulation of modulation).
* CV always **adds to** the knob's position (knobs stay live); result is clamped to the
  knob's range. Modulated values update every 32 samples (~0.7 ms @ 48k).

### Sidechain wiring
One stereo sidechain bus: **Left = Sidechain In 1, Right = Sidechain In 2**. In your DAW,
route any track into the plugin's sidechain input. (In the Standalone app there's no
sidechain — use Players 2/3 as CV sources instead.)

## Audio file players

| Player | Route | Notes |
|--------|-------|-------|
| 1 | → circuit input, mixed with live audio | has a Level mini-knob (±dB) |
| 2 | → CV 1 source | never audible |
| 3 | → CV 2 source | never audible |

* Each player: **LOAD** (file picker: wav/aiff/flac/ogg/mp3), **PLAY** (press again to
  stop — the same button toggles), **LOOP** toggle (default: off for P1, on for P2/P3).
* Play always restarts from the beginning. Non-looping players stop themselves at the end
  of the file and the button resets.
* PLAY states are plugin parameters, so a DAW can automate them.
* Loaded file paths are saved with the plugin state and reload with your session.

## Evaluation order (so the routing is predictable)

1. CV 2 is computed first and applied (it may retune CV 1's strength/slew or any knob).
2. LFO 2 bends LFO 1's rate.
3. LFO 1 and the envelope follower apply to their targets.
4. CV 1 applies to its target.
5. Everything sums onto the knob positions, clamps to range, and feeds the circuit.

## New parameters (all DAW-automatable)

p1gain, playing1-3, loop1-3, lfo1rate, lfo1depth, lfo1shape, lfo1target, lfo2rate,
lfo2depth, envamt, cv1target, cv1strength, cv1slew, cv2target, cv2strength, cv2slew.

## Sim change from v0.1

The circuit's internal pot smoothing was tightened from 25 ms to 5 ms so LFO/CV wobble is
audible; the CV Slew knobs now own "how smooth" a modulation feels.
