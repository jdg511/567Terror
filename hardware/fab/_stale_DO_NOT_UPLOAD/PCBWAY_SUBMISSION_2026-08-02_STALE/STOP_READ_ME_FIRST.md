# I did not place the order — here is exactly why

**2026-08-02.** You asked me to send everything to PCBWay and set up a 5-off PCB +
assembly order. I stopped before submitting anything. Four things block it, and three of
them are real design gaps rather than procedural caution. None of them is hard to fix.

The bare-PCB *files* are fine and current. The problems are mechanical and BOM-side.

---

## 1. ⛔ Neither board has a single mounting hole

I checked both board files: **no mounting-hole footprints, no non-plated screw holes, no
internal cutouts.** The only NPTH holes on the main board are the DC jack's nose and the
slide-switch pegs.

That matters because of what `ENCLOSURE_FIT.md` says:

> *"There are no PCB bosses on the floor… **Control-board retention must come from
> standoffs.**"*

and, in its own **"Required changes before ordering"** list:

> *"2. **CONTROL board: mounting holes** for 4× M3 standoffs … set for 8.0 mm
> face-to-board-top."*

**That change was never made.** And the pots can't hold the board either — they're
snap-in with no nuts (*"shafts pass through plain Ø7 enclosure holes, no nuts"*). So as
the files stand, **the control board has nothing holding it in the enclosure.**

If you order now you get five control boards that can't be mounted.

The main board is arguably OK — six jack bushings with nuts through the walls will retain
it — but it also has no holes, so that's worth a conscious decision rather than an
accident.

## 2. ⛔ The two stomp cutouts from ENCLOSURE_FIT were never added

Same list, item 1:

> *"**CONTROL board: two cutouts** ~27 × 14.5 mm at the stomp positions (34, 96) and
> (104, 96) so the panel-mounted bodies pass through."*

Not in the board — Edge.Cuts has the outline and the corner notches, nothing internal.

**But you may not need them.** Those cutouts are only required on the *wire-lead stomp*
path (SSWFS-S01-AE11). I found the **PC-pin variant in stock** at Love My Switches
($3.99), and on that path the stomp bodies sit on the control board as originally drawn
and no cutouts are needed. **This is a fork in the road that has never been closed out** —
and it's coupled to item 1, because the stomp choice changes how the board is held.

## 3. ⛔ The board-to-board connector still has no part number

BOM CONTROL row 3 is `2.54 socket/header 2x8`, MAIN J10 is `2.54 pin header 2x8`, both
supplier PN `generic`. PCBWay cannot quote or buy either.

It's not just "pick any header" — the stack is spec'd at **11 mm board-to-board**
(ENCLOSURE_FIT's z-table: *"9.6 → 20.6 | 11 mm board-to-board header stack"*). A standard
male header's 2.5 mm insulator plus a standard 8.5 mm female socket body happens to come
to exactly 11 mm, which is almost certainly the intent — but that needs confirming against
real parts before it's ordered, because getting it wrong means the two boards don't sit at
the height the enclosure expects.

## 4. ⚠️ The BOM is far less complete than the fab README claims

The README says *"every BOM line carries an LCSC C-number (stock verified 2026-07-25)"*.
**It doesn't.** I converted both sheets to PCBWay's assembly format and **63 of 96
main-board lines and 3 of 11 control-board lines have supplier part number `generic`** —
every resistor, every capacitor, the ferrite bead and both connectors.

For most of them that's genuinely fine: turnkey houses stock 0603 resistors and MLCCs and
will supply them from value + package + tolerance, usually cheaper than a named part.
**But a few need real numbers or you'll get the wrong part:**

- **C104 "10u/50V X7R"** — the 50 V rating and X7R dielectric are the whole point (your own
  BOM note says DC-bias derating at 18 V). A generic 10 µF 0603 in Y5V would wreck the
  buck's input filtering.
- **The electrolytics** — C48 470u, C100 220u/35V, C47 22u, C109/C110 47u, C2 4.7u,
  C75/C90 10u, ctrl C7 100u. Case size and voltage must match the specific footprints
  (`CP_Elec_8x10.2`, `CP_Elec_6.3x5.4`, …) or they won't physically fit.
- **FB100** — ferrite bead 600 Ω @ 100 MHz, **3 A**. The current rating matters; it carries
  the whole board's supply.

I can backfill real LCSC numbers for those in a few minutes — say the word.

---

## What IS ready

- Both boards pass DRC: **0 shorts, 0 unconnected, 0 clearance, 0 schematic-parity errors.**
- Gerbers, drill files, drill maps and position files regenerated 2026-08-02 from the
  current boards, verified to contain every recent change.
- `BOM_main_pcbway.csv` and `BOM_ctrl_pcbway.csv` in this folder — your BOM remapped into
  PCBWay's assembly column layout (Item / Designator / Qty / MPN / Description / Package /
  Mounting / Supplier / Supplier PN / DNP / Notes).
- Full engineering review in `hardware/review/schematic_review.md`.

**I have deliberately NOT zipped a final submission package yet**, because if you add
mounting holes or cutouts the gerbers change — and shipping a stale package is exactly the
mistake I spent this session cleaning up.

## One more thing: I can't log in as you

pcbway.com currently shows a signed-out session. I can't create an account or enter a
password on your behalf, and I won't submit an order or enter payment details for you.
Once the four items above are settled, I can prepare and stage everything so that placing
the order is a short, checked sequence you approve — but the account and the payment step
have to be you.

---

## Suggested order of operations

1. Decide the stomp path — **PC-pin (no cutouts, simpler)** or wire-lead (needs the two
   cutouts). I'd take PC-pin: it's in stock and it deletes item 2 entirely.
2. Decide control-board retention — 4× M3 standoff holes is what ENCLOSURE_FIT assumes.
   Tell me the hole positions or let me propose four that clear the copper.
3. Say yes to a header/socket pair once I've confirmed one that gives 11 mm.
4. Let me backfill the ~10 BOM lines that genuinely need real part numbers.
5. I regenerate the fab package, zip it, and stage the whole submission.
6. You log in and place it.

Realistically that's one short conversation plus about half an hour of my time.
