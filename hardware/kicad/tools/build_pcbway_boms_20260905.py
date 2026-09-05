import openpyxl
import csv
import re
import os

SRC = "BOM.xlsx"
OUT_DIR = os.path.join("fab")

KNOWN_MANUFACTURERS = [
    "Alpha", "ALPS", "Yageo", "Murata", "Vishay", "KEMET", "TDK", "Panasonic",
    "onsemi", "ON Semi", "Texas Instruments", "TI", "STMicro", "STMicroelectronics",
    "Raspberry Pi", "Diodes Inc", "Diodes Incorporated", "Nexperia", "Littelfuse",
    "Bourns", "Microchip", "Infineon", "ROHM", "Nichicon", "Rubycon", "Wurth",
    "Würth", "AVX", "Samsung", "Taiyo Yuden", "Amphenol", "Molex", "JST",
    "TE Connectivity", "Fairchild", "Toshiba", "Renesas", "MPS", "Monolithic Power",
    "Analog Devices", "ADI", "Maxim", "Linear Technology", "LM", "E-Switch",
    "C&K", "Omron", "NKK",
]

SMD_FOOTPRINT_HINTS = [
    "0201", "0402", "0603", "0805", "1206", "1210", "SOT", "SOIC", "SOP", "QFN",
    "QFP", "TQFP", "MSOP", "SSOP", "DFN", "BGA", "SMD", "SMA", "SMB", "SMC",
    "MELF", "PowerPAK", "DPAK", "D2PAK", "TO-252", "TO-263", "WSON", "LGA",
]
THT_FOOTPRINT_HINTS = [
    "THT", "TH_", "Radial", "Axial", "DIP", "TO-92", "TO-220", "TO-247",
    "PinHeader", "PinSocket", "Screw_Terminal", "1590", "SPDT", "SF12",
    "SSWFS", "Potentiometer", "RK09", "Connector_PinHeader", "Barrel_Jack",
    "Push", "Switch", "CUI", "Terminal_Block",
]


MPN_PREFIX_MANUFACTURERS = [
    # (prefix, manufacturer) -- generic JLC/LCSC passive part-numbering conventions
    ("CC0603", "Yageo"), ("CC0402", "Yageo"), ("CC0805", "Yageo"), ("CC1206", "Yageo"),
    ("RC0603", "Yageo"), ("RC0402", "Yageo"), ("RC0805", "Yageo"), ("RC1206", "Yageo"),
    ("GRM", "Murata"), ("RK09K", "ALPS"), ("RK09D", "ALPS"),
    ("PZ254V", "CVILUX"), ("PM254V", "CVILUX"),
    ("SF12", "Alpha"), ("SF17", "Alpha"),
    ("0603WAF", "UNI-ROYAL (Uniohm)"), ("0603N", "UNI-ROYAL (Uniohm)"),
    ("CL10", "Samsung Electro-Mechanics"), ("CL21", "Samsung Electro-Mechanics"),
    ("CL31", "Samsung Electro-Mechanics"),
    ("TCC0603", "TDK"),
    ("TL074", "Texas Instruments"), ("CD4051", "Texas Instruments"),
    ("CD4052", "Texas Instruments"), ("CD74HC", "Texas Instruments"),
    ("SN74", "Texas Instruments"),
    ("MP1584", "Monolithic Power Systems"),
    ("WS2812B", "Worldsemi"),
    ("PCM12SMTR", "C&K"),
    ("PJ-", "CUI Devices"),
    ("DC-044A", "CUI Devices"),
    ("HCB3216", "Sunlord"),
    ("SMDRI127", "Sunlord"),
]

# Generic / second-sourced industry-standard part numbers with no single true
# manufacturer -- deliberately left without a guessed Manufacturer rather than
# assert a wrong one. MPN alone is enough for PCBWay to source these.
GENERIC_MULTISOURCE_PREFIXES = [
    "1N4148", "1N5819", "BZT52C", "MMBT39", "MMBFJ2", "MMBTA", "AO3401",
    "SS14", "SS34", "SMAJ", "BCP56",
]


def guess_manufacturer(mpn, value, footprint):
    text = f"{mpn} {value} {footprint}"
    for name in KNOWN_MANUFACTURERS:
        if name.lower() in text.lower():
            return name
    mpn_str = str(mpn).strip()
    for prefix, name in MPN_PREFIX_MANUFACTURERS:
        if mpn_str.upper().startswith(prefix.upper()):
            return name
    return ""


def is_generic_multisource(mpn):
    mpn_str = str(mpn).strip().upper()
    return any(mpn_str.startswith(p.upper()) for p in GENERIC_MULTISOURCE_PREFIXES)


def guess_type(footprint):
    fp = (footprint or "")
    for hint in SMD_FOOTPRINT_HINTS:
        if hint.lower() in fp.lower():
            return "SMD"
    for hint in THT_FOOTPRINT_HINTS:
        if hint.lower() in fp.lower():
            return "THT"
    return "SMD"  # most common default for this design; flagged rows reviewed by Jason


def process_sheet(wb, sheet_name, out_path):
    ws = wb[sheet_name]
    header = [c.value for c in ws[1]]
    col = {name: i for i, name in enumerate(header)}

    rows_out = []
    line_no = 1
    skipped_dnp = []
    missing_mpn = []
    blank_mfr = []

    for row in ws.iter_rows(min_row=2, max_row=ws.max_row, values_only=True):
        if row[col["Refs"]] is None:
            continue
        qty = row[col["Qty"]]
        refs = row[col["Refs"]]
        value = row[col["Value"]] or ""
        footprint = row[col["Footprint"]] or ""
        mpn = row[col["MPN"]] or ""
        dnp = row[col["DNP"]]

        if dnp:
            skipped_dnp.append((refs, value))
            continue

        if not str(mpn).strip() or str(mpn).strip().upper() in ("N/A", "TBD", "NONE"):
            missing_mpn.append((refs, value))

        manufacturer = guess_manufacturer(mpn, value, footprint)
        if not manufacturer:
            if is_generic_multisource(mpn):
                blank_mfr.append((refs, mpn, value, "generic/multi-sourced"))
            else:
                blank_mfr.append((refs, mpn, value, "unknown"))
        part_type = guess_type(footprint)

        rows_out.append({
            "Line#": line_no,
            "Qty": qty,
            "Designator": refs,
            "MPN": mpn,
            "Manufacturer": manufacturer,
            "Description": value,
            "Package": footprint,
            "Type": part_type,
        })
        line_no += 1

    with open(out_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["Line#", "Qty", "Designator", "MPN", "Manufacturer", "Description", "Package", "Type"])
        writer.writeheader()
        writer.writerows(rows_out)

    return rows_out, skipped_dnp, missing_mpn, blank_mfr


wb = openpyxl.load_workbook(SRC, data_only=True)

main_rows, main_dnp, main_missing, main_blank_mfr = process_sheet(wb, "MAIN board", os.path.join(OUT_DIR, "PCBWay_BOM_main.csv"))
ctrl_rows, ctrl_dnp, ctrl_missing, ctrl_blank_mfr = process_sheet(wb, "CONTROL board", os.path.join(OUT_DIR, "PCBWay_BOM_ctrl.csv"))

print(f"MAIN: {len(main_rows)} lines written, {len(main_dnp)} DNP excluded, {len(main_missing)} missing/placeholder MPN, {len(main_blank_mfr)} blank Manufacturer")
for r in main_missing:
    print("  MAIN missing MPN:", r)
print(f"CTRL: {len(ctrl_rows)} lines written, {len(ctrl_dnp)} DNP excluded, {len(ctrl_missing)} missing/placeholder MPN, {len(ctrl_blank_mfr)} blank Manufacturer")
for r in ctrl_missing:
    print("  CTRL missing MPN:", r)

all_blank = main_blank_mfr + ctrl_blank_mfr
generic_blank = [r for r in all_blank if r[3] == "generic/multi-sourced"]
unknown_blank = [r for r in all_blank if r[3] == "unknown"]

print(f"\nBlank-Manufacturer rows: {len(generic_blank)} deliberately left blank (generic/multi-sourced part, MPN alone is sufficient for PCBWay), {len(unknown_blank)} genuinely unidentified (worth double-checking):")
print("  -- Generic/multi-sourced (MPN alone is sufficient, no single true manufacturer) --")
for refs, mpn, value, _ in generic_blank:
    print(f"  {refs}: MPN={mpn!r} value={value!r}")
print("  -- Genuinely unidentified (double-check these) --")
for refs, mpn, value, _ in unknown_blank:
    print(f"  {refs}: MPN={mpn!r} value={value!r}")
