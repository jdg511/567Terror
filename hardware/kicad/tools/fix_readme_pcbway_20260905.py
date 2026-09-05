import sys, shutil

path = "README_PCBWAY.md"
backup = "README_PCBWAY.bak_20260905.md"
shutil.copy(path, backup)

text = open(path, encoding="utf-8").read()
orig_len = len(text)


def must_replace(text, old, new, label):
    n = text.count(old)
    if n != 1:
        print(f"FAIL {label}: found {n} occurrences (expected 1)")
        sys.exit(1)
    return text.replace(old, new)


old_block = '''2. **Stomp switches SW1/SW2 — mark DNP, do NOT ask PCBWay to source them.**
   Suntsu SSWFS-S01-AC09-HWH is a Digi-Key *Marketplace* listing, Mouser has
   no listing for it or its wire variant, and **JLCPCB has no foot-switch
   category at all** — no fab-side distributor stocks this class of part.
   It is a commodity pedal-industry switch: Amplified Parts **P-H604**
   publishes matching dimensions (12 mm × 1 mm bushing, 14.3 mm long, body
   25.3 × 12.6 mm) against this footprint's Ø12.2 hole and 25.5 × 13 body,
   and Love My Switches stocks PCB-mount ($3.99) and pre-wired ($5.75)
   versions. Buy them direct and fit them at final assembly. This takes the
   only unsourceable line off the fab order's critical path.
   ⚠️ Two numbers still unverified: the below-panel body depth (ENCLOSURE_FIT
   assumes 15.0 ± 1.0 mm — measure a real one before cutting metal), and the
   footprint `descr` says thread "15/32-32" while ENCLOSURE_FIT and Amplified
   Parts both say M12 × 1.0. The Ø12.2 clearance hole suits either; the nut
   does not.'''

new_block = '''2. **Stomp switches SW1/SW2 — Alpha SF12011F-0102-20R-M-011, no longer DNP.**
   2026-09-05: replaced the unsourceable Suntsu SSWFS-S01 with Alpha's
   SF12011F-0102-20R-M-011 (Taiwan), a genuine PCB-mount (PC-pin) SPDT
   momentary footswitch from their SF12 series. PCBWay turnkey should be
   able to source and place this by MPN — no DNP flag, no direct-buy
   workaround needed. Both switches stay MOMENTARY (not latching) to
   preserve the STARVE gesture (both held) in the firmware.
   Verified from Alpha's own SF12 series datasheet (outline drawing, page
   3, SF12011F-0102-20R-M-011 entry): terminals in a straight row, 2.5 mm
   pitch, ~1.0 mm pin width; pinout is Pin 1 = N.O., Pin 2 = COM, Pin 3 =
   N.C. (wiring diagram: PUSH 1-2 ON, FREE 2-3 ON). Thread is M12 × 0.75
   (vs. the old SSWFS-S01's M12 × 1.0) — the Ø12.2 clearance hole is
   unaffected since nominal M12 OD doesn't depend on thread pitch, so no
   enclosure change is needed. Body 13.4 mm dia., ~12 mm deep below panel —
   fits the existing ENCLOSURE_FIT 15.0 ± 1.0 mm depth budget with margin.
   The footprint, schematic pinout, and PCB routing at SW1/SW2 were all
   updated to match; DRC and ERC are both clean (0 errors) as of
   2026-09-05.'''

text = must_replace(text, old_block, new_block, "stomp switch section")

# Refresh the DRC status line to reflect the 2026-09-05 CONTROL board re-check.
old_drc = '''DRC status (KiCad 10, **re-run 2026-08-02 against the current boards**):
**0 unconnected, 0 shorts, 0 clearance violations, 0 schematic-parity errors**
on both boards. MAIN = 72 warnings + 4 courtyard errors; CONTROL = 6 warnings,
0 errors. The 4 courtyard errors are the same shallow decoupling-cap kisses
(≤0.4 mm, bodies verified clear) — no action needed.'''

new_drc = '''DRC status (KiCad 10): MAIN last re-run 2026-08-02 — 72 warnings + 4
courtyard errors (same shallow decoupling-cap kisses, ≤0.4 mm, bodies
verified clear — no action needed), 0 shorts/clearance/schematic-parity
errors. CONTROL last re-run 2026-09-05 (after the SW1/SW2 switch swap and a
zone refill) — 6 warnings, **0 errors**.'''

text = must_replace(text, old_drc, new_drc, "DRC status line")

open(path, "w", encoding="utf-8").write(text)
print("OK, wrote", len(text), "bytes (was", orig_len, "), backup at", backup)
