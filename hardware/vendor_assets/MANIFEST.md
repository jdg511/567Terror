# Vendor assets manifest — Glitchwave 567 hardware

Collected 2026-07-25 via find-missing-3d-models workflow. All files verified
as real PDFs (`%PDF` magic). Dimensions below were read from the actual
factory drawings, not guessed.

## Enclosure

| File | Part | Source | Status |
|---|---|---|---|
| `1590XX_reference_drawing.pdf` | Hammond 1590XX | hammfg.com official | VERIFIED |

Key numbers (official drawing): external 145.2 × 121.2 × 39.3 mm; internal
cavity 140.7 × 116.7 at lid face → 139.0 × 115.0 at closed face (draft ~1.4°);
inside depth 35.0 mm; wall 2.0 mm; floor (pedal top face) 2.25 mm; 4× corner
bosses ~Ø9, 6-32 UNC, lid screw centers 135.0 × 111.0. Hammond max PCB
138 × 114 (needs corner notches). CAD not yet downloaded:
- STEP: https://www.hammfg.com/files/parts/stp/1590XX.zip
- DXF:  https://www.hammfg.com/files/parts/dxf/1590XX.zip

## Jacks (all LCSC-verified in stock 2026-07-25)

| File | Part | LCSC | Role | Key dims |
|---|---|---|---|---|
| `PJ-603A_datasheet.pdf` | HOOYA PJ-603A 6.35mm TRS, right-angle, M12 bushing | C309273 | IN + OUT (2×) | axis 5.0 mm above PCB; panel hole ~12.2; 20.5 mm behind panel |
| `PJ-3410_datasheet.pdf` | XKB PJ-3410 3.5mm w/ switch contact, right-angle, M7.7×0.75 | C5146694 | CV1/CV2 in + CV out (3×) | axis 4.5 mm above PCB; panel hole ~8.0; 17.5 mm behind panel |
| `DC-044A-2.5A-2.0_datasheet.pdf` | XKB DC-044A 2.1mm barrel, 3A, right-angle | C319095 | DC in (1×) | axis ~3.6–3.9 mm (VERIFY vs 3D model); Ø6 nose, hole ~6.4 |
| `DC-044-20A_datasheet.pdf` | Hroparts DC-044-20A (alternate) | C136713 | DC alt | axis exactly 6.0 mm but only 0.5 A rated |
| `PJ-324M_datasheet.pdf` | SOFNG PJ-324M 3.5mm M6×0.5 (alternate) | C22355739 | CV alt | axis 3.2 mm; Eurorack M6 thread but only 3.5 mm long |
| `PJ-625DK_datasheet.pdf` | HOOYA PJ-625DK dual 6.35mm block (alternate) | C309295 | IN+OUT alt | fixed 16.5 mm pitch; axis ~12–13 unverified; low stock |

## Pots (LCSC-verified)

Selected: **ALPS RK09K1130A5R** — LCSC **C209779**, B10k LINEAR, vertical
snap-in, 20 mm knurled plastic shaft, tip 20 mm above PCB, stock 3,400+.
Datasheet (LCSC-hosted, not saved locally):
https://datasheet.lcsc.com/datasheet/pdf/fe00e3a4c2ecb8edf80cea6be8ff3e19.pdf?productCode=C209779

CRITICAL FINDING: LCSC stocks NO 9mm vertical pot with threaded bushing/nut
(Alpha RK09K-with-bushing style is JLCPCB-only, 404s on LCSC). The RK09K1130
is snap-in — shafts pass through plain Ø7 enclosure holes, no nuts; the
control board must be retained by standoffs. All 6 controls become B10k
linear; the Pico applies tapers in firmware (audio-taper VOL curve etc.), so
this is sonically identical to the plugin.

## Stomp switches — RESOLVED (2026-07-25)

LCSC stocks NO soft-touch momentary footswitch, but PCBWay turnkey also
sources from Digi-Key/Mouser/Arrow/Avnet (per pcbway.com assembly pages).

Selected: **Suntsu SSWFS-S01 series** — soft-touch SPST OFF-(ON) momentary
stomp, PBS-24B-2 form factor: 25.5×13.0 mm body, **M12×1.0 thread** (14.3 mm
hex bushing), 6 A/125 VAC, 100k mech cycles.
- Primary: SSWFS-S01-AA09/AC09-HWH (PC-mount pins → control board footprint).
  Digi-Key MARKETPLACE listing — confirm PCBWay buys Marketplace items
  before ordering (unverified).
- Fallback: SSWFS-S01-AC05-HWH (solder lug), Digi-Key own stock, $2.95,
  ships loose with the order → 2 wires to the same control-board pads.
Datasheet: https://suntsu.com/wp-content/uploads/2021/02/SSWFS-S01-AE11-0HWH0-C20.pdf
Caveat: datasheet doesn't literally say "soft touch"; form factor/construction
match the Daier soft-touch exactly, feel inferred. Rejected: Gorva (no
distributor stock), Bulgin MPI002 (19.2 mm, 35k cycles, ~$25, 12 N heavy),
E-Switch PV6F240SS kept as anti-vandal fallback (Mouser, $11.45).

## Not obtained / to do

- Hammond 1590XX STEP + DXF (URLs above) — grab before enclosure 3D check.
- RK09K1130A5R + PBS-24B factory drawings saved locally.
- XKB DC-044A axis height confirmation (3D model or sample measurement).
