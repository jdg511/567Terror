import openpyxl, shutil

shutil.copy("BOM.xlsx", "BOM.bak_20260905c.xlsx")

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

ws.cell(row=target_row, column=col["Value"]).value = "Alpha SF12011F-0102-20R-M-011 (momentary, TAP/BYP + TAP/TEMPO)"
ws.cell(row=target_row, column=col["Footprint"]).value = "SF12011F-0102-M"
ws.cell(row=target_row, column=col["MPN"]).value = "SF12011F-0102-20R-M-011"
ws.cell(row=target_row, column=col["Source"]).value = "PCBWay (turnkey, sourced by MPN)"
ws.cell(row=target_row, column=col["Part # (LCSC C# / DK)"]).value = "N/A - Alpha (Taiwan) direct, not LCSC/DK stocked"
ws.cell(row=target_row, column=col["Notes / verification"]).value = (
    "2026-09-05: switched from Suntsu SSWFS-S01 (unsourceable) to Alpha SF12011F-0102-20R-M-011. "
    "Both SW1 and SW2 kept MOMENTARY (not latching) to preserve the STARVE gesture (both-held) and "
    "existing 2-switch/5-gesture firmware in stomps.c - a latching bypass switch was considered and "
    "rejected. \"011F\" prefix = Alpha's genuine PCB-mount (PC-pin) terminal type per their SF12 series "
    "datasheet, confirmed board-mountable with no panel cutouts (matches project's existing no-cutout "
    "architecture) - the E-Switch FS5700 alternate looked into earlier was rejected because it uses rear "
    "solder lugs, not PCB pins, and cannot be machine-assembled. VERIFIED from Alpha's own SF12 series "
    "datasheet (user-supplied PDF, page 3, SF12011F-0102-20R-M-011 outline drawing): terminals in a "
    "straight row, 2.5mm pitch, ~1.0mm pin width; pinout Pin 1=N.O., Pin 2=COM, Pin 3=N.C. (wiring "
    "diagram: PUSH 1-2 ON, FREE 2-3 ON). Pin 1 (N.O.) wired to GND, pin 2 (COM) wired to the existing "
    "GPIO/pullup net, pin 3 (N.C.) left genuinely unconnected. Thread M12x0.75 vs prior M12x1.0 spec - "
    "panel hole stays D12.2 (M12 nominal OD unaffected by thread pitch), no enclosure change needed. "
    "Body 13.4mm dia, ~12mm deep below panel - fits existing 15.0+/-1.0mm enclosure depth budget. Since "
    "the real part's pin pitch/positions differ from the old SSWFS-S01 footprint, the PCB routing at "
    "SW1/SW2 was re-run (new footprint, corrected pinout, existing traces rerouted to reach the new pad "
    "positions) - DRC and ERC both clean (0 errors) as of 2026-09-05. No longer DNP - PCBWay turnkey "
    "should be able to source and place this by MPN."
)
ws.cell(row=target_row, column=col["DNP"]).value = None

wb.save("BOM.xlsx")
print("Saved BOM.xlsx")

# verify
wb2 = openpyxl.load_workbook("BOM.xlsx")
ws2 = wb2["CONTROL board"]
for c in ws2[target_row]:
    print(c.coordinate, repr(c.value)[:120])
