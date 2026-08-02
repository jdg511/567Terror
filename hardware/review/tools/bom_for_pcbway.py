"""Convert BOM.xlsx into the column layout PCBWay's assembly quote expects.

PCBWay's PCBA BOM template wants, per line: Item, Designator(s), Qty, Manufacturer
Part Number, Manufacturer, Description/Value, Package/Footprint, Mounting type,
Supplier, Supplier Part Number, and a DNP/DNI flag. Our sheet has most of that in
different column names, so this maps it and flags anything unquotable.

Writes fab/PCBWAY_SUBMISSION/BOM_<board>_pcbway.csv and prints any line PCBWay
could not act on (no MPN, or an MPN that is a generic description rather than a
buyable part number).

Usage: bom_for_pcbway.py
"""
import csv
import os
import re

import openpyxl

HW = r"C:\Users\Jason\source\repos\Glitchwave\hardware"
SRC = os.path.join(HW, "BOM.xlsx")
OUT = os.path.join(HW, "fab", "PCBWAY_SUBMISSION")

SHEETS = {"MAIN board": "main", "CONTROL board": "ctrl"}

HEADER = ["Item", "Designator", "Qty", "Manufacturer Part Number", "Manufacturer",
          "Description / Value", "Package / Footprint", "Mounting", "Supplier",
          "Supplier Part Number", "DNP", "Notes"]

# An MPN that is really a category description, not something a buyer can order.
GENERIC = re.compile(
    r"^(generic|smd |mlcc|2\.54 socket/header|socket/header|tbd|\?|-)?$|"
    r"(socket/header|generic|MLCC|SMD alu)", re.I)


def mounting(footprint):
    f = (footprint or "").lower()
    if any(k in f for k in ("0603", "0805", "1206", "soic", "sot", "sod", "sma",
                            "plcc", "qfn", "tssop", "dfn", "2012", "1608", "3216")):
        return "SMT"
    return "THT"


def main():
    os.makedirs(OUT, exist_ok=True)
    wb = openpyxl.load_workbook(SRC)
    problems = []

    for sheet, tag in SHEETS.items():
        ws = wb[sheet]
        rows = list(ws.iter_rows(values_only=True))
        hdr = [str(c or "").strip() for c in rows[0]]

        def col(*names):
            for n in names:
                if n in hdr:
                    return hdr.index(n)
            return None

        c_qty = col("Qty")
        c_ref = col("Refs", "Ref", "Designator")
        c_val = col("Value")
        c_fp = col("Footprint")
        c_mpn = col("MPN")
        c_src = col("Source")
        c_pn = col("Part # (LCSC C# / DK)", "Part #")
        c_note = col("Notes / verification", "Notes")
        c_dnp = col("DNP")

        out_path = os.path.join(OUT, "BOM_%s_pcbway.csv" % tag)
        n = 0
        with open(out_path, "w", newline="", encoding="utf-8-sig") as fh:
            w = csv.writer(fh)
            w.writerow(HEADER)
            for r in rows[1:]:
                if not r or not r[c_ref]:
                    continue
                n += 1
                mpn = str(r[c_mpn] or "").strip()
                supplier = str(r[c_src] or "").strip()
                spn = str(r[c_pn] or "").strip()
                fp = str(r[c_fp] or "").strip()
                dnp = str(r[c_dnp] or "").strip()
                w.writerow([n, r[c_ref], r[c_qty], mpn, "", r[c_val], fp,
                            mounting(fp), supplier, spn, dnp,
                            str(r[c_note] or "")])
                if GENERIC.match(mpn) or GENERIC.search(mpn) or not spn \
                        or spn.lower() == "generic":
                    problems.append((tag, r[c_ref], r[c_val], mpn, spn))
        print("wrote %-38s %d lines" % (os.path.basename(out_path), n))

    print()
    if problems:
        print("LINES PCBWAY CANNOT QUOTE AS WRITTEN (no buyable part number):")
        for t, ref, val, mpn, spn in problems:
            print("   %-5s %-18s %-28s MPN=%-30r supplier PN=%r"
                  % (t, ref, str(val)[:28], mpn, spn))
    else:
        print("every line has a buyable part number")


if __name__ == "__main__":
    main()
