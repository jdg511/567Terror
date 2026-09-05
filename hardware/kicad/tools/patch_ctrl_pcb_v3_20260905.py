import sys, uuid, shutil, os

HERE = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(HERE, "..", "glitchwave567_ctrl", "glitchwave567_ctrl.kicad_pcb")
backup = os.path.join(HERE, "..", "glitchwave567_ctrl", "glitchwave567_ctrl.kicad_pcb.bak_20260905c")
shutil.copy(path, backup)

text = open(path, encoding="utf-8").read()
orig_len = len(text)


def must_replace(text, old, new, label):
    n = text.count(old)
    if n != 1:
        print(f"FAIL {label}: found {n} occurrences (expected 1)")
        sys.exit(1)
    return text.replace(old, new)


def u():
    return str(uuid.uuid4())


def new_block(footprint_uuid, ref, value, ref_prop_uuid, val_prop_uuid, ds_prop_uuid,
              desc_prop_uuid, at_xy, no_net, com_net, nc_placeholder):
    return f'''\t(footprint "SF12011F-0102-M"
\t\t(layer "F.Cu")
\t\t(uuid "{footprint_uuid}")
\t\t(at {at_xy})
\t\t(descr "Alpha (Taiwan) SF12011F-0102-20R-M-011, SPDT momentary footswitch, genuine PCB-mount (PC-pin) terminal type per Alpha SF12 series datasheet. Pin 1=N.O., Pin 2=COM, Pin 3=N.C. (PUSH 1-2 ON, FREE 2-3 ON). Terminals in a row, 2.5mm pitch, verified from datasheet outline drawing. Thread M12x0.75, PANEL HOLE D12.2. Body 13.4mm dia, ~12mm deep below panel.")
\t\t(property "Reference" "{ref}"
\t\t\t(at 0 -3.2 0)
\t\t\t(layer "F.SilkS")
\t\t\t(uuid "{ref_prop_uuid}")
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1 1)
\t\t\t\t\t(thickness 0.15)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Value" "{value}"
\t\t\t(at 0 3.2 0)
\t\t\t(layer "F.Fab")
\t\t\t(uuid "{val_prop_uuid}")
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1 1)
\t\t\t\t\t(thickness 0.15)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Datasheet" "https://www.mouser.com/datasheet/2/13/Foot_Switch___SF12_Series-2303797.pdf"
\t\t\t(at 0 0 0)
\t\t\t(layer "F.Fab")
\t\t\t(hide yes)
\t\t\t(uuid "{ds_prop_uuid}")
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(property "Description" ""
\t\t\t(at 0 0 0)
\t\t\t(layer "F.Fab")
\t\t\t(hide yes)
\t\t\t(uuid "{desc_prop_uuid}")
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(attr through_hole)
\t\t(duplicate_pad_numbers_are_jumpers no)
\t\t(fp_line
\t\t\t(start -6.7 -6.7)
\t\t\t(end 6.7 -6.7)
\t\t\t(stroke
\t\t\t\t(width 0.12)
\t\t\t\t(type solid)
\t\t\t)
\t\t\t(layer "F.Fab")
\t\t\t(uuid "{u()}")
\t\t)
\t\t(fp_line
\t\t\t(start -6.7 6.7)
\t\t\t(end -6.7 -6.7)
\t\t\t(stroke
\t\t\t\t(width 0.12)
\t\t\t\t(type solid)
\t\t\t)
\t\t\t(layer "F.Fab")
\t\t\t(uuid "{u()}")
\t\t)
\t\t(fp_line
\t\t\t(start 6.7 -6.7)
\t\t\t(end 6.7 6.7)
\t\t\t(stroke
\t\t\t\t(width 0.12)
\t\t\t\t(type solid)
\t\t\t)
\t\t\t(layer "F.Fab")
\t\t\t(uuid "{u()}")
\t\t)
\t\t(fp_line
\t\t\t(start 6.7 6.7)
\t\t\t(end -6.7 6.7)
\t\t\t(stroke
\t\t\t\t(width 0.12)
\t\t\t\t(type solid)
\t\t\t)
\t\t\t(layer "F.Fab")
\t\t\t(uuid "{u()}")
\t\t)
\t\t(fp_line
\t\t\t(start -6.95 -6.95)
\t\t\t(end 6.95 -6.95)
\t\t\t(stroke
\t\t\t\t(width 0.05)
\t\t\t\t(type solid)
\t\t\t)
\t\t\t(layer "F.CrtYd")
\t\t\t(uuid "{u()}")
\t\t)
\t\t(fp_line
\t\t\t(start 6.95 -6.95)
\t\t\t(end 6.95 6.95)
\t\t\t(stroke
\t\t\t\t(width 0.05)
\t\t\t\t(type solid)
\t\t\t)
\t\t\t(layer "F.CrtYd")
\t\t\t(uuid "{u()}")
\t\t)
\t\t(fp_line
\t\t\t(start 6.95 6.95)
\t\t\t(end -6.95 6.95)
\t\t\t(stroke
\t\t\t\t(width 0.05)
\t\t\t\t(type solid)
\t\t\t)
\t\t\t(layer "F.CrtYd")
\t\t\t(uuid "{u()}")
\t\t)
\t\t(fp_line
\t\t\t(start -6.95 6.95)
\t\t\t(end -6.95 -6.95)
\t\t\t(stroke
\t\t\t\t(width 0.05)
\t\t\t\t(type solid)
\t\t\t)
\t\t\t(layer "F.CrtYd")
\t\t\t(uuid "{u()}")
\t\t)
\t\t(fp_circle
\t\t\t(center 0 0)
\t\t\t(end 6.7 0)
\t\t\t(stroke
\t\t\t\t(width 0.12)
\t\t\t\t(type solid)
\t\t\t)
\t\t\t(fill no)
\t\t\t(layer "F.Fab")
\t\t\t(uuid "{u()}")
\t\t)
\t\t(fp_text user "D12.2 panel hole"
\t\t\t(at 0 -4.9 0)
\t\t\t(layer "F.Fab")
\t\t\t(uuid "{u()}")
\t\t\t(effects
\t\t\t\t(font
\t\t\t\t\t(size 0.7 0.7)
\t\t\t\t\t(thickness 0.1)
\t\t\t\t)
\t\t\t)
\t\t)
\t\t(pad "1" thru_hole circle
\t\t\t(at -2.5 0)
\t\t\t(size 1.8 1.8)
\t\t\t(drill 1.0)
\t\t\t(layers "*.Cu" "*.Mask")
\t\t\t(remove_unused_layers no)
\t\t\t(net "{no_net}")
\t\t\t(uuid "{u()}")
\t\t)
\t\t(pad "2" thru_hole circle
\t\t\t(at 0 0)
\t\t\t(size 1.8 1.8)
\t\t\t(drill 1.0)
\t\t\t(layers "*.Cu" "*.Mask")
\t\t\t(remove_unused_layers no)
\t\t\t(net "{com_net}")
\t\t\t(uuid "{u()}")
\t\t)
\t\t(pad "3" thru_hole circle
\t\t\t(at 2.5 0)
\t\t\t(size 1.8 1.8)
\t\t\t(drill 1.0)
\t\t\t(layers "*.Cu" "*.Mask")
\t\t\t(remove_unused_layers no)
\t\t\t(net "{nc_placeholder}")
\t\t\t(uuid "{u()}")
\t\t)
\t\t(embedded_fonts no)
\t)'''


sw1_old = open(os.path.join(HERE, "SW1_block.txt"), encoding="utf-8").read()
sw2_old = open(os.path.join(HERE, "SW2_block.txt"), encoding="utf-8").read()

sw1_new = new_block(
    footprint_uuid="776e5246-c32d-4c55-bc5d-cb29a5b3e8b4",
    ref="SW1", value="Alpha SF12011F-0102-20R-M-011 (TAP/BYP)",
    ref_prop_uuid=u(), val_prop_uuid=u(), ds_prop_uuid=u(), desc_prop_uuid=u(),
    at_xy="34 96", no_net="GND", com_net="/STOMP1",
    nc_placeholder="unconnected-(SW1-Pin3_N.C.-Pad3)",
)
sw2_new = new_block(
    footprint_uuid="409a75de-546a-4bdc-b52b-23af52578e6b",
    ref="SW2", value="Alpha SF12011F-0102-20R-M-011 (TAP/TEMPO)",
    ref_prop_uuid=u(), val_prop_uuid=u(), ds_prop_uuid=u(), desc_prop_uuid=u(),
    at_xy="104 96", no_net="GND", com_net="/STOMP2",
    nc_placeholder="unconnected-(SW2-Pin3_N.C.-Pad3)",
)

text = must_replace(text, sw1_old, sw1_new, "SW1 footprint block")
text = must_replace(text, sw2_old, sw2_new, "SW2 footprint block")

# Bridge the existing signal traces (which end at the OLD pad-1 position) to the
# new, recentered COM pad (pad "2", now at the footprint origin for both switches).
anchor_sw2 = '''\t(segment
\t\t(start 101.2 96.25)
\t\t(end 104.5 99.55)
\t\t(width 0.25)
\t\t(layer "F.Cu")
\t\t(net "/STOMP2")
\t\t(uuid "b6f9ae45-b49f-4441-964a-a223ba874a21")
\t)'''
bridge_sw2 = anchor_sw2 + f'''
\t(segment
\t\t(start 101.2 96.25)
\t\t(end 104 96)
\t\t(width 0.25)
\t\t(layer "F.Cu")
\t\t(net "/STOMP2")
\t\t(uuid "{u()}")
\t)'''
text = must_replace(text, anchor_sw2, bridge_sw2, "SW2 COM bridge segment")

anchor_sw1 = '''\t(segment
\t\t(start 30.8 96.25)
\t\t(end 40.7 106.15)
\t\t(width 0.25)
\t\t(layer "F.Cu")
\t\t(net "/STOMP1")
\t\t(uuid "65f04cba-3d11-4763-b548-140574828203")
\t)'''
bridge_sw1 = anchor_sw1 + f'''
\t(segment
\t\t(start 30.8 96.25)
\t\t(end 34 96)
\t\t(width 0.25)
\t\t(layer "F.Cu")
\t\t(net "/STOMP1")
\t\t(uuid "{u()}")
\t)'''
text = must_replace(text, anchor_sw1, bridge_sw1, "SW1 COM bridge segment")

open(path, "w", encoding="utf-8").write(text)
print("OK, wrote", len(text), "bytes (was", orig_len, "), backup at", backup)
