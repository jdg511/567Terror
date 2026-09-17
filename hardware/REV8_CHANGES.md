# Rev 8 hardware change list

What the board and BOM need in order to match the plugin as of v0.51.
**Nothing here has been applied to the schematic, the PCB or the BOM yet.**
Written 2026-09-17 while Jason was away, because the one real change touches
the drilled enclosure face and that is his call, not mine.

Backup of the files this would touch:
`hardware/_backups/prev051-20260916-214347/`

---

## The one real change: a THIRD stomp

The plugin has run on three stomps since v0.45. The control board still has
two (SW1, SW2). Everything else the plugin gained since then is firmware
only and costs the board nothing.

| what | detail |
|---|---|
| SW3 | Alpha **SF12011F-0102-20R-M-011**, same momentary PCB-pin part as SW1/SW2. Pin 1 (N.O.) to GND, pin 2 (COM) to the STOMP3 net, pin 3 (N.C.) unconnected, exactly as the other two are wired. |
| R | 10k 0603 pullup to 3V3, matching SW1/SW2 |
| C | 100n 0603 X7R to GND, matching SW1/SW2 |
| net | **STOMP3 -> GP22**, which `board.h` already reserves as `PIN_SPARE_GP22 22 // expansion room`. No GPIO has to be freed up. |
| LED | **none needed.** The chain is already 6: SECT_A, SECT_B, SECT_C, TEMPO, BYPASS, GATE. SECT_C exists and is currently unused. |

### Approximate cost

Rough, not quoted: the Alpha switch is the only real money, call it **$3 to
$5** in ones through PCBWay turnkey (it is Alpha Taiwan direct, not LCSC or
Digi-Key stocked, which is why the BOM carries it by MPN). The resistor and
cap are about **a cent each**. So roughly **$3 to $5 of parts per pedal**,
plus whatever the third D12.2 hole costs in the enclosure face.

### Why this needs Jason, not me

The control board bolts behind the drilled face and carries six pots and six
LEDs already. A third stomp means:

1. a third **D12.2** hole in the 1590XX face, so the drill drawing changes
2. finding room for a 13.4 mm body, ~12 mm deep, between the existing two
3. re-running the routing around the new footprint

Point 1 is the one that matters: it changes a physical part that may already
be ordered or machined, and the spacing between three stomps is an
ergonomics decision (can you hit A and C with one foot for Layer Z?) that
wants a real foot and a real box, not a guess from me.

---

## Smaller items found while looking

- **The README sheet in `BOM.xlsx` is stale.** It still says "Stomps: Suntsu
  SSWFS-S01-AC09-HWH via Digi-Key", but the CONTROL board sheet was moved to
  the Alpha SF12011F on 2026-09-05 because the Suntsu was unsourceable. The
  README note should be brought in line before anyone orders from it.
- **The firmware is still two-switch.** v0.51 gave it the same press-length
  classifier the plugin uses (tap / medium / hold, 400 ms / 1.2 s), but the
  third switch, the per-circuit kills, the layers and the presets are not in
  `stomps.c` yet. Full parity is still outstanding.

## Still open from earlier, unrelated to the stomp

- Size the pad between the JFET and the Bazz Fuss on the breadboard.
- Consider running the LM567 at 5 V off an LDO with R16 pulled to VA. Pin 8's
  absolute max (15 V) is independent of V+, so the chip could sit at its
  datasheet sweet spot while the Q node still swings to the full rail.
- Test LM567 unit-to-unit variance before the next fab run.
