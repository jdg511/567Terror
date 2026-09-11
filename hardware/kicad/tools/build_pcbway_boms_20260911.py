"""Build PCBWay turnkey BOM CSVs from BOM.xlsx.

2026-09-11 rewrite of build_pcbway_boms_20260905.py. Three bugs fixed:

 1. TYPE was guessed from the footprint NAME and defaulted to "SMD" when nothing
    matched, which labelled all six through-hole panel jacks as SMD and the SMD
    trimmers/slide switches as THT. Now read straight from the .kicad_pcb
    footprint mounting attribute, which is ground truth.
 2. MANUFACTURER was resolved by case-insensitive SUBSTRING match against a name
    list, so "TI" matched inside "Vertical" and "Potentiometer" and "LM" matched
    inside "LM13700". Now: explicit MPN overrides, then verified MPN prefixes,
    then nothing. A blank manufacturer is safe (PCBWay sources by MPN); a wrong
    one is not.
 3. DNP rows were dropped from the CSV entirely while still appearing in the
    centroid, so PCBWay saw placements with no BOM line. Now emitted with an
    explicit DNP column.
"""
import openpyxl
import csv
import re
import os

HW = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(HW, "BOM.xlsx")
OUT_DIR = os.path.join(HW, "fab")
BOARDS = {
    "MAIN board": os.path.join(HW, "kicad", "glitchwave567", "glitchwave567.kicad_pcb"),
    "CONTROL board": os.path.join(HW, "kicad", "glitchwave567_ctrl", "glitchwave567_ctrl.kicad_pcb"),
}

# --- MPN string cleanups: BOM cell -> (clean MPN, manufacturer) -------------
# The BOM carries a few MPNs with a parenthetical vendor note glued on. PCBWay
# parses the MPN column literally, so the note has to move out of it.
MPN_OVERRIDES = {
    "BCP56 (HXY)":              ("BCP56", "HXY MOSFET"),
    "MMBTA13 (FUXINSEMI)":      ("MMBTA13", "FUXINSEMI"),
    "78L09 (UMW 78L09-150)":    ("78L09-150", "UMW"),
    "SC0915 Raspberry Pi Pico": ("SC0915", "Raspberry Pi"),
}

# --- verified MPN prefix -> manufacturer ------------------------------------
# Every entry here was checked against the live LCSC/JLC catalogue on
# 2026-09-11 or earlier. Do not add a guess to this table.
PREFIX_MFR = [
    ("PZ254V", "XFCN"), ("PM254V", "XFCN"),          # was wrongly "CVILUX", and then "TI"
    ("3224W", "Bourns"),                              # was wrongly "TI"
    ("LM13700", "Texas Instruments"),                 # was wrongly "LM"
    ("LM567", "Texas Instruments"),                   # was wrongly "LM"
    ("TL074", "Texas Instruments"),
    ("CD4051", "Texas Instruments"), ("CD4052", "Texas Instruments"),
    ("CD74HC", "Texas Instruments"), ("SN74", "Texas Instruments"),
    ("MP1584", "Monolithic Power Systems"),
    ("RK09K", "ALPS"), ("RK09D", "ALPS"),
    ("SF12", "Alpha"), ("SF17", "Alpha"),
    ("PCM12", "C&K"),
    ("PJ-", "CUI Devices"), ("DC-044A", "CUI Devices"),
    ("WS2812B", "Worldsemi"),
    ("CC0603", "Yageo"), ("CC0402", "Yageo"), ("CC0805", "Yageo"), ("CC1206", "Yageo"),
    ("RC0603", "Yageo"), ("RC0402", "Yageo"),
    ("GRM", "Murata"),
    ("CL10", "Samsung Electro-Mechanics"), ("CL21", "Samsung Electro-Mechanics"),
    ("CL31", "Samsung Electro-Mechanics"),
    ("TCC0603", "TDK"),
    ("0603WAF", "UNI-ROYAL (Uniohm)"), ("0603N", "UNI-ROYAL (Uniohm)"),
    ("HCB3216", "Sunlord"), ("SMDRI127", "Sunlord"),
    ("RVT", "Honor Elec"),                            # RVT1V... SMD electrolytics
    ("RVE", "KNSCHA"),                                # RVE100UF... SMD electrolytic
]

# Industry-standard second-sourced parts. No single true manufacturer, and a
# guessed one only misleads PCBWay. MPN alone is enough for these.
GENERIC_PREFIXES = [
    "1N4148", "1N5819", "BZT52C", "MMBT39", "MMBFJ2", "AO3401",
    "SS14", "SS34", "SMAJ", "FRC0603",
]


def clean_mpn(raw):
    raw = str(raw or "").strip()
    if raw in MPN_OVERRIDES:
        return MPN_OVERRIDES[raw]
    return raw, None


def manufacturer_for(mpn):
    up = mpn.upper()
    for pre in GENERIC_PREFIXES:
        if up.startswith(pre.upper()):
            return ""
    for pre, name in PREFIX_MFR:
        if up.startswith(pre.upper()):
            return name
    return ""


def board_mounting(pcb_path):
    """ref -> 'SMD' | 'THT', read from each footprint's own (attr ...)."""
    s = open(pcb_path, encoding="utf-8", errors="replace").read()
    out = {}
    for m in re.finditer(r'\(footprint\s+"([^"]+)"', s):
        st = m.start(); depth = 0; i = st
        while i < len(s):
            c = s[i]
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        blk = s[st:i + 1]
        r = re.search(r'\(property "Reference"\s+"([^"]*)"', blk)
        a = re.search(r'\n\t\t\(attr ([^\)]*)\)', blk)
        if not r:
            continue
        attr = (a.group(1) if a else "")
        out[r.group(1)] = "THT" if "through_hole" in attr else "SMD"
    return out


def process(wb, sheet, pcb_path, out_path):
    ws = wb[sheet]
    hdr = {c.value: i for i, c in enumerate(ws[1])}
    mount = board_mounting(pcb_path)
    rows, line = [], 1
    notes = []
    for row in ws.iter_rows(min_row=2, max_row=ws.max_row, values_only=True):
        refs = row[hdr["Refs"]]
        if refs is None:
            continue
        des = [d.strip() for d in str(refs).split(",") if d.strip()]
        qty = row[hdr["Qty"]]
        value = row[hdr["Value"]] or ""
        fp = row[hdr["Footprint"]] or ""
        dnp = bool(row[hdr["DNP"]])
        mpn, mfr_override = clean_mpn(row[hdr["MPN"]])

        if dnp:
            rows.append({"Line#": line, "Qty": qty, "Designator": ",".join(des),
                         "MPN": "DNP", "Manufacturer": "",
                         "Description": "DO NOT POPULATE - leave pads bare (%s)" % value,
                         "Package": fp, "Type": mount.get(des[0], "SMD"), "DNP": "DNP"})
            line += 1
            notes.append("DNP kept in BOM so it matches the centroid: %s" % ",".join(des))
            continue

        mfr = mfr_override or manufacturer_for(mpn)
        types = {mount.get(d) for d in des}
        if None in types:
            notes.append("NOT ON BOARD: %s" % ",".join(d for d in des if d not in mount))
            types.discard(None)
        if len(types) > 1:
            notes.append("MIXED mounting in one row (%s): %s" % (",".join(des), types))
        part_type = (types.pop() if types else "SMD")

        if qty is not None and len(des) != int(qty):
            notes.append("QTY MISMATCH row %s: qty=%s but %d designators" % (line, qty, len(des)))

        rows.append({"Line#": line, "Qty": qty, "Designator": ",".join(des), "MPN": mpn,
                     "Manufacturer": mfr, "Description": value, "Package": fp,
                     "Type": part_type, "DNP": ""})
        line += 1

    fields = ["Line#", "Qty", "Designator", "MPN", "Manufacturer", "Description", "Package", "Type", "DNP"]
    with open(out_path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)

    placed = {d for r in rows for d in r["Designator"].split(",")}
    on_board = set(mount)
    only_board = sorted(on_board - placed)
    only_bom = sorted(placed - on_board)
    return rows, notes, only_board, only_bom


wb = openpyxl.load_workbook(SRC, data_only=True)
for sheet, out in (("MAIN board", "PCBWay_BOM_main.csv"), ("CONTROL board", "PCBWay_BOM_ctrl.csv")):
    rows, notes, only_board, only_bom = process(wb, sheet, BOARDS[sheet], os.path.join(OUT_DIR, out))
    blank = [r["Designator"] for r in rows if not r["Manufacturer"] and r["MPN"] != "DNP"]
    print("%-14s -> %-22s %3d lines, %3d designators" %
          (sheet, out, len(rows), sum(len(r["Designator"].split(",")) for r in rows)))
    print("    on board but not in BOM : %s" % (only_board or "none"))
    print("    in BOM but not on board : %s" % (only_bom or "none"))
    print("    blank Manufacturer (generic/multi-source, MPN is sufficient): %d lines" % len(blank))
    for n in notes:
        print("    NOTE:", n)
