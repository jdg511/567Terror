# Glitchwave 567 — Schematic Review Report (datasheet pass)

Date: 2026-07-25. Method: per kicad-schematic-review methodology — structured
netlist extraction (machine-verified per sheet), then per-IC datasheet review
(4 parallel datasheet investigations), then fixes applied and regenerated.

## FIXED — high severity (would have been dead or wrong hardware)

1. **MP1584EN custom symbol had scrambled pin numbers** vs the real SOIC-8
   (datasheet: 1=SW 2=EN 3=COMP 4=FB 5=GND 6=FREQ 7=VIN 8=BST). Remapped.
   *Evidence: MPS MP1584 Rev 1.0 p.2/p.4.*
2. **MP1584 EN abs-max violation**: EN pulled to VA (up to 18 V) but EN abs
   max is 6 V. Added R114 24k9 EN→GND (datasheet Fig 4 app: 100k/24.9k, also
   gives ~6 V input UVLO). *p.2 abs max, p.15 Fig 4.*
3. **SVF integrators missing feedback dividers**: −IN was tied directly to
   the state node; tuning would land at ~611 kHz and state levels collapse.
   Added mirror 220k/680 dividers (R94-R97) per LM13700 Fig 29/32 topology;
   C61/C62 1n→220p so the sweep reaches ~8.6 kHz. *TI SNOSBW2F p.17-18.*
4. **LM13700 darlington buffer outputs had no DC load** (class-A follower
   can't sink): added R98/R99 10k SVF_BP/SVF_LP→GND. *EC table note 1, Fig 32.*
5. **LM567C supply abs-max**: 78L09 worst case 9.36-9.45 V > LM567C 9 V abs
   max. Added D105 1N4148W series drop (V567 ≈ 8.3 V). *TI SNOSBQ4 abs max.*
6. **+3 dB bell was electrically inert as drawn** (follower with feedback R
   doing nothing → actually a −3 dB shelf). Replaced with exact all-real-pole
   bell: G = 1+Zf/Zg, Zf = 3.6k∥56n, Zg = 4.3k+47n → +3.04 dB @ 790 Hz,
   Q = 0.500 (computed). No gyrator needed at Q=0.5.
7. **Pico ADC_VREF contention**: external 10Ω/1µF feed removed — the module
   already drives ADC_VREF through onboard 201Ω/2.2µF; paralleling rails
   creates circulating current. Pin now NC. *Pico datasheet §4.3.*

## FIXED — medium

8. FREQ_CV PWM filter: second RC pole added (R174/C107) — one pole leaves
   ~0.33 µA of 100 kHz ripple on Iabc which frequency-modulates the 567
   (pitch = most audible). Two poles ≈ −112 dB. Other CVs: one pole fine.
9. Envelope attack 4.7k→3.9k for exact 4 ms (Mu-Tron ballistics target).
10. LM567 pin 3: R53 1k series clamp-current limit (first-edge transient).
11. VA rail: D106 SMAJ18A TVS (CD4051 at VA=18 V has only 2 V abs-max margin).
12. MP1584 input cap → 10µ/50V X7R (DC-bias derating at 18 V).
13. Unused LM13700 buffer inputs grounded (datasheet parking condition).

## ACCEPTED AS-IS (documented, no change)

- **No LFIL/OFIL caps on the LM567** — the entire point of the pedal.
- **OTA outputs tied together** (U10A+B, U15A+B) — current summing, by design.
- **R38 100k RT floor** outside the 2k-20k documented range: only the
  OTA-off fallback frequency is unspecified; the OTA current dominates in
  normal use. Documented on the sheet.
- **CD4052B rON in the CT path**: adds ~3.6·rON/Reff timing error (≤~12%
  at max OTA current, ~300Ω typ rON). Mitigation: firmware calibration per
  cap range; error is static and calibratable. Noted on sheet. A low-rON
  mux (DG409-class) is the hardware alternative if calibration disappoints.
- **LM13700 at VA=9 V** is 0.5 V below the 9.5 V recommended-operating
  minimum (abs max is fine). Classic 9 V-pedal usage; headroom shrinks.
  Works; noted.
- **MP1584 COMP 51k/3.3n** is conservative-stable (datasheet-optimal for
  44 µF out would be ~200k/330p — faster transients). Kept: audio load is
  benign. Marked "NOT RECOMMENDED FOR NEW DESIGNS" by MPS (MP2338 successor)
  — fine for a one-off; note for future reorders.
- **Wet-vs-dry VCA max gains** (1.22× vs 2.60×) differ from the original
  1M/100k mixer ratio — intentional; firmware CV law reproduces the sim's
  exact mix behavior (it has more resolution than the analog ratio did).
- Control-feedthrough thump from LM13700 VOS (up to gm·VOS·Rf): mitigated
  by AC coupling and slow CV slews; add offset trims only if audible.

## Verification basis

- Per-sheet netlists machine-verified against declared pin→net tables
  (generator EXPECT diff = zero on all 8+1 sheets, before and after fixes).
- Full-project netlist stitch re-verified after fixes.
- Datasheets consulted: MPS MP1584 Rev 1.0; TI LM13700 SNOSBW2F; TI LM567
  SNOSBQ4E/F + Philips NE567; TI CD4051B/52B; Raspberry Pi Pico + RP2040.
- Filter math verified by direct computation (SK-HP pairs, bell, ballistics,
  PWM ripple) — SK values confirmed best-E24; bell redesigned.
- NOT yet verified: footprint-level pin mapping (next phase), PCB current
  paths (layout phase), DC-044A axis height (3D model), Suntsu stomp feel.
