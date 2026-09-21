# Rev 8 hardware change list

What the board and BOM need in order to match the plugin as of v0.51.
**Nothing here has been applied to the schematic, the PCB or the BOM yet.**

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
