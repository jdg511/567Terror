# Glitchwave 567 / "Where The Fuzz Meets The Funk" - PCBWay ordering guide
**rev 0.5, 2026-09-11.** Supersedes rev 0.2 (2026-07-29). Previous copy kept as
`README_PCBWAY.bak_20260911.md`.

Two boards, one enclosure (Hammond 1590XX). Order both as **4-layer, full turnkey
assembly**. Both boards are silkscreened
"WHERE THE FUZZ MEETS THE FUNK - MAIN / CONTROL  rev0.1  Illicit Apothecary".

## Upload this

`Glitchwave567_PCBWay_Release_20260911.zip` in this folder. It contains:

```
Gerbers/MAIN_board/       16 files, plotted 2026-09-11 by review/tools/make_fab.ps1
Gerbers/CONTROL_board/    16 files, same run
BOM/PCBWay_BOM_main.csv   96 lines / 297 designators (incl. 2 DNP rows)
BOM/PCBWay_BOM_ctrl.csv   11 lines /  22 designators
BOM/BOM.xlsx              the master sheet the CSVs are generated from
Centroid/Centroid_main.csv  297 placement rows
Centroid/Centroid_ctrl.csv   22 placement rows
README_PCBWAY.md          this file
KiCad_Source/             schematics, boards, project footprint library
```

Nothing else in `hardware/fab/` is a deliverable. `_stale_DO_NOT_UPLOAD/` holds the
superseded 2026-08-02 submission folder and assorted junk; see its READ_ME.txt.

## Board specs (both boards, identical stackup)

| Parameter | Value |
|---|---|
| Layers | 4 (F.Cu route / In1 solid GND plane / In2 route / B.Cu route) |
| Size | 138.00 x 114.00 mm, corner notches 11 x 11 mm, in Edge.Cuts |
| Thickness | 1.6 mm, 1 oz copper all layers |
| Min track / clearance | 0.25 mm / 0.13 mm (PCBWay standard 5/5 mil capable) |
| Vias | 0.5 mm pad / 0.3 mm drill, min hole-to-hole 0.25 mm |
| Smallest drill | 0.30 mm PTH (PCBWay minimum is 0.15 mm, so large margin) |
| Finish | lead-free HASL is fine; ENIG optional |
| Mask / silk | any colour / white |
| Quantity | 5 of each board |
| Assembly sides | MAIN: top only. CONTROL: top, plus one bottom part (J1) |

**DRC/ERC as of 2026-09-11.** MAIN: 73 violations, 6 errors, all six courtyard
overlaps with the bodies verified physically clear (see the table below), 67
silkscreen warnings, and zero shorts, zero clearance, zero unconnected.
CONTROL: 6 violations, 0 errors. MAIN ERC has 3 errors, all three
known and benign: U10 and U15 pins 5+12 are paralleled LM13700 OTA outputs
(current-mode summing, which ERC cannot model), and U5's VCC has no PWR_FLAG
because its rail arrives through D105 from the 78L09.

Every footprint on both boards now carries a courtyard (13 were missing until
2026-09-11), so DRC can finally see body collisions on the panel jacks, the Pico
and the pots. That surfaced two new courtyard overlaps on MAIN. Neither is a real
collision; measured body-to-body gaps:

| Pair | Courtyard overlap | Real body gap | Closest copper |
|---|---|---|---|
| C47 x U8 | 7.40 x 0.10 mm | 0.400 mm | 2.46 mm |
| C110 x C43 | 2.96 x 0.39 mm | 0.189 mm | 2.61 mm |
| C109 x C94 | 2.67 x 0.17 mm | 0.406 mm | 2.83 mm |
| Q11 x U9 | 2.10 x 0.18 mm | 2.200 mm | 0.84 mm |
| **J5 x FB100** (new) | 2.40 x 0.30 mm | 0.700 mm | ~0.21 mm |
| **J4 x R98** (new) | 2.96 x 0.23 mm | 0.338 mm | 2.58 mm |

Courtyards are not plotted into gerbers, so none of this reaches the fab. It is an
internal design-review signal only. The tightest spots to eyeball on the first
article are the DC jack J5 beside the ferrite bead FB100, and the 3.5 mm jack J4
beside R98.

## Do not populate

**C41 and C42 must be left bare.** They are the LFIL/OFIL filter pads beside the
LM567 and are deliberately unpopulated: they are the voice of the pedal. They
appear in the BOM with MPN `DNP` and `DNP` in the last column, and they appear in
the centroid because the pads exist. Please do not "helpfully" fit them.
No other part is DNP.

## Turnkey sourcing notes

1. **Every line carries a real MPN and a verified manufacturer.** On 2026-09-11
   all 87 distinct LCSC part numbers on the two boards were looked up individually
   in the live LCSC/JLC catalogue. Every one is a real, orderable part, in stock,
   with the capacitance / resistance / package matching the BOM. Zero dead part
   numbers and zero value mismatches. The Manufacturer column is now read from
   that lookup rather than guessed from the MPN, which is what had been putting
   CUI Devices on XKB and HOOYA jacks, Sunlord on a TAI-TECH ferrite and an SXN
   inductor, TDK on a CCTC capacitor, and "UNI-ROYAL (Uniohm)" on 40 lines of
   UNI-ROYAL(Uniroyal Elec) resistors. The lookup table lives in
   `kicad/tools/lcsc_verified_20260911.json`.
   Thinnest stock at time of check, none of them a problem for 5 boards:
   LM567CMX/NOPB 562, PJ-603A 518, PCM12SMTR 441, DC-044A 1210, MMBTA13 3084.
   Parts cost at LCSC qty-1 pricing is about **$28.34 per MAIN board and $4.75 per
   CONTROL board**, so roughly $33 a set plus the Pico and the two Alpha stomps.
2. **Stomp switches SW1/SW2 on CONTROL: Alpha SF12011F-0102-20R-M-011.** Not DNP.
   A genuine PC-pin SPDT momentary footswitch from Alpha's SF12 series (Taiwan),
   terminals in a straight row at 2.5 mm pitch, Pin 1 = N.O., Pin 2 = COM,
   Pin 3 = N.C. Not stocked by LCSC or DigiKey; PCBWay sources it direct by MPN.
   Both stay MOMENTARY (not latching) to preserve the firmware's STARVE gesture.
3. **Raspberry Pi Pico (U20), MPN SC0915.** The official module, SMD-mounted flat
   on its castellated edge pads. PCBWay can source it, or Jason can consign two.
4. **Pots RV1-RV6 on CONTROL: ALPS RK09K1130A70**, B10k linear, snap-in, 30 mm
   shaft. MAIN's RV1-RV3 are a different part: Bourns 3224W-1-103E SMD trimmers.
5. **THT vs SMD.** Only 7 parts on MAIN are through hole: the six panel jacks
   (J1, J2 = PJ-603A; J3, J4, J6 = PJ-3410; J5 = DC-044A) and the stack socket
   J10. Everything else on MAIN is SMD. On CONTROL the pots, both stomps and the
   stack header J1 are through hole; the caps and the six WS2812B are SMD. The
   Type column in both BOM CSVs is generated from each footprint's own mounting
   attribute in the board file, not guessed from its name.

## Assembly notes

- **CONTROL board stack header.** Pots, stomps and WS2812 LEDs mount on F.Cu; the
  2x8 keyed stack header (ref **J1** on CONTROL) mounts on **B.Cu**. It uses a
  project-specific **pre-mirrored** footprint,
  `PinHeader_2x08_P2.54mm_Vertical_Mirrored`, which cancels KiCad's flip so pin N
  lands on pin N of J10 below. Verified 16/16 pads and 16/16 nets against J10.
  **Do not substitute the stock PinHeader_2x08 footprint**: flipping the stock one
  permutes the pin numbers (1 to 15, 2 to 16, 3 to 13, and so on) and reverses the
  whole connector.

  | Board | Half | Part | LCSC | Key dimension |
  |---|---|---|---|---|
  | MAIN, J10, **top** side | female socket | XFCN **PM254V-12-16-H85** | C46595985 | body **8.50 +/-0.15 mm** |
  | CONTROL, J1, **bottom** side | male header | XFCN **PZ254V-12-16P** | C492425 | insulator **2.50 mm**, mating pin 6.0 +/-0.20 mm |

  2.50 + 8.50 = **11.00 mm**, plastic face to plastic face: a positive mechanical
  stop, not a nominal. Worst case 10.70 to 11.30 mm. The 6 mm pin sits inside the
  socket's 3.68 to 6.35 mm insertion window and does not bottom out, which is what
  makes the plastics the datum. Both halves re-verified real and in stock
  2026-09-11.

  **Do not substitute a long-pin or stacking header** (13.5 mm pins, say). It
  would exceed the socket's maximum insertion depth, bottom out internally, hold
  the plastics apart and leave the gap indeterminate.

  **The socket goes on MAIN deliberately.** MAIN carries the DC jack, so its half
  is live whenever power is applied. Recessed socket contacts cannot be shorted by
  a dropped screw or a probe; 16 exposed pins at 18 V can. It also puts the
  lighter half on the board that hangs upside down.
- **MAIN board jacks.** The six wall-mounted jacks are right-angle parts hanging
  off the board edges. Verify they sit flush before soldering.
- **CONTROL board standoffs.** Four M3 holes at (17, 9.5), (121, 9.5), (17, 108.5)
  and (121, 108.5), 3.2 mm NPTH. MAIN deliberately has none: six jack bushings
  with nuts through the enclosure walls retain it.
- First-article checklist is the README sheet inside `BOM.xlsx`. Run it before
  boxing the assembled boards.

## Changes since rev 0.2

- **2026-09-11: C107 duplicate reference designator fixed, and a missing 22 uF
  added.** The MAIN schematic had two different C107 symbols: a 100n 0603 on
  FREQ_CV in the mcu sheet, and a 22 uF 0805 in the power sheet that is the
  MP1584's second output capacitor. Only the 100n existed on the PCB, so the +5 V
  rail had half its intended bulk capacitance and the BOM listed C107 twice with
  conflicting parts. The power-sheet capacitor is now **C112**, placed at
  (62.6, 25.0) on MAIN with a 0.3 mm stub to the +5 V trunk and its return through
  the F.Cu ground pour, exactly as C108 is connected. Zones refilled, DRC and ERC
  both re-run with no change to their baselines.
- **2026-09-11: BOM generator rewritten** (`kicad/tools/build_pcbway_boms_20260911.py`).
  The Type column now comes from the board's own mounting attributes instead of a
  guess that defaulted to SMD, which had mislabelled all six panel jacks as SMD
  and the SMD trimmers and slide switches as THT. Manufacturer resolution no
  longer does substring matching, which had been putting "TI" on the XFCN headers
  and the Bourns trimmers (it was matching inside the word "Vertical") and "LM" on
  the TI LM13700 and LM567. DNP rows are now emitted instead of dropped, so every
  centroid row has a matching BOM line.
- **2026-09-11: stale package quarantined.** The 2026-08-02 `PCBWAY_SUBMISSION/`
  folder moved to `_stale_DO_NOT_UPLOAD/`.
- 2026-09-05: CONTROL stomp switches changed to Alpha SF12011F-0102-20R-M-011,
  DNP cleared, footprint and routing updated.
- 2026-08-02: CONTROL stack header moved to B.Cu; CONTROL M3 standoff holes added;
  board-to-board connector pair chosen; BOM backfilled so no line says "generic".
- 2026-07-29: RV1-RV6 changed to ALPS RK09K1130A70 (30 mm shaft) to fix knob
  protrusion. BOM-only change, no board rework.

## Still open (does not block the PCB order)

**The enclosure drill template is stale and quarantined.**
`drill_template_1to1.STALE_2026-07-25_DO_NOT_USE.pdf` still shows the input jack
at its pre-2026-07-31 position (J1 moved +3.5 mm in x). It was never a fab
deliverable, only a 1:1 print for the drill press, so it does not affect this
order. A fresh one must be produced before any enclosure is drilled.
