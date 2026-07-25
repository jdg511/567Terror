# Glitchwave 567 — PCBWay ordering guide (rev 0.1, 2026-07-25)

> **⛔ HOLD (2026-07-25): do not order yet.** The enclosure fit check
> (`hardware/ENCLOSURE_FIT.md`) found the PC-pin stomp mounting buries the
> pot knobs; the CONTROL board needs two stomp cutouts + mounting holes and
> the stomp BOM line changes to the wire variant. Order after that rev.

Two boards, one enclosure (Hammond 1590XX). Order both as **4-layer, full
turnkey assembly**. Gerbers/drill/pos live next to this file; BOMs are the
sheets of `hardware/BOM.xlsx` (MAIN = 297 lines, CONTROL = 22 lines).

## Board specs (both boards identical stackup)

| Parameter | Value |
|---|---|
| Layers | 4 — F.Cu route / In1 solid GND plane / In2 route / B.Cu route |
| Size | 138 × 114 mm, corner notches 11 × 11 mm (in Edge.Cuts) |
| Thickness | 1.6 mm, 1 oz copper all layers |
| Min track / clearance used | 0.25 mm / 0.13 mm (PCBWay standard 5/5 mil capable) |
| Vias | 0.5 mm pad / 0.3 mm drill, min hole-to-hole 0.25 mm |
| Finish | HASL lead-free is fine (ENIG optional) |
| Mask / silk | Any color Jason likes; silk clipping over pads is expected |

DRC status (KiCad 10, 2026-07-25): **0 unconnected, 0 clearance/short/hole
violations** on both boards. Remaining warnings are cosmetic silkscreen
overlaps plus 4 shallow courtyard kisses (≤0.4 mm, bodies verified clear) —
no action needed.

## Turnkey sourcing notes

1. **LCSC parts** — every BOM line carries an LCSC C-number (stock verified
   2026-07-25). Pots: ALPS RK09K1130A5R (C209779) ×6, B10k linear, snap-in.
2. **Digi-Key line** — stomp switches on the CONTROL board:
   Suntsu **SSWFS-S01-AC09-HWH** (PC-pin, soft-touch momentary SPST) ×2.
   It is a Digi-Key *Marketplace* listing — confirm PCBWay will purchase
   Marketplace stock; fallback **SSWFS-S01-AC05-HWH** (solder-lug, Digi-Key
   own stock, $2.95) ships loose and hand-wires to the same pads.
3. **Raspberry Pi Pico (U20)** — the official Pico module, SMD-mounted flat
   (castellated edge pads, official land pattern). PCBWay can source it or
   Jason can consign two.
4. **DNP parts** — LFIL/OFIL cap pads near the LM567 are intentionally
   unpopulated ("the voice" of the pedal). They are marked DNP in the BOM.
   Do not let the fab "helpfully" populate them.

## Assembly notes

- CONTROL board: pots, stomps, and WS2812 LEDs mount on F.Cu; the 2×8 keyed
  stack header (J10) mounts on B.Cu. Same header on MAIN mounts topside at
  (96, 104) — only DC nets cross the header.
- MAIN board: wall-mounted jacks (2× PJ-603A, 3× PJ-3410, 1× DC-044A) are
  right-angle parts hanging off board edges — verify they sit flush before
  wave/hand solder.
- First-article checklist is the README sheet inside `BOM.xlsx` — run it
  before boxing the assembled boards.

## Suggested order

Qty 5 of each board, turnkey, lead-free HASL, any-color mask. One enclosure
drill template (`drill_template_1to1.pdf`) is printed at 100% scale — it is
NOT a fab deliverable, it is for Jason's drill press.
