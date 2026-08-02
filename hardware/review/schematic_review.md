# Glitchwave 567 — schematic & board review, 2026-08-02

**Verdict: the PCBs are ready to fabricate. The turnkey BOM has one unspecified line
(the board-to-board connector) that must be filled in before you can place a full
turnkey order.**

The single most important thing this review found was not a circuit error: **the fab
package in `hardware/fab/` was four rounds of board changes out of date.** It has been
regenerated. Details in §1.

| | MAIN | CONTROL |
|---|---|---|
| DRC violations | 72 | 6 |
| DRC errors | 4 (documented cap kisses) | **0** |
| unconnected / shorts / clearance | **0 / 0 / 0** | **0 / 0 / 0** |
| schematic↔PCB parity errors | **0** | **0** |
| ERC errors | 3 (all false positives, §5) | **0** |
| duplicate track segments | **0** (was 33) | 0 |
| footprint body overlaps involving a jack/header | **0** | **0** |
| netless copper pads | **0** (4 paste apertures excluded) | **0** |

---

## 1. ⛔ The fab package was stale — regenerated · **BLOCKER, fixed**

`hardware/fab/` was built **2026-07-25**. The boards were last modified **2026-08-02**.
Everything from the last two sessions was missing from the gerbers, drill files and
position files:

- the input jack J1 moving +3.5 mm to clear the buck inductor
- the output jack J2 clearance fix (four resistors moved)
- the control-board stack header moving to B.Cu with the mirrored footprint
- the MP1584 exposed pad being connected (§2)

**Ordering from the old package would have built the July board** — including a jack
fouling a 12 mm inductor, and a control-board header that cannot mate.

**Fixed:** everything regenerated from the current `.kicad_pcb` files. Verified in
`pos_main.csv` (J1 at x = 113.5, R1 at 122.4, R107/109/111 at +2.0, R144 at −2.5) and
`pos_ctrl.csv` (J1 `side=bottom`, footprint `…_Mirrored`).

There was **no generation script** — that is why it went stale silently. There is one now:
`hardware/review/tools/make_fab.ps1`. It refills zones before plotting (`--check-zones`),
so gerbers can never come from a stale fill. **Re-run it after any board edit.**

### ⛔ The enclosure drill template is wrong and has been quarantined
`drill_template_1to1.pdf` (2026-07-25) still shows the input jack at its old position.
Renamed to `drill_template_1to1.STALE_2026-07-25_DO_NOT_USE.pdf`. It was never a fab
deliverable, so it does not block the PCB order — but **do not drill an enclosure from
it.** A new template is needed; I did not generate one because getting it wrong ruins a
box, and it needs checking against ENCLOSURE_FIT's panel geometry rather than inferring.

---

## 2. ⛔ MP1584 exposed thermal pad was not connected · **HIGH, fixed**

**Evidence:** U19's pad "9" returned an empty net (netcode 0) in the raw board file. The
symbol has only 8 pins, so nothing ever drove it. Neither DRC nor ERC can see this — there
is no 9th pin to report as unconnected.

**Why it matters.** MPS's datasheet (Rev 1.0, PIN FUNCTIONS p.4) gives pin 5 as a single
row: **"GND / Exposed Pad — … Connect exposed pad to GND plane for optimal thermal
performance."** GND and the pad are the same node. For a 3 A switcher with θJA = 50 °C/W
this is the primary heat path. Unconnected means no heatsinking, no GND-pour stitching,
and no netlist instruction telling the assembler the pad matters.

**Fix applied:** the pad was renumbered to **"5"** and set to net GND, with a **solid**
zone connection rather than thermal-relief spokes (spokes would throttle the heat path we
are fixing). This needs no symbol edit and no netlist re-import, and DRC confirms
**parity = 0** afterwards.

⚠️ **It lives in the board's embedded footprint copy.** If you ever run *Update Footprints
from Library* on U19 it will revert. Re-run `review/tools/q.py main ep` afterwards to check.

The four other netless pads under U19 are **F.Paste-only apertures** — the standard
four-window paste pattern that stops the part floating on a solder bump during reflow.
Correct as drawn; not a defect.

---

## 3. Six pot frames were floating · **MEDIUM, fixed**

RV1–RV6 on the control board (ALPS RK09K1130) each had two "MP" snap-in mounting legs with
no net, leaving each pot's metal frame floating right next to the audio path. All six pots
have pin 3 = GND (verified across all six), so the frames were tied there by the same pad
renumbering method. Control board now has **zero netless copper pads**.

---

## 4. Power-path review

**Topology traced from the netlist:** DC jack J5 → P-FET reverse protection Q10 → ferrite
FB100 → `VA` (the raw, unregulated analog rail) → everything.

| Item | Finding | Result |
|---|---|---|
| Reverse-polarity P-FET | Q10 AO3401A **drain to input, source to load, gate pulled to GND by R100 100 k** — the correct high-side topology | **pass** |
| Gate-source clamp | **D100 = BZT52C10, a 10 V zener** across VPROT→QGATE. AO3401A VGS max is ±12 V, so the clamp holds VGS at −10 V with 2 V margin even at an 18 V input | **pass — good design** |
| Input TVS | D106 SMAJ18A, VA→GND. VRWM 18 V, VC ≈ 29.2 V. That clamp voltage is **deliberately** under the 30 V abs max of *both* the MP1584 and the AO3401A | **pass** — see the note below |
| Catch diode | D101 SS34, 40 V/3 A — matches MPS's recommended B340A class | **pass** |
| Buck output | R104 40.2 k / R105 7.68 k → **4.99 V**; bottom resistor 7.68 k is well under MPS's 40 kΩ ceiling | **pass** |
| +5 V → Pico | D90 SS14 into VSYS with C90 10 µF — the recommended Pico powering scheme | **pass** |
| 3V3 | generated by the Pico's own regulator; total external load ≈ 2 mA (six 10 k pots + a 4067) | **pass** |
| DC jack polarity | J5 pin 1 → GND, pin 2 → V+ = centre-negative, matching the schematic note | **pass**, but see below |

⚠️ **I previously suggested swapping the SMAJ18A for an SMAJ20A. That was wrong** — an
SMAJ20A clamps at ≈32.4 V, which would *exceed* the 30 V limits of the MP1584 and the
AO3401A. The SMAJ18A is the correct part. The right way to buy margin is to **rate the
input at 15 V rather than 18 V**, so an in-tolerance adapter never sits at the TVS's
standoff voltage.

⚠️ **`manual_review` — DC jack pin roles.** The DC-044A footprint's own `descr` says
*"Pin roles inferred (plain+ctr=tip, shrapnel=sleeve, tab=switch) — DMM-VERIFY before
power."* Meter a real jack before first power-on. The P-FET protects against getting it
backwards, which is why this is a check and not a blocker.

⚠️ **`manual_review` — J5's two mounting lugs are netless and I deliberately left them
that way.** On many DC-044A variants the mounting lugs are part of the sleeve contact,
which on this centre-negative design is **+V**. Grounding them would short the supply.
Same reasoning for SW1–SW3's four mounting lugs each. Meter them, then decide.

### The input voltage question — 9 V works, but 12 V is the honest spec
Three independent, datasheet-verified constraints all point the same way:

1. **LM13700 recommended single supply is 9.5 V–32 V** (SNOSBW2F §6.2). At a 9 V adapter,
   VA sits below the minimum.
2. **MP1584 turn-on** needs EN ≥ 1.65 V worst case. The divider (100 k/24.9 k — *exactly*
   MPS's Figure 4, which MPS labels **"8 V–28 V"**) means the buck drops out below
   **VA ≈ 8.27 V**. Do not "fix" this divider: it deliberately programs UVLO to
   ≈ VOUT + 2.5 V per the datasheet's "VIN − VOUT > 3 V" bootstrap rule.
3. **TL074 input common-mode must stay ≥ (V−) + 4 V** for the SOIC-14 D package
   (SLOS080W §5.3). With VREF = VA/2, a 9 V rail puts VREF at 4.5 V — a signal swinging
   ±3 V takes the input down to **1.5 V, well below the 4 V floor.** TI documents freedom
   from phase inversion only for the newer TL07xH die, not for a plain TL074C.

You said 9 V is not an issue, and the pedal will run at 9 V. Point 3 is new information
though: the symptom would be odd distortion or momentary dropout **on loud notes only**,
which is exactly the kind of fault that takes weeks to chase. At 12 V all three constraints
clear comfortably. **Recommendation: label the pedal 12 V, keep the 9 V compatibility as
"works, with reduced headroom".**

---

## 5. IC reviews — the ERC errors are all false positives

**ERC main board: 18 violations, 3 errors.** All three are false positives, now with
datasheet backing:

- **U10 pins 5+12 and U15 pins 5+12 tied together** ("Output and Output connected"). Both
  are LM13700s, and those are the two OTA **current** outputs summed into one node. The
  datasheet describes a push-pull *current* output (§7.3.1) that tolerates a **continuous
  short to ground** (§7.1). Currents simply add; peak output is only 500 µA typ. **Sound
  design — waive the ERC rule.**
- **U5 (LM567) pin 4 "power pin not driven."** V567 comes from the L78L09 via D105; the
  regulator symbol's output is not typed as a power output, so ERC cannot see a driver.
  **Add a PWR_FLAG** to silence it.
- 15 warnings are cached symbol/library mismatches plus two electrolytic footprints missing
  from `Capacitor_SMD`. Harmless now; a schematic→board re-sync would trip on them.

**LM13700 unused-pin treatment is correct on all six parts**, checked against TI's own
Electrical Characteristics test conditions:

| Rule (TI SNOSBW2F §6.4) | Board |
|---|---|
| Diode bias pins 2 & 15 **open** when linearising diodes unused | ✅ all six |
| Unused buffer input **grounded**, output **open** | ✅ 11 of 12 — **U12 pin 10 is floating** |
| Unused OTA half disabled by tying IABC to V− | ✅ U3/U7/U12 pin 16 |
| IABC driven through a series resistor, never a bare voltage | ✅ R48/R76/R79 etc. |
| Buffer emitter load resistor to V− when the buffer *is* used | ✅ **R98/R99, 10 k, and literally labelled "buffer DC load"** |

**U12 pin 10 (Buffer Input B) floating · low · not fixed.** TI's characterisation condition
grounds unused buffer inputs. With the buffer *output* also open there is no DC path, so
there is no current-draw or latch hazard — this is a tidiness deviation, not a fault. I
left it alone deliberately: editing the schematic would invalidate the freshly regenerated
fab package for zero functional gain. **Next time you open `mix_svf.kicad_sch`: delete the
no-connect on U12 pin 10 and drop a GND symbol on it.**

**TL074 unused sections are correctly terminated** — U6C/U6D, U16C/U16D, U2C/U2D are all
unity-gain followers with their inputs on VREF, which is the right choice (grounding them
would sit 4 V below the recommended common-mode floor).

**MP1584 compensation deviates from MPS Table 3** — R102 = 51 k / C105 = 3n3 against the
recommended 100 k / 150 pF. Working MPS's formulas backwards gives a **~32 kHz crossover
and a ~950 Hz zero**, i.e. a slower, more heavily damped loop than the reference. That errs
safe and will not oscillate, but it was not derived from the datasheet procedure.
**Do a load-step test at bring-up** (0.1 A → 1 A, scope the 5 V rail).

Full per-IC detail: `review/review_outputs/U19_MP1584EN.md`.
Datasheet contracts: `review/datasheet_cache/MP1584EN.summary.md`, `LM13700.summary.md`.

---

## 6. Warnings that do not block the order

**L78L09 has no output capacitor.** `/power/V9RAW` has exactly two nodes: U18 pin 1 and
D105's anode. The first capacitance is C40/C103/C49 on V567, on the far side of a diode.
Adding a 100 nF directly on V9RAW would be textbook. Not fixed — it needs a new component
and a board change, which would restart the fab regeneration. **Your call whether it is
worth a respin; the LM567's own rail is well decoupled, so the practical risk is low.**

**27 non-standard reference designators.** The `/output/` sheet carries R100A…R127A and
C72A. This is what makes `kicad-cli` print *"schematic has annotation errors"*. Two real
consequences: `R100` (100 k, power entry) and `R100A` (39 k, output stage) are confusable
during hand assembly and first-article inspection; and if you ever hit **Annotate** in the
schematic editor, KiCad will renumber all 27 and the board will no longer match. Fixing it
means a coordinated renumber across schematic + PCB + BOM + fab — mechanical and
scriptable, but not worth doing between now and an order. **Don't press Annotate.**

**The schematics carry no part metadata.** Every component's `Datasheet` field is empty and
there are no MPN or LCSC fields — all of that lives only in `BOM.xlsx`. So
`kicad-cli sch export bom` cannot produce an orderable BOM and there is no schematic→part
traceability. Survivable (PCBWay works from the spreadsheet) but worth injecting from the
BOM one day.

**15 footprints still have no courtyard on the boards** — J1–J6 and U20 on main, RV1–RV6
and SW1/SW2 on control. This is the root cause of *both* jack collisions going unnoticed
for months: DRC was silently measuring a 14 × 25 mm jack body as its 8.6 × 17.75 mm pad
box. The library files all have courtyards now; *Tools → Update Footprints from Library*
pushes them in. **But see the warning in §2 — that operation would also revert the U19
exposed-pad fix.** Do them together and re-check.

---

## 7. Verification basis

**Tooling.** The skill's helper scripts (`inventory_project.py`, `extract_kicad_sch.py`,
`build_review_context.py`, `datasheet_tool.py`) are **not installed** — only `SKILL.md` is
present. Equivalent tooling was written for this review and left in `hardware/review/tools/`.
Notably, instead of parsing `.kicad_sch` directly (which the skill's own extractor warns
cannot follow hierarchical sheets — and this project has eight), the factual basis is
**KiCad's own computed netlist** via `kicad-cli sch export netlist --format kicadxml`. That
is the same connectivity the PCB was built from.

**Ran:** netlist export and extraction for both boards (296 components / 253 nets on main,
22 / 19 on control — component counts cross-checked against the PCB footprint counts);
`kicad-cli pcb drc` and `sch erc` on both; a whole-board duplicate-segment and body-overlap
audit; a pad-vs-symbol-pin sweep that found the exposed-pad defect; raw `.kicad_pcb` pad
inspection to confirm every high-severity claim.

**Datasheets verified from primary sources:** MP1584 (MPS Rev 1.0, 8/8/2011), LM13700
(TI SNOSBW2F Rev Nov 2015), TL07xx (TI SLOS080W Rev Jul 2025). Summaries in
`review/datasheet_cache/`.

**Not verified / skipped:**
- No datasheet review of the CD4051B, CD4052B, CD74HC4067, TL074 *instances* beyond the
  supply/common-mode rules, MMBFJ201, MMBT3904, MMBTA13, BCP56, 74AHCT1G125, WS2812B or
  the Pico module. Their connections look consistent but are **schematic-consistency
  observations, not datasheet verification**.
- **All layout-quality checks are `manual_review`**: buck loop area, FB routing away from
  SW, thermal via placement under U19, ground-plane integrity, EMI. These need a visual
  pass in pcbnew against MPS's layout guide (p.16), not a netlist.
- The LM567's own external network (RT/CT, output filter) was not checked against its
  datasheet.
- No thermal calculation for Q11 (BCP56), which is a linear pass element in the "starve"
  circuit.

---

## 8. What is actually left before you order

| # | Item | Blocks order? |
|---|---|---|
| 1 | **Pick the actual header + socket pair for the board-to-board connector.** BOM CONTROL row 3 still says generic `2.54 socket/header 2x8` — PCBWay cannot buy that. Main J10 takes one half, control J1 (now on B.Cu) the other. | **YES — turnkey BOM** |
| 2 | Mark SW1/SW2 **DNP** on the PCBA BOM and buy the stomps direct (no distributor stocks them) | yes, if left as-is |
| 3 | Decide the input voltage label — 12 V recommended, 9 V works with reduced headroom | no |
| 4 | Meter a real DC-044A to confirm pin roles before first power-on | no — bring-up |
| 5 | Load-step test the 5 V rail at bring-up | no — bring-up |
| 6 | Regenerate the enclosure drill template before cutting metal | no — not a fab item |
