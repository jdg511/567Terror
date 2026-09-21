# Rev 8 hardware change list

What the board and BOM need in order to match the plugin as of v0.51.
**APPLIED 2026-09-21 - schematic, PCB, BOM and fab package all updated. See
"DONE (2026-09-21)" at the bottom for what was actually done, including two
corrections to the assumptions in this file.**

Backup of the files this would touch:
`hardware/_backups/prev051-20260916-214347/`

---

## DECIDED (2026-09-21)

- **Enclosure stays the Hammond 1590XX.** No change. The 1550E (171 x 121 x
  55) and the 1590D/DD (188 mm) were costed out and rejected: the 1590D and
  DD cap PCB width at ~110 mm against our 114 mm board, the DD is also
  2.3 mm shallower than the XX and the 470uF can only has 0.80 mm of
  clearance as it is, and the 1550E is an industrial box with no pedal-shop
  powder coating or UV printing behind it.
- **Stomp pitch is 1.800 in (45.72 mm)**, A to B and B to C.

---

## The one real change: a THIRD stomp

The plugin has run on three stomps since v0.45. The control board still has
two (SW1, SW2). Everything else the plugin gained since then is firmware
only and costs the board nothing.

### Geometry, decided

Control board is 138.00 x 114.00 mm. Stomps stay on **y = 96.00**, the row
they are on now. Three of them, centred on the board:

| stomp | x (mm) | was |
|---|---|---|
| **A** | **23.28** | SW1 at 34.00, moves 10.72 mm outboard |
| **B** | **69.00** | new switch, dead centre |
| **C** | **114.72** | SW2 at 104.00, moves 10.72 mm outboard |

- pitch A-B = B-C = **45.72 mm (1.800 in)**
- **A to C = 91.44 mm (3.600 in)**
- margin to board edge = **23.28 mm** (was 34.00)
- switch body is 13.4 mm dia, so **16.58 mm** of clear board from body to
  edge, and **32.32 mm (1.272 in)** of clear air between neighbouring bodies
- D12.2 panel hole leaves **17.18 mm** from hole edge to board edge

### Drill drawing

Dimension from the **centreline**, not an edge. B lands exactly on the box
centre, which is a useful check: board x = 69.00 sits at 72.60 mm from the
outside face of a 145.2 mm wide 1590XX (3.05 mm wall + 0.55 mm board gap +
69.00), and 145.2 / 2 = 72.60. Symmetric by construction.

    face centreline
           |
    O------+------O ... A and C at centreline +/- 45.72 mm (+/- 1.800 in)
    A      B      C     three D12.2 holes, all on one line

### Parts

| what | detail |
|---|---|
| SW3 | Alpha **SF12011F-0102-20R-M-011**, same momentary PCB-pin part as SW1/SW2. Pin 1 (N.O.) to GND, pin 2 (COM) to the STOMP3 net, pin 3 (N.C.) unconnected, exactly as the other two are wired. |
| R | 10k 0603 pullup to 3V3, matching SW1/SW2 |
| C | 100n 0603 X7R to GND, matching SW1/SW2 |
| net | **STOMP3 -> GP22**, which `board.h` already reserves as `PIN_SPARE_GP22 22 // expansion room`. No GPIO has to be freed up. |
| LED | **none needed.** The chain is already 6: SECT_A, SECT_B, SECT_C, TEMPO, BYPASS, GATE. |

Roughly **$3 to $5 of parts per pedal** (the Alpha switch is the only real
money, and it is Alpha Taiwan direct rather than LCSC or Digi-Key stocked,
which is why the BOM carries it by MPN for PCBWay turnkey). The resistor and
cap are about a cent each.

### The LED row has to move with them

LED4 (34, 76), LED5 (104, 76) and LED6 (69, 76) sit 20 mm directly above the
stomp row. LED6 at x = 69 is currently above nothing, which strongly
suggests this row is SECT_A/B/C and was laid out anticipating a third stomp
in the middle. Confirm against the netlist, then:

| LED | x (mm) | note |
|---|---|---|
| LED4 | 34.00 -> **23.28** | follows stomp A |
| LED5 | 104.00 -> **114.72** | follows stomp C |
| LED6 | **69.00** | already correct, above stomp B |

The upper LED row (LED1-3 at y = 36) and all six pots stay on the existing
35 mm column grid at x = 34 / 69 / 104. Only the y = 76 row moves.

### What still has to happen

1. Add SW3 + its pullup and cap to the schematic, net STOMP3 to GP22.
2. Move SW1 and SW2 outboard to 23.28 and 114.72, place SW3 at 69.00.
3. Move LED4 and LED5 to match (after confirming the SECT mapping).
4. Re-route the area and re-run DRC and ERC.
5. Update the drill template to three D12.2 holes at centreline +/- 45.72 mm.
6. Add the SW3 line to the CONTROL board sheet of `BOM.xlsx`.

---

## Smaller items found while looking

- **The README sheet in `BOM.xlsx` is stale.** It still says "Stomps: Suntsu
  SSWFS-S01-AC09-HWH via Digi-Key", but the CONTROL board sheet was moved to
  the Alpha SF12011F on 2026-09-05 because the Suntsu was unsourceable. Fix
  before anyone orders from it.
- **The firmware is still two-switch.** v0.51 gave it the same press-length
  classifier the plugin uses (tap / medium / hold, 400 ms / 1.2 s), but the
  third switch, the per-circuit kills, the layers and the presets are not in
  `stomps.c` yet. Full parity is still outstanding.

## Still open, unrelated to the stomp

- Size the pad between the JFET and the Bazz Fuss on the breadboard.
- Consider running the LM567 at 5 V off an LDO with R16 pulled to VA. Pin 8's
  absolute max (15 V) is independent of V+, so the chip could sit at its
  datasheet sweet spot while the Q node still swings to the full rail.
- Test LM567 unit-to-unit variance before the next fab run.

---

## DONE (2026-09-21) - all of the above applied, plus corrections

Applied by Claude (Rev 8 session). Fresh backups: `*.bak_rev8_20260920` beside each
touched file. Both boards DRC/ERC verified back to their pre-change baselines.

1. **CONTROL schematic**: SW3 added (Alpha SF12011F-0102-20R-M-011, pin 1 N.O. to GND,
   pin 2 COM to STOMP3, pin 3 N.C. no-connect); STOMP3 wired to **J1 pin 15** (was
   spare - no header change needed). ERC 0 errors.
2. **Correction found while wiring SW3**: the schematic had SW1/SW2's GND on pin 3
   (N.C.) and the no-connect on pin 1 (N.O.) - inverted vs both the PCB copper and the
   active-LOW firmware (stomps.c reads !gpio_get). The PCB was right; the schematic is
   now fixed to match (GND on pin 1 for all three switches).
3. **Correction to the LED assumption**: the y=76 row is NOT SECT_A/B/C. Firmware chain
   order (hw_io.c: "nearest WS_IN first" + board.h enum) makes LED1-3 (pot row, y=36) the
   SECT LEDs and LED4/5/6 = TEMPO/BYPASS/GATE. It is still the stomp-feedback row, so it
   moved with the stomps as planned: LED4 -> 23.28, LED5 -> 114.72, LED6 already at 69.
4. **CONTROL PCB**: SW1 -> (23.28,96), SW2 -> (114.72,96), SW3 placed at (69,96).
   Unplanned collisions found and fixed: three GND stitching vias at (24,96)/(69,96)/
   (114,96) sat inside the new pad rows - deleted; the B.Cu /POT3_W vertical at x=115.5
   ran through SW2's new COM pad - detoured east around the pad row; C7 (bulk cap)
   courtyard hit LED5's new spot - C7 moved (122,76) -> (123.5,76), its +5V trace still
   lands in the pad. WSD3/WSD4/WSD5/+5V/STOMP1/STOMP2 rerouted for the moved parts;
   STOMP3 routed J1.15 -> SW3 COM (F.Cu along y=107.9, one via, B.Cu up x=69).
   Stomp silk is now "A" / "B" / "C" (the plugin's naming since v0.45) instead of
   TAP / BYPASS. Zones refilled. **DRC: 0 errors, 2 warnings** (both pre-existing
   pot-silk items at x=69 y=58; the old 6-warning baseline included 4 silk-over-copper
   hits from the TAP/BYPASS labels sitting on J1's pads - moving the labels cleared them).
5. **MAIN schematic**: R175 (10k to 3V3) + C125 (100n to GND) added on mcu sheet below
   the STOMP1/2 RCs; STOMP3 label on J10 pin 15 and Pico GP22 (pad 29, no-connect
   removed); "GP22 spare" note updated. ERC unchanged (same 3 pre-existing errors in
   other sheets, mcu sheet clean).
6. **MAIN PCB**: R175 at (112,107.35) rot 90 between RV1/RV2 (3V3 from RV1 pad 1; its
   silk refdes is hidden - the trimmer gap is too tight for text; it is in the centroid);
   C125 at (39.5,112.6) south of the Pico (GND via added at (41.3,111.8)). STOMP3 trunk:
   J10.15 south on B.Cu to y=113, west along y=113 to x=38, up on In2.Cu to y=89.45,
   via, F.Cu threading the 0.15mm lane between the Pico pad row and the CV1_ADC trace
   into GP22 (rule is 0.13). Zones refilled. **DRC: 6 errors / 67 warnings = exactly the
   pre-existing baseline** (the 6 are the documented courtyard kisses).
7. **Netlist parity checked pin-by-pin on both boards**: CONTROL 0 mismatches; MAIN's
   only mismatch is pre-existing and unrelated (Q11 pin 4 "VA" exists in the schematic
   netlist but the footprint has no pad 4 - worth a look someday, untouched by Rev 8).
8. **BOM.xlsx**: CONTROL SW row -> qty 3, SW1,SW2,SW3; MAIN 100n row -> qty 45 (+C125),
   10k row -> qty 41 (+R175); README sheet's stale Suntsu stomp lines replaced with the
   Alpha part (LCSC C14663 / C25804 re-verified in stock 2026-09-21).
9. **Fab package regenerated for BOTH boards** (gerbers + separate PTH/NPTH drills +
   maps + pos, kicad MCP with make_fab.ps1 settings, zones freshly refilled first):
   `fab/Glitchwave567_PCBWay_Release_20260921.zip`; README_PCBWAY.md bumped to rev 0.6;
   the 20260911 zip moved to _stale_DO_NOT_UPLOAD. Drill file spot-check: the three
   switch pad columns sit at exactly x = 23.28 / 69.00 / 114.72 on y=96, old positions gone.
10. **Still open**: enclosure drill template (three D12.2 holes at centreline +/- 45.72mm,
    check against ENCLOSURE_FIT before drilling); firmware third-stomp parity in stomps.c
    (STOMP3 = GP22, active-LOW, 10k pullup + 100n like the others); the "Still open,
    unrelated" items above.
