# PCBWay order readiness check, 2026-09-11

Independent re-verification of the fab package before uploading, followed by the fixes.
Everything below was re-derived from the live files, not copied from earlier notes.

## Verdict: READY TO UPLOAD

Upload `hardware/fab/Glitchwave567_PCBWay_Release_20260911.zip`.

One real defect was found and fixed (a duplicate reference designator that cost the
board a 22 uF capacitor), plus three BOM-generator bugs. Everything was re-verified
after the changes.

---

## 1. What was found and fixed today

### F1. C107 was a duplicate refdes, and the board was short one 22 uF (FIXED)

`C107` existed twice in the MAIN schematic:

| Sheet | Value | Footprint | Function |
|---|---|---|---|
| `mcu.kicad_sch` | 100n | C_0603 | filter cap on FREQ_CV |
| `power.kicad_sch` | 22u | C_0805 | second MP1584 output cap, parallel with C108 |

The PCB had only one C107 (the 100n on FREQ_CV/GND). The power sheet's 22 uF had **no
footprint on the board at all**, so the +5 V buck output would have been built with one
22 uF instead of the two the schematic calls for. That rail feeds the Pico and, through
J10, six WS2812B LEDs on the control board, which is roughly 360 mA of switching load at
full white. The BOM listed C107 on two rows with two different parts, so PCBWay would
have bounced it or placed a 22 uF 0805 on a 0603 land.

**Nothing caught it** because KiCad's netlist collapses the duplicate to one component:
footprint count equalled unique-refdes count, DRC schematic parity was satisfied, and
ERC raised no duplicate-reference violation. Only a designator-level BOM-versus-board
diff finds it.

Fix applied:

* `power.kicad_sch`: the 22 uF symbol renamed **C107 to C112**. 297 symbols, no duplicates.
* MAIN PCB: **C112 placed at (62.6, 25.0)**, `C_0805_2012Metric`, rotation 0, top side.
  Pad 1 (+5 V) fed by a new 0.3 mm F.Cu stub from (61.65, 25.0) up to the existing +5 V
  trunk at y = 23.65. Pad 2 (GND) returns through the F.Cu ground pour, exactly the way
  C108's ground pad is connected. Verified geometrically: the refilled pour surrounds
  pad 2 on all four sides and has a proper clearance cutout around pad 1.
* Placement was chosen by a clearance search over the region, not by eye. Nearest
  obstacle margin 0.42 mm against a 0.13 mm rule. No via was added: every via position in
  that pocket came out under 0.23 mm against the In2.Cu diagonals, and C108 is pour
  connected the same way.
* Zones refilled, `BOM.xlsx` row changed from `C107,C108` to `C108,C112`.

### F2. BOM Type column was wrong on 12 designators (FIXED)

`build_pcbway_boms_20260905.py`'s `guess_type()` guessed from the footprint name and
defaulted to `SMD`.

| Designators | Part | Said | Actually |
|---|---|---|---|
| J1, J2 | PJ-603A 6.35 mm jack | SMD | **through hole** |
| J3, J4, J6 | PJ-3410 3.5 mm jack | SMD | **through hole** |
| J5 | DC-044A barrel jack | SMD | **through hole** |
| RV1, RV2, RV3 | Bourns 3224W-1-103E trimmer | THT | **SMD** (LCSC C81348) |
| SW1, SW2, SW3 | C&K PCM12SMTR slide switch | THT | **SMD** (LCSC C221841) |

PCBWay quotes and processes SMT and THT separately, so six mislabelled panel jacks means
a wrong quote. The Type column is now read from each footprint's own mounting attribute
in the board file. MAIN now reports exactly 7 THT parts, matching the board.

### F3. BOM Manufacturer column was wrong on 6 lines (FIXED)

`guess_manufacturer()` did a case-insensitive **substring** match against a name list.
`"TI"` matched inside "Ver**ti**cal" and "Poten**ti**ometer"; `"LM"` matched inside
"**LM**13700".

| Designators | MPN | Said | Actually |
|---|---|---|---|
| MAIN J10, CTRL J1 | PM254V-12-16-H85 / PZ254V-12-16P | TI | **XFCN** |
| MAIN RV1-RV3 | 3224W-1-103E | TI | **Bourns** |
| U3, U7, U10, U11, U12, U15 | LM13700MX/NOPB | LM | **Texas Instruments** |
| U5 | LM567CMX/NOPB | LM | **Texas Instruments** |
| D105 | 1N4148W | LM | generic, now blank |

The old script's own prefix table also mapped PZ254V/PM254V to "CVILUX", which is also
wrong. Matching is now by verified MPN prefix only, never against the value or footprint
text. Four MPNs that carried a parenthetical vendor note were cleaned up so the MPN
column parses: `BCP56 (HXY)` to `BCP56` / HXY MOSFET, `MMBTA13 (FUXINSEMI)` to `MMBTA13`
/ FUXINSEMI, `78L09 (UMW 78L09-150)` to `78L09-150` / UMW, and
`SC0915 Raspberry Pi Pico` to `SC0915` / Raspberry Pi.

### F4. C41/C42 DNP mismatch between BOM and CPL (FIXED)

Both are flagged DNP in `BOM.xlsx`. The old generator dropped them from the CSV while
they stayed in the centroid, so PCBWay saw two placements with no BOM line. They are now
emitted with MPN `DNP`, a `DNP` column, and "DO NOT POPULATE" in the description, and
README_PCBWAY.md calls them out explicitly.

### F5. Stale package quarantined (FIXED)

`fab/PCBWAY_SUBMISSION/` was the 2026-08-02 package, predating both the stomp-switch swap
and this C112 fix, and it was the likeliest thing to be uploaded by mistake given its
name and its STOP_READ_ME_FIRST.md. It and the junk files (`ziD43XBN`, `zib47OUg`, a
0-byte zip, an old .rar, a scratch folder) are now in
`fab/_stale_DO_NOT_UPLOAD/` with a READ_ME. `README_PCBWAY.md` is rewritten as rev 0.3.

### F6. Two schematic footprint fields named footprints that do not exist (FIXED)

C100 pointed at `CP_Elec_8x10.2` and C48 at `CP_Elec_10x10.2`; the board correctly uses
`CP_Elec_8x10` and `CP_Elec_10x10`. Those bad names were leaking into the BOM's Package
column and raising two ERC warnings. Corrected in both the schematics and `BOM.xlsx`.

---

## 2. Verification after the changes

**DRC.** MAIN 72 violations, 4 errors, 68 warnings, identical to the pre-change
baseline: the same four shallow decoupling-cap courtyard kisses and the same silkscreen
warnings. Zero shorts, zero clearance, zero unconnected. C112 appears in zero
violations. CONTROL 6 violations, 0 errors, unchanged.

**ERC.** MAIN 16 violations, 3 errors (was 18 / 3; the two footprint-library warnings are
gone). The three errors are known and benign: U10 and U15 pins 5+12 are paralleled
LM13700 OTA outputs doing current-mode summing, which ERC cannot model, and U5's VCC has
no PWR_FLAG because its rail arrives through D105 from the 78L09. CONTROL 0 errors,
8 library-copy warnings.

**Designator parity, all three artefacts, both directions:**

| Board | Placeable footprints | BOM designators | Centroid rows | Duplicates | Orphans |
|---|---|---|---|---|---|
| MAIN | 297 | 297 | 297 | none | none |
| CONTROL | 22 | 22 | 22 | none | none |

CONTROL's four M3 mounting holes are correctly flagged `exclude_from_pos_files` and are
absent from both BOM and centroid.

**Freshness.** Every fab artefact is newer than both `.kicad_pcb` files. Gerbers, drill,
drill maps and position files for BOTH boards were regenerated by
`review/tools/make_fab.ps1` with the real kicad-cli, which passes `--check-zones`, so
they cannot have been plotted from a stale zone fill. C112's two paste apertures are
present in `glitchwave567-F_Paste.gtp`.

**Board specs, measured from Edge.Cuts.** Both boards 138.00 x 114.00 mm, 4 copper
layers, 1.6 mm. Smallest PTH drill 0.300 mm against a PCBWay minimum of 0.15 mm.
MAIN assembly is 100% top side; CONTROL is top side plus one bottom part (J1).

**Parts re-checked live today:** XFCN PZ254V-12-16P (C492425, 28,029 in stock), XFCN
PM254V-12-16-H85 (C46595985, 3,385), Samsung CL21A226MAQNNNE 22 uF 25 V X5R 0805
(C45783), Bourns 3224W-1-103E (C81348, SMD), C&K PCM12SMTR (C221841, SMD), UMW 78L09-150
(C347271), HXY BCP56 (C5345976).

---

## 3. Order parameters

| Field | Value |
|---|---|
| Boards | 2 distinct designs, order separately |
| Size | 138 x 114 mm each |
| Layers | 4 |
| Thickness | 1.6 mm |
| Copper | 1 oz outer and inner |
| Min hole | 0.30 mm |
| Surface finish | lead-free HASL (ENIG optional) |
| Mask / silk | any colour / white |
| Quantity | 5 each |
| Assembly | full turnkey, PCBWay sources by MPN |
| Assembly sides | MAIN top only; CONTROL top plus one bottom part |
| Special instruction | **C41 and C42 are DNP, do not populate** |

PCBWay assembly pricing is a manual quote, typically 1 to 2 business days after upload.
There is no instant price, so budget for that.

---

## 4. Second pass, same day: courtyards and silkscreen

### F7. All 13 missing courtyards added (FIXED)

J1-J6 and U20 (the Pico) on MAIN and RV1-RV6 on CONTROL had no courtyard, so DRC was
structurally blind to body collisions on exactly the largest mechanical parts. That is
why both jack collisions went undetected in July. Courtyard outlines were copied from the
library footprints into all 13 board instances. **Every footprint on both boards now has
one.**

What that exposed: two new courtyard overlaps on MAIN, both of them courtyard-only with
the real bodies clear. Nothing was moved, because nothing collides.

| Pair | Courtyard overlap | Real body gap | Closest copper | Status |
|---|---|---|---|---|
| C47 x U8 | 7.40 x 0.10 mm | 0.400 mm | 2.46 mm | pre-existing |
| C110 x C43 | 2.96 x 0.39 mm | 0.189 mm | 2.61 mm | pre-existing |
| C109 x C94 | 2.67 x 0.17 mm | 0.406 mm | 2.83 mm | pre-existing |
| Q11 x U9 | 2.10 x 0.18 mm | 2.200 mm | 0.84 mm | pre-existing |
| **J5 x FB100** | 2.40 x 0.30 mm | 0.700 mm | ~0.21 mm | **newly visible** |
| **J4 x R98** | 2.96 x 0.23 mm | 0.338 mm | 2.58 mm | **newly visible** |

Courtyards are not plotted into gerbers, so none of this reaches PCBWay. It is an
internal design-review signal. The tightest spots to eyeball on the first article are the
DC jack J5 beside the ferrite bead FB100 (0.70 mm of body air, ~0.21 mm of copper), and
the 3.5 mm jack J4 beside R98 (0.338 mm of body air).

CONTROL gained six courtyards and zero new violations.

### F8. Silkscreen renamed (FIXED)

Both boards now read **"WHERE THE FUZZ MEETS THE FUNK - MAIN / CONTROL  rev0.1  Illicit
Apothecary"** instead of "GLITCHWAVE 567 - ...". Font dropped 1.2 to 1.0 mm so the longer
string keeps roughly the same physical width: measured in the plotted silk gerber it spans
x 42.43 to 95.57 on a 138 mm board, against 45.69 to 92.37 before. Silkscreen-over-copper
warnings actually went **down**, 33 to 32.

**Post-second-pass DRC/ERC.** MAIN 73 violations, 6 errors (the six courtyard overlaps
above), 67 warnings, still zero shorts, zero clearance, zero unconnected. CONTROL 6
violations, 0 errors. Parity re-verified after regeneration: MAIN 297 = 297 = 297,
CONTROL 22 = 22 = 22.

### Committed

All of the above is committed as **f73cb0d** on `main` (61 files: board, schematics, BOM,
tools, regenerated fab outputs, the new release zip, the quarantined stale folder). The
eight unrelated modified files under `src/` and `docs/` were deliberately left alone and
are still uncommitted. **Nothing has been pushed.**

## 5. Third pass: every part verified against the live catalogue

### F9. All 87 LCSC part numbers looked up individually (PASSED)

The last inherited assumption in this report was "every line carries a real,
orderable MPN", which came from the README rather than from a check. Given that the
C107 defect was also an inherited assumption, it was worth actually doing.

Every distinct LCSC part number on both boards, 87 of them, was queried against the
live LCSC/JLC catalogue. **Result: all 87 are real, in stock, and the value and
package match the BOM. Zero dead part numbers, zero value mismatches.**

Thinnest stock, none of them a problem for a 5-board run:

| Part | Designators | Need (5 boards) | Stock |
|---|---|---|---|
| LM567CMX/NOPB | U5 | 5 | 562 |
| PJ-603A | J1, J2 | 10 | 518 |
| PCM12SMTR | SW1-SW3 | 15 | 441 |
| DC-044A-2.5A-2.0 | J5 | 5 | 1,210 |
| MMBTA13 | Q1 | 5 | 3,084 |

### F10. The Manufacturer column was still wrong, differently (FIXED)

The rewrite in F3 replaced substring matching with a hand-written MPN-prefix table.
That table was itself wrong on a large share of the BOM:

| Designators | MPN | Prefix table said | Catalogue says |
|---|---|---|---|
| ~40 resistor lines | 0603WAF* | UNI-ROYAL (Uniohm) | **UNI-ROYAL(Uniroyal Elec)** |
| J3, J4, J6 | PJ-3410 | CUI Devices | **XKB Connection** |
| J1, J2 | PJ-603A | CUI Devices | **HOOYA** |
| J5 | DC-044A-2.5A-2.0 | CUI Devices | **XKB Connection** |
| FB100 | HCB3216KF-601T30 | Sunlord | **TAI-TECH** |
| L100 | SMDRI127-220MT | Sunlord | **SXN(Shun Xiang Nuo Elec)** |
| C1, C3, C8, C60 | TCC0603X7R224K500CT | TDK | **CCTC** |
| C105 | 0603N332J500CT | UNI-ROYAL | **Walsin Tech Corp** |
| RV1-RV6 (CTRL) | RK09K1130A70 | ALPS | **ALPSALPINE** |
| 11 more lines | diodes, FETs, electrolytics | blank | MDD, ST(Semtech), JSMSEMI, ROQANG, Honor Elec, FOJAN, onsemi, Alpha & Omega, Jiangsu Changjing |

"UNI-ROYAL (Uniohm)" is the worst of these: it conflates two different companies
and it was on roughly half the BOM.

The generator no longer guesses. Manufacturer is read from
`kicad/tools/lcsc_verified_20260911.json`, populated from the per-part lookups, keyed
by the LCSC code already in `BOM.xlsx`. Only two parts have no LCSC number
(the Pico SC0915 and the Alpha SF12011F stomps) and both are mapped by hand.
**Every line on both boards now carries a verified manufacturer. Zero blanks.**

The lesson, for the third time on this BOM: do not infer a manufacturer from a part
number. Look it up.

### Parts cost

At LCSC qty-1 pricing, excluding the two parts PCBWay sources direct:

| | per board |
|---|---|
| MAIN | $28.34 |
| CONTROL | $4.75 |
| **one set** | **$33.10** |
| five sets | $165.49 |

Add roughly $4 for each Pico and a few dollars for each Alpha stomp. This is parts
only: bare PCB fabrication, assembly labour and PCBWay's sourcing markup are all on
top, and they come back in the manual quote.

Dearest lines per board: six LM13700 at $6.05, three Bourns trimmers at $3.44, the
MP1584 at $2.93, the LM567 at $2.19, seven TL074 at $2.05.

## 6. Still open

* **The enclosure drill template is still stale and quarantined.** Not a fab deliverable,
  so it does not block the PCB order, but no metal gets cut until a new one exists.
* **rev0.1 is still on the silkscreen.** The board has changed since that number was put
  there, but it has never been fabbed, so rev0.1 is still accurate for the first article.

## 7. Files changed today

```
kicad/glitchwave567/glitchwave567.kicad_pcb     C112 added, +5V stub, zones refilled,
                                                7 courtyards added, silk renamed
kicad/glitchwave567/power.kicad_sch             C107 -> C112; CP_Elec_10x10.2 -> CP_Elec_10x10
kicad/glitchwave567/core567.kicad_sch           CP_Elec_8x10.2 -> CP_Elec_8x10
BOM.xlsx                                        C107,C108 -> C108,C112; two footprint cells
kicad/tools/build_pcbway_boms_20260911.py       NEW, replaces the 20260905 generator
kicad/tools/build_release_package_20260911.py   NEW, same as 20260905 with the new zip name
review/tools/run_make_fab.ps1                   NEW, detached wrapper for make_fab.ps1
fab/PCBWay_BOM_main.csv, PCBWay_BOM_ctrl.csv    regenerated
fab/pos_*.csv, Centroid_*.csv, gerbers_*/, *.zip  regenerated
kicad/glitchwave567_ctrl/glitchwave567_ctrl.kicad_pcb  6 pot courtyards, silk renamed
fab/README_PCBWAY.md                            rewritten as rev 0.4
fab/Glitchwave567_PCBWay_Release_20260911.zip   NEW release package, 64 files
fab/_stale_DO_NOT_UPLOAD/                       NEW, holds the quarantined 08-02 package
```

Backups kept: `glitchwave567.kicad_pcb.bak_20260911`,
`glitchwave567.kicad_pcb.bak_20260911_withC112_prefill`, `power.kicad_sch.bak_20260911`,
`BOM.bak_20260911_precap.xlsx`, `build_pcbway_boms_20260905.py.bak_20260911`,
`README_PCBWAY.bak_20260911.md`.
