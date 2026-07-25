# Glitchwave 567 — First-Article Bring-Up & Test Plan (rev 0.1, 2026-07-25)

For the first assembled MAIN + CONTROL board pair back from PCBWay turnkey.
Work on the bench, boards OUT of the enclosure, on a current-limited supply.
Tools: DMM, bench supply (9–18 V, set current limit 150 mA at first), audio
probe (amp + capacitor-tipped probe lead), guitar or signal generator,
a USB cable for the Pico, and the plugin (v0.32) for A/B reference.

The BOM.xlsx README sheet has the pre-power checklist; it is repeated and
expanded here. Check boxes in order — do not skip ahead to audio before the
rails pass.

## Stage 0 — Visual inspection (unpowered)

- [ ] Both boards: no solder bridges on the TSSOP (U21 4067) and SOIC parts;
      reflow quality on the Pico's castellated edge pads (U20).
- [ ] Electrolytic polarity: bulk caps near the DC jack, C47/C48 timing
      caps (22u/470u) at the CD4052, Pico VSYS 10u (C90).
- [ ] DNP check: the LM567 LFIL/OFIL pads (pins 1/2 region, marked DNP)
      must be **empty** — if PCBWay fitted caps there, remove them; they
      would "fix" the chatter that is the whole point of this pedal.
- [ ] Stomp switches seated square on the CONTROL board; 2×8 header keyed
      correctly, boards mate without force.
- [ ] DC-044A polarity with DMM continuity: **center-negative** — barrel
      tip pin → GND net, sleeve → VIN_RAW.
- [ ] PJ-603A jacks: tip vs tip-switch contacts identified (DMM).
- [ ] With DMM in resistance mode: VIN_RAW→GND not a short (>1 k after
      cap charge), VA→GND >1 k, +5V→GND >100 R, 3V3→GND >100 R.

## Stage 1 — Rails (MAIN board alone, no CONTROL board, no jacks plugged)

Supply at **9.0 V**, current limit 150 mA. Expected draw: 40–90 mA.

| Testpoint (probe at part) | Expect @ 9 V in | Expect @ 18 V in |
|---|---|---|
| VPROT (after AO3401A P-FET) | ≈ input − 0.02 V | same |
| VA (analog rail) | ≈ 8.9 V | ≈ 17.9 V |
| V567 (78L09 → 1N4148W D105) | **≈ 8.3 V** | ≈ 8.3 V |
| +5V (MP1584 buck) | 5.0 ± 0.15 V | 5.0 ± 0.15 V |
| VREF (buffered mid-rail) | **VA/2** ≈ 4.45 V | ≈ 8.95 V |
| 3V3 (Pico onboard reg, at C91) | 3.3 V | 3.3 V |
| PICO_VSYS (after D90) | ≈ 4.7 V | ≈ 4.7 V |

- [ ] Reverse-polarity test (optional but recommended once): swap supply
      leads at 9 V with 100 mA limit — draw must be ≈ 0 (P-FET blocks).
- [ ] Repeat the rail table at 18 V. Nothing should heat beyond warm
      (78L09 dissipates the most; it only feeds the LM567).

## Stage 2 — Pico enumeration & fw-0.1

- [ ] Hold BOOTSEL, plug USB: RPI-RP2 mass-storage drive appears (works
      even with the pedal unpowered — USB feeds VSYS through the Pico).
- [ ] Drag fw-0.1 UF2. Open the USB serial console.
- [ ] Console `scan` command: all 12 mux channels + CV1/CV2 print.
      Grounded spares C12–C15 must read ≈ 0 (< 20 LSB). VA_SENSE must
      track the bench supply. Trims RV1–3 respond.
- [ ] Attach CONTROL board: 6 pots sweep 0→4095 full-range; both stomps
      read active-low; WS2812 chain runs the boot rainbow in order
      A, B, C, tempo, bypass, gate (chain order = fault isolation: if
      LED3 is dark but 4–6 light, the fault is LED3 itself).

## Stage 3 — Audio path trace (audio probe, follow the block diagram)

Guitar (or 100 mV / 200 Hz generator) into IN. Probe in order; each point
must be louder/dirtier per the block diagram. With no firmware CVs yet,
run fw-0.1's `cv` console command to force sensible defaults
(mix 50/50, gate open, bypass=effect, dirt mid, SVF LP mid).

1. [ ] Input buffer out — clean unity signal.
2. [ ] HP40 out (2× Sallen-Key) — clean, thin bass (40 Hz 24 dB/oct).
3. [ ] Dirt branch: Bazz Fuss collector — gated velcro fuzz; STARVE CV
       sweep audibly sags it.
4. [ ] 567 branch: +15 dB trim out — hot clean; LM567 input (pin 3) —
       square-ish.
5. [ ] **LM567 Q output (pin 8): the chatter.** Play single notes and
       sweep FREQ — lock/unlock stutter, ring-mod sidebands. This is the
       money test; see Stage 4 for the A/B.
6. [ ] SVF outputs: LP/BP/HP/Notch each selectable via `fmode` console
       command; cutoff sweeps with FIZZ CV; Q self-oscillates near max.
7. [ ] Mix VCA out — crossfades dirt vs 567 with MIX.
8. [ ] Voicing chain: HP60 → bell (+3 dB @ 790 Hz) → +6 dB DIP stage →
       JFET J201 stage → diode ladder — each point a bit more shaped;
       DIP switches audibly add/remove their stages.
9. [ ] Gate/VOL VCA → output buffer → OUT jack: VOL pot obeys the audio
       taper (≈10% level at half rotation); gate closes to silence when
       muting strings (tune RV1–3 by ear).

## Stage 4 — The 567 chatter A/B vs plugin v0.32

Same riff through both (plugin fed by interface, pedal by amp or DI):

- [ ] Tracking range: lock occurs over a comparable FREQ-knob span.
- [ ] Chatter texture just out of lock (the sim's kDetOn/kDetOff feel).
- [ ] Idle squeal/bleed with strings muted — VOL down must kill it.
- [ ] Lock/unlock thump intensity.
- [ ] FIZZ/SVF sweep character near cutoff.

Differences are *tuning inputs*, not failures: adjust firmware tunables
(and if the analog side needs it, record values for a rev 0.2 BOM tweak).

## Stage 5 — Controls & gestures

- [ ] STOMP1 short press: bypass crossfade — silent, ~10 ms, no thump.
- [ ] STOMP2 taps: tempo LED follows; LFO1 rate audibly tracks.
- [ ] STOMP2 hold: SVF mode cycles, section LEDs annunciate.
- [ ] Both stomps held: STARVE gesture sags, releases clean.
- [ ] CV1/CV2 in: patch a phone/function-gen — modulation follows;
      unplugged jacks are silent (switch contacts ground the tips).
- [ ] CV OUT: 0–5 V LFO on a scope/DMM; source select via console.

## Stage 6 — Enclosure fit (after electrical pass)

- [ ] Dry-fit the stack in the drilled 1590XX (drill_template_1to1.pdf);
      pots centre in Ø7 holes, stomp bushings clamp the CONTROL board,
      wall jack nuts reach (PJ-603A axis 5.0 mm, DC ≈3.65 mm, CV 4.5 mm
      above MAIN board top — confirm DC-044A axis on the real part, it
      was the one unverified datasheet number).
- [ ] Lid closes; nothing shorts against the aluminium (check tall parts:
      470u can C48, L100, Pico USB connector clearance to back plate).
- [ ] Full play test in the box, wall power, pedalboard chain.

## Sign-off

Record in this file: serial #1 rail voltages, current draw at 9/18 V,
which tunables changed vs defaults, and any rev 0.2 notes. Then order
the remaining enclosures drilled.
