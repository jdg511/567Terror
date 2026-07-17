# Glitchwave 567 — Circuit-to-DSP Mapping Notes

This document explains how each part of the schematic (Glitchwave567 rev 1.0, EasyEDA) is
modeled in `src/dsp/Glitchwave567.h`, and which behaviors are assumptions that Jason should
verify against the real pedal.

## Power / reference

* 9V single supply (VCC), C9 100u bulk filtering.
* R14/R15 (47k/47k) + C10 47u generate the +4.5V mid-rail bias (VREF).
* In the sim, every stage works in "volts around 4.5V". The op-amp rails limit each stage
  to roughly 1.4V ... 7.6V (TL074 can't reach its rails on 9V) — modeled as a soft clip at
  ±3.1V around VREF.

## Stage 1 — Input network + buffer (U1.1)

* R1 (1k) series, R2 (2.2M) to ground, C1 (220n) coupling, R3 (2.2M) bias to +4.5V.
* High-pass corner: 1/(2π·2.2M·220n) ≈ **0.33 Hz** → effectively just a DC blocker.
* U1.1 is a unity-gain buffer. The buffered clean signal is also the **DRY** signal tapped
  for the output mixer (assumption: the vertical net under the gain stage comes off the
  buffer output, not the gain-stage output — the R10:R9 = 100k:1M mixer ratio only makes
  sense if DRY is instrument-level and WET is the full-swing 567 square).

## Stage 2 — Gain stage (U1.3)

* Non-inverting amp: gain = 1 + R4/R5 = 1 + 470k/2.2k ≈ **×214.6 (+46.6 dB)**.
* C2 (4.7u) under R5 rolls the gain back to unity below fc = 1/(2π·2.2k·4.7u) ≈ **15.4 Hz**.
* With +46 dB of gain on 9V rails, any guitar signal slams into the rails → the LM567 sees
  what is essentially a **square wave at the input pitch**. This stage is the pedal's fuzz
  front-end and is why the 567 tracks at all.
* Modeled as: `y = x + 213.6 * onePoleHPF(x, 15.4 Hz)` followed by the op-amp rail clip.

## Stage 3 — LM567 tone decoder (U2) — the heart of the glitch

The LM567 is a PLL tone decoder. Timing network: RT = R6 (3.6k) + FREQ1 (B10k), CT = C4 (220n).

* Free-running VCO frequency: f0 = 1/(1.1·RT·CT) → **≈ 304 Hz (pot max) ... 1148 Hz (pot min)**.
* Pin 3 (IN) is driven through C3 (220n) into the chip's ~20k input impedance → HPF ≈ 36 Hz.
* **Key design choice in this schematic: there are no capacitors on LFIL (pin 2) or OFIL
  (pin 1).** In a normal tone-decoder application those caps set the loop bandwidth and
  stop the output from "chattering". With only stray capacitance there, the loop is
  hyper-fast and the output comparator chatters at audio rate — this chatter IS the pedal's
  voice. The Q output becomes a chaotic square wave made of sum/difference products between
  the input pitch and the VCO — ring-mod-like, stuttery, gloriously broken.
* Behavioral PLL model (runs at the oversampled rate):
  * input limiter → comparator with small hysteresis → `sIn` (±1)
  * VCO phase accumulator → `sVco` (±1) and quadrature `sVcoQ` (±1)
  * loop phase detector = `sIn XOR sVco` → stray-cap one-pole (~400 kHz, i.e. barely
    filtered) → pulls VCO frequency ±7% around f0 (datasheet max detection bandwidth 14%)
  * quadrature detector = `sIn XOR sVcoQ` → stray-cap one-pole → Schmitt comparator
    (thresholds `kDetOn`/`kDetOff`) → **Q output** (open collector, active low)
* **Q output node**: pulled up to 9V by R16 (100k), loaded by the Sallen-Key input
  (R7 + FIZZ_A + ...). Because the pull-up is weak, the "high" level depends on the FIZZ
  setting (≈5.2V ... 7.0V) while the "low" level is a hard ~0.15V — a lopsided square with
  a big DC component that shifts with duty cycle. C8 blocks it at the very end, so knob
  moves and lock/unlock transitions produce authentic thumps.
* Silence behavior: a tiny noise floor is injected at the 567 input so the free-running
  VCO bleeds through unpredictably at idle, like the real chip. Turn VOL down to kill it.

All the "magic constants" (stray caps, comparator thresholds, VCO pull range) live in one
`Tunables` struct at the top of the header so they can be tweaked while A/B-ing against the
real pedal.

## Stage 4 — Variable Sallen-Key LPF (U1.4) — "FIZZ"

* Unity-gain Sallen-Key: Ra = R7(10k)+FIZZ_A(0–50k), Rb = R8(10k)+FIZZ_B(0–50k),
  C7 = 3.9n (feedback), C6 = 1n (to VREF). FIZZ is a dual-ganged B50k.
* Cutoff: fc = 1/(2π·√(Ra·Rb·C7·C6)) → **≈ 8.1 kHz (pot at 0) ... 1.34 kHz (pot max)**.
* Q = ½·√(C7/C6) ≈ **0.99** → a pleasant slight resonance at the cutoff.
* This is what tames (or unleashes) the harsh edges of the Q square wave.
* Modeled as an RBJ biquad low-pass with smoothed coefficient updates + rail clip.

## Stage 5 — Output mixer (U1.2)

* Inverting summing amp around VREF, feedback R11 = 100k.
* WET: Sallen-Key out → VOL1 (A100k, audio taper) → R9 (1M) → node. Max wet gain = 100k/1M
  = **0.1** — needed because the wet square is ~4V peak vs. an instrument-level dry.
* DRY: buffer out → DRY1 (A100k) → R10 (100k) → node. Max dry gain = **1.0**.
* Audio taper modeled as a=(81^x−1)/80 (10% at half-rotation); wiper source impedance is
  included (it audibly interacts with R10 on the dry side).
* Output: C8 (220n) into R12 (100k) → 7.2 Hz DC-block, R13 (470Ω) series to the jack.
* Note: the mixer inverts polarity (both paths equally) — kept, as in the real pedal.

## Levels and oversampling

* Jack calibration: 1.0 full-scale sample ≙ 2.0V at the input jack (typical hot pickup
  peak). An **Input Trim** (±24 dB, sim-only, not in the real pedal) is provided since
  interface levels vary — set it so the sim "feels" like your rig.
* The whole circuit runs 4× oversampled (PLL edges and rail clipping alias otherwise).
  Some aliasing grit remains at extreme settings; arguably in character.

## Things to listen for when evaluating vs. the real unit

1. Tracking range: does lock happen over a similar FREQ-knob range for the same riff?
2. Chatter texture when slightly out of lock (this is where kDetOn/kDetOff matter).
3. Idle squeal/bleed level with strings muted.
4. Thump intensity when lock engages/disengages (stray-cap constants).
5. FIZZ sweep: brightness range and the resonant edge near cutoff.
