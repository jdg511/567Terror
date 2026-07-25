# Glitchwave 567 — Enclosure Fit Check (rev 0.1, 2026-07-25)

Checked against the **official Hammond 1590XX STEP model** (verified in CAD:
external 145.2 × 121.2 × 39.3 mm, floor 2.2, cavity 115.1 × 139.1 at the
floor growing to 116.5 × 140.5 with draft) and the **factory datasheets**
for the Suntsu stomp and ALPS pot (both now saved in `vendor_assets/`).

## ⛔ ORDER HOLD — one real conflict found, fix is cheap but needs a decision

**Do not upload the fab package to PCBWay yet.** The boards route and pass
DRC, but the PC-pin stomp mounting makes the pot knobs unusable. Details
and the fix below. Everything else fits.

## New facts measured from the CAD / datasheets

1. **Usable interior depth is 33.0 mm, not 35.0.** The last ~4 mm before
   the open edge has an internal lip (cavity narrows to 136.7 × 112.7 —
   smaller than our 138 × 114 boards). Boards live between the floor and
   the lip.
2. **There are no PCB bosses on the floor.** The only bosses are the four
   full-height corner columns for the 6-32 lid screws — which our 11 mm
   corner notches already clear. The inside floor has embossed Hammond
   text ~0.3 mm tall (harmless). Control-board retention must come from
   standoffs/adhesive mounts, not molded bosses.
3. **Suntsu SSWFS-S01 geometry (datasheet):** body 15.0 ± 1.0 mm deep
   below the bushing shoulder, bushing M12×1.0 × 14.3 long, actuator
   9.4 above that, travel 3.6. Wire variant (…-11-…-C20) exits 76.5 mm
   leads sideways at the body base; lug variant adds ~3.6 mm below.
4. **ALPS RK09K1130A5R geometry (datasheet):** shaft tip 20.0 mm above
   the PCB; the grippable knurl is only the top 7 mm (13.0 → 20.0 above
   PCB); body 6.8 tall.

## The conflict (with the PC-pin stomp as ordered)

A PC-pin stomp soldered to the control board with its shoulder seated on
the panel **forces** the control board to sit 15.0 mm below the face inner
surface (that's its body height — there is no adjustment). Face is 2.25
thick, so the face outer surface lands 17.25 mm above the control board:

> pot shaft tip 20.0 − 17.25 = **2.75 mm of shaft protruding. ±1 mm.**

No knob fits on 2.75 mm; the knurl is buried. Every pot becomes a
fingernail nub. (Checked the other direction too: there is no taller
LCSC-stocked RK09K, and even with taller pots this stack pushes the main
board too deep for the tall electrolytics. The 15 mm body is the problem.)

## The fix (all numbers verified to fit)

Switch the stomps to the **wire-lead variant** (SSWFS-S01-**AE11**-HWH-C20,
or the AC05 lug variant with leads dressed sideways) and panel-mount them
with their own nuts — the switch no longer dictates the board position:

| z (from face inner) | What |
|---|---|
| 0 – 15 (16 max) | stomp bodies, panel-mounted, hanging into the box |
| 8.0 | CONTROL board top face (on 8 mm mounts) — **pot knurl fully clears the face** (knurl starts 13 above PCB = 5.0 inside the face, tip 9.75 mm proud; full 7 mm knurl exposed → normal push-on knobs work) |
| 9.6 | control board bottom |
| 9.6 → 20.6 | 11 mm board-to-board header stack (J10) |
| 10.4 → 20.6 | main-board topside parts growing toward control (tallest: 470u can 10.2 → 0.8 mm clear of control board) |
| 15.6 | 1/4″ jack axes (5.0 above main top) — mid-wall, 15.6 from floor inner, 17.4 from the lip: **all wall holes comfortably inside solid wall** (DC at 16.95, CVs at 16.1) |
| 20.6 – 22.2 | MAIN board |
| 22.2 → 33.0 | 10.8 mm free to the lip — internal trims/DIPs reachable, lid clears everything |

Stomp bodies at max tolerance reach z 16, wires exit sideways — 3.6 mm
clear of the Pico module's PCB below (the Pico sits directly under STOMP1's
shadow, which is why the downward-lug variant is rejected: its lugs would
reach z 19.6, into the Pico).

### Required changes before ordering

1. **CONTROL board: two cutouts** ~27 × 14.5 mm at the stomp positions
   (34, 96) and (104, 96) so the panel-mounted bodies pass through; keep
   the existing stomp pads, moved to the cutout edge as wire terminals.
   (Copper in those zones: only the stomp nets — trivial reroute.)
2. **CONTROL board: mounting holes** for 4× M3 standoffs (adhesive-base
   nylon standoffs on the face inner, or countersunk M3 screws through
   the face — Jason's call on looks), set for 8.0 mm face-to-board-top.
3. **BOM: stomp line changes** from SSWFS-S01-AC09-HWH (PC-pin) to
   SSWFS-S01-AE11-HWH-C20 (wire) ×2 — same switch, same Digi-Key caveat;
   ships loose either way, two wires each at final assembly.
4. Main board: **no changes** (verified: nothing else violates the stack).
5. Drill template: unchanged (same face XY, same Ø12.2 + keyway).

## What passes as-is

- Board outlines 138 × 114 with 11 mm notches: fit the cavity at every
  depth they occupy, clear the corner bosses.
- All six wall jack holes: comfortably mid-wall (worst margin ≥ 9 mm of
  solid metal each side of the hole edge).
- Stomp bushing: 14.3 mm thread through the 2.25 face + washer + nut with
  room to spare; actuator stands ~21 mm proud — normal stomp feel.
- Vertical budget: 22.2 mm stack in 33.0 usable — 10.8 mm spare over the
  main board for the under-the-lid trims and DIP switches.
- Pico USB: faces the board interior between the boards; firmware loads
  by lifting the lid and using a short cable, or pre-flashing — unchanged.

## Decision needed from Jason

The stomp mounting change reverses your earlier "PC-pin" call — but the
PC-pin geometry physically buries the pot knobs, so something had to give.
Options: (a) approve the wire-variant fix above (recommended — I'll rev
the control board same-day); (b) keep PC-pin stomps and accept knob-less
recessed pots (not recommended); (c) different pots entirely (taller
shaft, needs a new sourcing hunt and still tightens the can clearance).
The fab package stays on hold until you pick.
