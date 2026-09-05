import openpyxl, shutil

shutil.copy("BOM.xlsx", "BOM.bak_20260905b.xlsx")

wb = openpyxl.load_workbook("BOM.xlsx")
ws = wb["CONTROL board"]
header = [c.value for c in ws[1]]
col = {name: i + 1 for i, name in enumerate(header)}

target_row = None
for row in ws.iter_rows(min_row=2, max_row=ws.max_row):
    refs = row[col["Refs"] - 1].value
    if refs and "SW1" in str(refs) and "SW2" in str(refs):
        target_row = row[0].row
        break

assert target_row is not None, "SW1/SW2 row not found"
print("Found SW1/SW2 row at", target_row)

ws.cell(row=target_row, column=col["Value"]).value = "Alpha SF12011F-0102-20R-M-050 (momentary, TAP/BYP + TAP/TEMPO)"
ws.cell(row=target_row, column=col["Footprint"]).value = "SF12011F-0102-M"
ws.cell(row=target_row, column=col["MPN"]).value = "SF12011F-0102-20R-M-050"
ws.cell(row=target_row, column=col["Source"]).value = "PCBWay (turnkey, sourced by MPN)"
ws.cell(row=target_row, column=col["Part # (LCSC C# / DK)"]).value = "N/A - Alpha (Taiwan) direct, not LCSC/DK stocked"
ws.cell(row=target_row, column=col["Notes / verification"]).value = (
    "2026-09-05: switched from Suntsu SSWFS-S01 (unsourceable) to Alpha SF12011F-0102-20R-M-050. "
    "Both SW1 and SW2 kept MOMENTARY (not latching) to preserve the STARVE gesture (both-held) and "
    "existing 2-switch/5-gesture firmware in stomps.c - a latching bypass switch was considered and "
    "rejected. \"011F\" prefix = Alpha's genuine PCB-mount (PC-pin) terminal type per their SF12 series "
    "datasheet, confirmed board-mountable with no panel cutouts (matches project's existing no-cutout "
    "architecture) - the E-Switch FS5700 alternate looked into earlier was rejected because it uses rear "
    "solder lugs, not PCB pins, and cannot be machine-assembled. Pin 1=COM, Pin 2=N.O., Pin 3=N.C. per "
    "datasheet wiring diagram; N.C. left unconnected. Footprint reuses the prior SSWFS-S01 pad positions "
    "for COM/N.O. so existing routing needed no rework; N.C. is a new pad. Thread M12x0.75 vs prior "
    "M12x1.0 spec - panel hole stays D12.2 (M12 nominal OD unaffected by thread pitch), no enclosure "
    "change needed. CAVEAT: exact COM-to-throw terminal pitch is not published in any datasheet copy "
    "fetchable this session (Mouser's PDF host blocks automated retrieval) - footprint uses oversized "
    "isotropic pads (D2.2/D1.3) as a placeholder tolerance margin. VERIFY against the physical part or "
    "PCBWay's DFM/sourcing review before the order is placed. No longer DNP - PCBWay turnkey should be "
    "able to source and place this by MPN."
)
ws.cell(row=target_row, column=col["DNP"]).value = None

wb.save("BOM.xlsx")
print("Saved BOM.xlsx")

# verify
wb2 = openpyxl.load_workbook("BOM.xlsx")
ws2 = wb2["CONTROL board"]
for c in ws2[target_row]:
    print(c.coordinate, repr(c.value)[:120])
