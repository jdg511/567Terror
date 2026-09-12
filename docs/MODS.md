# Glitchwave 567 -- Step 2 Mods (v0.2 .. v0.43)

## v0.43 -- dirt GAIN range is now x0.01 .. x100, with unity at the knob's centre

The Layer Y GAIN knob (FREQ with TAP held) was x1.1 .. x300. It is now
**x0.01 .. x100**, four decades, log as before.

* **Unity lands dead centre.** `0.01 * 10000^0.5 = 1.0` exactly, so knob at
  12 o'clock is x1.00 and you can read the whole bottom half as attenuation
  and the whole top half as drive. That is not an accident of the numbers --
  four decades is the only span that makes 0.01 and 100 symmetric about 1.
* **The bottom half is genuine cut**, down to -40 dB. This is the knob you
  want for the rev 7 open item: with SW1 in, the natural-gain JFET slams the
  Bazz Fuss and GAIN minimum used to still be fuzz. Now you can back it off
  and find, by ear, roughly how much pad belongs between the JFET and the
  fuss before you solder one in.
* **The top drops from x300 to x100.** The fuss is far past its clip point by
  x100, so the wall is still a wall; you lose about 9.5 dB of headroom above
  it that was only ever pushing an already-saturated stage harder.
* **The default moves from 0.0 to 0.5** so the shipped sound does not change.
  0.0 used to mean x1.1, near unity; under the new range 0.0 means x0.01, a
  40 dB cut, which would have made the dry path nearly silent out of the box.
  0.5 is x1.00.
* Readout precision follows the range: 3 decimals below x0.1, 2 decimals to
  x10, whole numbers above.

**Existing presets and saved sessions will read differently.** The parameter
is stored normalised 0..1, so a preset saved at 0.0 used to sound like x1.1
and will now sound like x0.01. Anything you want to keep should be re-saved.

The mod system is unaffected: it modulates the normalised value, so LFO and
envelope routing to GAIN just follows the new curve.


## v0.42 -- Layer A drops the envelope-follower Shape control off the GAIN knob

On the secret Layer A (both stomps held), the ENVELOPE GAIN knob used to
reach `envshape`. That mapping is gone. Layer A is now three live knobs:

* MIX -> STARVE (the red "?")
* FREQ -> env THRESHOLD
* LPF -> env RATIO

GAIN on Layer A is dead, the same as Rate 1 and Rate 2 already were, and its
value line stays dashed instead of showing the shape number. Releasing the
stomps puts the knob back on `envgain` as usual, so nothing about normal
playing changes.

The `envshape` parameter itself is kept: it still runs in the DSP and is
still automatable from a host, it just no longer has a knob on the pedal, so
it sits at whatever it was last set to (default unless a preset moved it).
Say the word if you want it deleted outright and pinned to its default.


## v0.41 -- hardware rev 7 mirrored into the plugin: ladder and +6 dB deleted, one supply voltage, JFET moved in front of the fuss, LM567 on its own 7.5 V rail

The sim now matches the rev 7 board. Four changes, and the last one is the
one that will surprise you when you hear it.

* **The -3/-6 asymmetric ladder is gone**, in the sim and on the board. The
  output stage is now just the fixed voicing into the op-amp rail, and the
  rail is still the thing STARVE collapses. `ladderDb`, `ladderLo`,
  `halfClip`, `ladderClip` and `asymClip` are deleted from the DSP, not
  merely switched off.
* **The +6 dB output boost is gone** for the same reason. The `boost6`
  parameter and its smoothing are removed.
* **One adapter voltage: 9 V.** The 12/15/18 V options are off the hardware,
  so `supply4` and the SIM SUPPLY selector are deleted. STARVE still sags
  that single rail linearly to the 1 V floor. In rev 7 that sagging rail is
  VDIRT, which is why the JFET and the Bazz Fuss die on it together.
* **The JFET stage MOVED rather than being deleted.** SW1 now puts the J201
  Fetzer Valve in FRONT of the Bazz Fuss (stage 3b) instead of after the
  mixer, with its drain on VDIRT so STARVE reaches it. It also runs at its
  REAL voltage gain now instead of unity: `tune.jfetGain` = 6, the middle of
  a J201 Fetzer's natural x4..x10 spread, with the gain AND the usable input
  swing both scaling with the drain rail (a starved JFET loses gm and
  headroom at the same time, so it does not just get quieter, it stops
  curving). Consequence, and it is the real one, not a modelling artifact:
  a natural-gain JFET turns a 100-300 mV guitar into 0.4-3 V, and the fuss
  clips at a few hundred mV, so **with SW1 in, GAIN at minimum is already
  full fuzz**. The hardware fix is a fixed pad BETWEEN the JFET and the
  fuss, never before the JFET (attenuating first just starves it of the
  level it needs to curve). That pad is still open, to be sized on the
  breadboard, so the sim runs it unpadded too.
* **V567 is its own rail, and it is 7.5 V, not 8.7 V.** Rev 7 drops it out
  of VA with D105 + D107, two 1N4148W in series. Why the target moved: TI's
  recommended operating maximum for the LM567C is 8.5 V and absolute max is
  9.0 V, so 8.7 V was outside the datasheet with 0.3 V to destruction. And
  no resistive divider can hold any target here, because the chip draws
  7-10 mA idle and 12-15 mA activated -- a 56R/1.6k divider would have moved
  the rail ~430 mV in step with the chatter, modulating the timing network
  and the Q-node swing with the audio. Two diodes are about six times
  stiffer over that swing (dynamic resistance at 12 mA is only ~2.2 ohm) and
  the 100u at the pin mops up the rest. The BZX84C8V2 across the shunt leg
  becomes a genuine wrong-adapter fault clamp that never conducts in normal
  use -- at 8.7 V its 7.79-8.61 V tolerance band would have had it
  conducting continuously at the nominal operating point.
  The cost lands on the wet path and is modelled: R16 pulls the Q node up to
  V567, so its high level drops from about +3.75 V to +2.50 V relative to
  the 4.5 V mixer reference, roughly 15% of the total Q swing. The MIX law
  absorbs it. If f0 drift ever matters more than wet level, TI characterises
  stability at 4.75-5.75 V, so 5 V is the real sweet spot.

Under the cover: PCB SWITCHES is down to one row, "SW1 JFET PRE-FUSS", and
where SIM SUPPLY used to be there is now a read-only LM567 RAIL readout
(7.5 V, VA - D105 - D107) -- read-only by design, since two diodes set it,
not a trimmer. The closed strip reads `JFET / C41 / C42 / V567 / HINTS`.
Defaults are unchanged: everything OFF/OUT, HINTS ON.

## v0.40 -- demo player strip: 27 embedded clips, looping transport, -24..+12 dB level

A 140 px strip below the pedal face, deliberately outside the cover's dim
veil so it keeps playing while the cover is open -- the whole point is
A/B-ing the under-the-cover switches against real playing.

* Dropdown of 27 Ogg clips baked in with `juce_add_binary_data` (Ogg decodes
  with `registerBasicFormats()`, no extra flags, unlike MP3). The list is
  grouped into submenus by the text before the dash.
* START becomes STOP; the clip loops until stopped. Clip choice and level
  save with the preset, the transport does not.
* DEMO LEVEL knob, -24 to +12 dB. The processor sums the clip into the
  pedal's input, so it runs through the entire circuit.

## v0.39 -- Starve corrected to a realistic 9-18V/100mA linear supply, Freq/Gain swapped on the secret envelope controls, hint captions added

Follow-up corrections to v0.38, all on Layer A (both stomps held):

* **Starve supply spec fixed**: the modeled supply is now a 9-18 V / 100 mA
  wall-wart or 9 V battery -- a genuinely realistic rating for a single
  small stompbox (the v0.38 "2.4 A" figure was way oversized for a pedal
  like this). The current-limit foldback curve from v0.38 is gone too:
  per spec this is now a plain LINEAR sag, a straight line from whatever
  supply voltage is selected (9/12/15/18 V) all the way down to the 1 V
  floor, regardless of which supply voltage is chosen. Same formula shape
  as the original pre-v0.38 code, just with the floor moved from 5 V to
  1 V. Readout under Mix stays a bare number, no `V`.
* **Threshold and Shape swapped knobs**: Threshold moves to the Freq knob;
  Shape moves to the Gain knob (Freq's old job). Ratio stays on LPF,
  Starve stays on Mix. Wasn't specified which knob absorbs the one
  Threshold vacated, so it's treated as a straight swap between Freq and
  Gain -- flag it if that's not what you had in mind.
* **Hint captions for Layer A**: with Hints ON (the HINTS: ON/OFF toggle),
  Layer A's row now shows three short, still-cryptic titles instead of
  dashes: Freq = `ET?`, LPF = `ER?`, Mix = `SV?`. Gain stays dark (no
  caption requested for it). With Hints OFF, the row goes back to the
  fully secret dashes/`?` from before -- unchanged.

## v0.38 -- realistic 1V/2.4A supply starve, secret envelope-follower Ratio/Shape/Threshold, KNOB LAYERS chart drops the A row

Three separate asks bundled into one pass, all living on the already-secret
Layer A (both stomps held):

* **KNOB LAYERS chart**: the "A -- BOTH" row is gone from the on-screen
  legend entirely (it only ever showed dashes and a "?" anyway). The chart
  is now X/Y/Z only. Layer A itself still exists and still works exactly
  the same way (hold both stomps) -- it is just no longer documented on the
  face, same as Starve always was.
* **Starve, now SUPER realistic**: the secret Starve knob (Mix, in Layer A)
  used to sag the rail in a straight line from the supply voltage down to a
  5 V floor. It now models an actual 2.4 A-rated wall-wart/battery: nearly
  flat voltage for most of the knob's travel (a real supply barely sags
  until it's close to its rated current), then a hard current-limit
  foldback in the last stretch, diving all the way to a 1 V floor. That is
  well past the point any real op-amp or JFET stage in this circuit would
  already be dead and silent -- we let it go there anyway so you hear the
  whole death spiral, not just the "still technically alive" part. The
  live readout under the Mix knob now shows the bare effective-rail number
  (e.g. `9.0` down to `1.0`), no `V` suffix.
* **Envelope follower Ratio** (LPF knob, Layer A): the follower used to be
  a flat 1:1 relationship between input level and its output. LPF now
  sweeps a compressor-style ratio continuously from `0.1:1` (expand) at
  0%, through `1:1` (unity) at 50%, to `1:10` (heavy compression) at 100%,
  applied above the new Threshold. Readout is just the ratio itself
  (`0.10:1` .. `1:1` .. `1:10`), no other label.
* **Envelope follower Shape** (Freq knob, Layer A): a separate knee-curve
  control, independent of Ratio -- log (concave) at 0%, straight line at
  50%, exponential (convex) at 100%. There's no standard named unit for a
  curve-shape blend the way there is for speed or mass, so it's just a
  bare gamma-exponent number (0.25 .. 1 .. 4), no suffix, no label.
* **Envelope follower Threshold** (Gain knob, Layer A): input below this
  level now reads as flat zero out of the follower; everything above it is
  rescaled 0..1 before Ratio/Shape are applied. Bare 0.00..1.00 number,
  no label.
* All three envelope controls default to "no change" (Threshold 0, Ratio
  and Shape centred at 0.5) so a stock patch sounds identical to before
  this pass. DSP change is entirely inside `ModSystem::compute()`
  (`envAboveThresh` / `envShapeExp` / `envRatioExp` / `envShaped`, used in
  place of the raw `inEnv` everywhere the follower feeds its target).

Would either of the curve controls be easy to actually build in analog
hardware? Threshold: trivial, just an offset/bias into the rectifier stage
before the VCA control, extremely common (every noise gate and most
compressors have exactly this). Ratio: doable but a real component --
you'd want a proper log/antilog VCA core (THAT2180-series or SSM2164, or a
discrete diode log-antilog pair) with the ratio pot tapping into the
control path, the same basic technique a dbx/1176-style compressor uses
for its ratio switch, just applied to the envelope-follower's control
voltage instead of the audio path. Shape (log/linear/exponential morph)
is the hard one in pure analog -- a continuous three-way curve morph
needs a crossfade between differently-shaped CV generators, which gets
big and fiddly with discrete parts fast. In practice, the boutique-pedal
way to get exactly this control today is a small microcontroller shaping
the VCA's control voltage (DAC out) while the audio path itself stays
100% analog -- keeps the sound analog, makes the curve-morph a couple of
lines of firmware instead of a rat's nest of op-amps.

## v0.37 -- INS/DEL keyboard emulation removed, right-click-only hold + in-plugin SETTINGS

Root cause of the "GUI stutters while holding INS/DEL, only inside Fender
Studio Pro, only the GUI, audio never glitches" report: Fender's own key
command sheet binds Insert to "Insert Marker" and Delete to "Delete" as
host-level shortcuts. Every OS auto-repeat keydown for those two keys was
falling straight through the (unfocused, unclaimed) plugin editor and
reaching the host, which was doing real per-repeat work on its own
timeline/marker redraw, stealing frames from the shared UI thread. There
is no host-guaranteed way to reserve a key from a DAW (VST3's onKeyDown
"handled" return is best-effort, not a contract), so the fix is to stop
using the keyboard at all:

* `tapStompDown()` / `bypassStompDown()` no longer poll
  `KeyPress::isKeyCurrentlyDown()`. TAP = Y and BYPASS = Z can only be held
  by pressing and holding the stomp with the mouse, or right-click to latch
  it held (v0.28, unchanged and now the only hold path). BOTH held = A,
  same as always.
* New one-time onboarding callout (`HoldHintOverlay` in PluginEditor.h):
  big bold off-white "RIGHT-CLICK TO HOLD" text with two arrows pointing at
  the TAP/BYPASS stomps. Starts full-size (60 px) centred on the face, holds
  still 2 s, travels down and shrinks onto the stomps over 5 s (7 s total),
  then fades out over 5 s (12 s total). Clicking anywhere in the plugin at
  any point cuts the (remaining) fade to 1 s from whatever opacity it is
  currently at; if it was still travelling when clicked, it keeps
  travelling for that 1 s rather than freezing in place. On top of
  everything else in the editor; does not intercept clicks.
* New SETTINGS button on the pedal face itself (footswitch strip, between
  the stomps' hint text and the small logo), opening the exact same Scale
  and Feedback window the standalone's title-bar Options menu always had.
  Fixes the actual complaint: that window was only ever reachable from
  `StandaloneApp.cpp`'s custom title bar, so it silently did not exist for
  VST3/plugin instances, there was nowhere in a DAW to reach it at all.
* Every on-screen and in-code mention of INS/DEL as the layer-hold keys
  (the KNOB LAYERS chart, the v0.30 control-scheme comments, the footswitch
  hint text) is updated to match; nothing else about the X/Y/Z/A layer
  behaviour, tap tempo, gate, or DSP changed.

## v0.36 — the plugin becomes "Where The Fuzz Meets The Funk"

Pure metadata/naming, zero behaviour change:
* PRODUCT_NAME (and every bundle/file name): "Glitchwave 567" →
  **"Where The Fuzz Meets The Funk"** — the plugin is now named after its own
  tagline. Vendor stays **Illicit Apothecary** and the VST3 category stays
  **Fx | Filter** (both in place since v0.34).
* PLUGIN_CODE / MANUFACTURER_CODE unchanged, so existing DAW sessions still
  find the renamed plugin. LV2 URI unchanged.
* Scale-and-Feedback window title + saved-feedback files follow the new name.
* Old "Glitchwave 567" VST3/LV2 installs are removed at install time so DAWs
  don't list the pedal twice. Standalone settings migrate to the new app name
  (audio device re-pick once).

## v0.35 — x1 scale + Scale and Feedback window + the glitch pantheon (graphics only)

Zero functional changes again: params, layers, tap tempo, gate and DSP are
v0.32 behaviour, face is v0.34 Terror. Everything here is UI.

### Scale (Jason: v0.34's 2.5x was "insanely HUGE")
* The editor opens at **x1 (1060x640) every launch** — plugin and standalone.
  uiScale lives on the processor (not a parameter) and the editor applies
  changes live.
* New standalone Options-menu item **"Scale and Feedback..."** (between
  Audio/MIDI Settings and the separator). Built with a custom standalone app:
  JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1 on the Standalone target only +
  src/StandaloneApp.cpp (subclassed window hides the stock Options button and
  shows the extended menu; stock items forward to handleMenuResult).
* The window (src/ScaleFeedback.h): **x0.5 / x1 / x1.5 / x2** boxes (apply
  instantly), the version number, and an optional feedback form — Name*,
  Email*, Address, State, Zip*, Country, Phone #, a 10-line feedback box,
  a 3-way notify choice (real-world pedal on sale / FREE plugins + real-world
  products only / nothing at all — default: nothing), and a note that we would
  never even consider selling anyone's info. SAVE writes a text file to
  Documents/Illicit Apothecary/ locally; nothing is ever transmitted.

### Face
* The brand/version line under the tagline is gone (version now lives in the
  Scale and Feedback window); "Where the Fuzz Meets the Funk" grew to 40 px.

### The glitch pantheon (all visual only — the audio path never sees them)
* **Major Glitch — every 33:33, 1.3 s.** Full replacement of the old veil
  glitch (hated), built from Jason's four reference images/videos: datamosh
  row-ripple with corrupted macroblocks → rainbow pixel-sort melt (the face
  drips downward in hue-cycling streaks) → vertical comb tear with saturated
  colour bands and diagonal black rips → liquid psychedelic hue-wash → snap
  back. Snapshot-based; every strobe frame mutates.
* **Med Glitch — every 11:11, 325 ms.** A two-act half-length cut from the
  same reference art (comb strips with sparse rainbow drips → liquid ripple
  with hue washes) played at double speed.
* **Smear — every 6:06.006, 550 ms.** Quick band-tear to the right.
* **Color bars — every 3:33, 360 ms.** Flickering cyan/magenta + green/yellow
  dash bars.
* **Title jiggle — every ~1:11 ± 12 s, speed ± 13 % per event.** The name
  splits into four slices ripping in different directions (one always tears
  the opposite way), RGB separation jumps per slice, smear trails drag behind
  it, and dropout slivers + stray confetti cut through the text. Every
  occurrence rolls differently.
* The big smear (v0.35-dev interim) was auditioned and deleted. A temporary
  on-face trigger-button test panel was used during the audition and removed
  for ship; GlitchFx keeps its trigger methods for future test builds.

## v0.34 — "Terror" glitch-art face (Jason's Claude Design restyle; graphics only)

**v0.33 was the design project's intermediate polish pass — the shipped jump is
v0.32 → v0.34 so the plugin title matches the design file ("v0.34 Terror").**

Zero functional changes: every parameter, the X/Y/Z/A layer machine, tap tempo,
latches, gate and DSP are byte-for-byte v0.32 behaviour. The +6 dB internal
switch STAYS — Jason: "have it as the actual pcb has it", and the PCB carries
that stage on an internal DIP switch (ships ON). (The design bundle's notes had
proposed removing it; overruled.)

### The new face (source: docs/ui/, from Jason's Claude Design project)
* Window 1060×640 logical at **setScaleFactor(2.5)** → 2650×1600 on screen
  (25 % bigger type than v0.32, per the design).
* "Illicit Apothecary" branding: baked-invert logo with chromatic aberration,
  mosaic-static photographic face (docs/ui/assets → downscaled + veil/scanlines
  pre-baked into assets/bg_face.jpg), "Where the Fuzz Meets the Funk" tagline
  with CRT tear, rainbow hairline, Barlow Semi Condensed + IBM Plex Mono
  embedded via juce_add_binary_data (OFL).
* Layer chips X/Y/Z/A in the header (white/cyan/yellow/red), KNOB LAYERS chart
  printed on the pedal face, IN/OUT PPMs restyled.
* Per-knob live value line (uses the real parameter text), knob rings in
  section colours (yellow / cyan / magenta / green) with the design's 216°–504°
  sweep, selector knobs unchanged (place-zone parking from v0.31).
* NeoPixel **swatch rulers** show every SHAPE / TARGET / MODE / DRV-RNG
  selection persistently (display only; knobs still do the choosing), with
  two-tone name labels ("Sine / Wobble").
* Starve goes fully secret: A-layer caption is a red **"?"** and the readout is
  the sagging rail voltage (supply → 5 V floor, real DSP formula).
* Cover redesigned: full-width INTERNAL strip (live summary line + gate LED +
  OPEN COVER) → opens a green-bordered panel over the LFO row with gate
  trimmers, PCB SWITCHES rows (JFET / −3/−6 LADDER / **+6 dB BOOST**), SIM
  SUPPLY 9/12/15/18 V, and a HINTS toggle (all hint text ships hidden).
* CV jack panels dropped from the face (the design removed them; the hardwired
  sidechain→depth VCAs still function — they're just not drawn).
* Timed glitch decoration: 7 s CRT scan sweep, colour-bar burst every 33 s,
  full-face "major glitch" every 11 min, tagline tear every 5.5 s.

### Plumbing
* CMake: VERSION 0.34.0, COMPANY_NAME "Illicit Apothecary" (was JasonDIY —
  standalone audio settings re-select once), VST3_CATEGORIES Fx **Filter**,
  new GwAssets binary-data target (art + 5 fonts, ~1.3 MB).
* docs/ui/ carries the design source: three .dc.html screens, support.js,
  original logo + mosaic assets, github.md (with the +6 dB resolution note).

## v0.32 — THE PEDAL TAKES ITS FINAL SHAPE (clip decided, ship defaults)

**Output stage decided** (audition over): two internal switches — **JFET
on/off (ships ON)** feeding a **−3/−6 asymmetric ladder on/off (ships
OFF)**. Both off = the bare op-amp rail. CLIP button removed.

**Under the cover** (renamed "Internal Pedal Trim Pots / Switches /
Simulated input voltage", gate LED stays on top): gate trim pots
(thresh/hold/fade), JFET switch, −3/−6 ladder switch, +6 dB switch, and the
supply selector — now **9 V / 12 V / 15 V / 18 V**.

**Ship defaults** (every fresh load; the standalone powers up on these
every time like a real pedal — DAW sessions still recall their state):
FREQ 0.5 Hz, LPF 200 Hz, Q 4, Mix D100/FX25, Vol 90 %, LFO1 0.5 Hz / 50 % /
Sine / →Freq, LFO2 0.2 Hz / 15 % / Sine / →LFO1 Rate, Env ×10 / →LPF /
LP / Up-Hi, Starve 0, JFET ON, ladder OFF, +6 dB ON, 9 V.

**FREQ range is now 0.2 Hz – 6 kHz** (was 0.1 Hz – 18 kHz) — display and
VCO mapping both.

**UI 2×**: whole panel renders at double size (all text doubled). INS/DEL
readout removed (controls signed off). **Tempo LED** added next to TAP:
blinks at the tapped LFO1 rate, flashes bright on every press. Tap tempo
commits on the **3rd press** now (rolling average of the last 3).

**Build formats**: VST3 + LV2 + Standalone on Windows & Linux; AU/AUv3
included in the build config for anyone compiling on macOS.

---



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
