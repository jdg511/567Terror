import re, sys, uuid, shutil, datetime

path = "glitchwave567_ctrl.kicad_sch"
backup = f"glitchwave567_ctrl.kicad_sch.bak_20260905"
shutil.copy(path, backup)

text = open(path, encoding="utf-8").read()
orig_len = len(text)

def must_replace(text, old, new, label):
    n = text.count(old)
    if n != 1:
        print(f"FAIL {label}: found {n} occurrences (expected 1)")
        sys.exit(1)
    return text.replace(old, new)

# 1) Insert new lib_symbols entry for Switch:SW_Push_SPDT, right before the LED:WS2812B lib entry
anchor = '    (symbol "LED:WS2812B" (pin_names (offset 0.254)) (in_bom yes) (on_board yes)'
new_lib_symbol = '''    (symbol "Switch:SW_Push_SPDT" (pin_numbers hide) (pin_names (offset 1.016) hide) (in_bom yes) (on_board yes)
      (property "Reference" "SW" (at 0 5.08 0)
        (effects (font (size 1.27 1.27)))
      )
      (property "Value" "SW_Push_SPDT" (at 0 -5.08 0)
        (effects (font (size 1.27 1.27)))
      )
      (property "Footprint" "" (at 0 0 0)
        (effects (font (size 1.27 1.27)) hide)
      )
      (property "Datasheet" "" (at 0 0 0)
        (effects (font (size 1.27 1.27)) hide)
      )
      (property "ki_keywords" "switch single-pole double-throw spdt ON-ON" (at 0 0 0)
        (effects (font (size 1.27 1.27)) hide)
      )
      (property "ki_description" "Momentary Switch, single pole double throw. Pin 1=A(NC), Pin 2=B(COM), Pin 3=C(NO) as wired in this project." (at 0 0 0)
        (effects (font (size 1.27 1.27)) hide)
      )
      (symbol "SW_Push_SPDT_0_1"
        (rectangle (start -3.175 3.81) (end 3.175 -3.81)
          (stroke (width 0) (type default))
          (fill (type background))
        )
        (circle (center -2.032 0) (radius 0.508)
          (stroke (width 0) (type default))
          (fill (type none))
        )
        (polyline
          (pts
            (xy 0 1.016)
            (xy 0 3.048)
          )
          (stroke (width 0) (type default))
          (fill (type none))
        )
        (circle (center 2.032 -2.54) (radius 0.508)
          (stroke (width 0) (type default))
          (fill (type none))
        )
        (circle (center 2.032 2.54) (radius 0.508)
          (stroke (width 0) (type default))
          (fill (type none))
        )
        (pin passive line (at 5.08 2.54 180) (length 2.54)
          (name "A" (effects (font (size 1.27 1.27))))
          (number "1" (effects (font (size 1.27 1.27))))
        )
        (pin passive line (at -5.08 0 0) (length 2.54)
          (name "B" (effects (font (size 1.27 1.27))))
          (number "2" (effects (font (size 1.27 1.27))))
        )
        (pin passive line (at 5.08 -2.54 180) (length 2.54)
          (name "C" (effects (font (size 1.27 1.27))))
          (number "3" (effects (font (size 1.27 1.27))))
        )
      )
    )

''' + anchor

text = must_replace(text, anchor, new_lib_symbol, "lib_symbols insert")

# 2) Replace SW1 instance block
sw1_old = '''  (symbol (lib_id "Switch:SW_Push") (at 228.6 45.72 0) (unit 1)
    (in_bom yes) (on_board yes) (dnp no)
    (uuid 325838c3-303e-412f-9969-c6b4a95eeb67)
    (property "Reference" "SW1" (id 0) (at 228.6 43.18 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Value" "Suntsu SSWFS-S01 (TAP/BYP)" (id 1) (at 228.6 48.26 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Footprint" "Glitchwave:SSWFS-S01" (id 2) (at 228.6 45.72 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    (property "Datasheet" "" (id 3) (at 228.6 45.72 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    (instances
      (project "glitchwave567"
        (path "/"
          (reference "SW1") (unit 1)
        )
      )
    )
  )'''

sw1_new = '''  (symbol (lib_id "Switch:SW_Push_SPDT") (at 228.6 45.72 0) (unit 1)
    (in_bom yes) (on_board yes) (dnp no)
    (uuid 325838c3-303e-412f-9969-c6b4a95eeb67)
    (property "Reference" "SW1" (id 0) (at 228.6 43.18 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Value" "Alpha SF12011F-0102-20R-M-050 (TAP/BYP)" (id 1) (at 228.6 51.0 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Footprint" "Glitchwave:SF12011F-0102-M" (id 2) (at 228.6 45.72 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    (property "Datasheet" "https://www.mouser.com/datasheet/2/13/Foot_Switch___SF12_Series-2303797.pdf" (id 3) (at 228.6 45.72 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    (instances
      (project "glitchwave567"
        (path "/"
          (reference "SW1") (unit 1)
        )
      )
    )
  )'''

text = must_replace(text, sw1_old, sw1_new, "SW1 instance")

# 3) Replace SW2 instance block
sw2_old = '''  (symbol (lib_id "Switch:SW_Push") (at 266.7 45.72 0) (unit 1)
    (in_bom yes) (on_board yes) (dnp no)
    (uuid faf28cad-1a29-48e1-96c9-070397e36bb6)
    (property "Reference" "SW2" (id 0) (at 266.7 43.18 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Value" "Suntsu SSWFS-S01 (TAP/BYP)" (id 1) (at 266.7 48.26 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Footprint" "Glitchwave:SSWFS-S01" (id 2) (at 266.7 45.72 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    (property "Datasheet" "" (id 3) (at 266.7 45.72 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    (instances
      (project "glitchwave567"
        (path "/"
          (reference "SW2") (unit 1)
        )
      )
    )
  )'''

sw2_new = '''  (symbol (lib_id "Switch:SW_Push_SPDT") (at 266.7 45.72 0) (unit 1)
    (in_bom yes) (on_board yes) (dnp no)
    (uuid faf28cad-1a29-48e1-96c9-070397e36bb6)
    (property "Reference" "SW2" (id 0) (at 266.7 43.18 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Value" "Alpha SF12011F-0102-20R-M-050 (TAP/TEMPO)" (id 1) (at 266.7 51.0 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Footprint" "Glitchwave:SF12011F-0102-M" (id 2) (at 266.7 45.72 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    (property "Datasheet" "https://www.mouser.com/datasheet/2/13/Foot_Switch___SF12_Series-2303797.pdf" (id 3) (at 266.7 45.72 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    (instances
      (project "glitchwave567"
        (path "/"
          (reference "SW2") (unit 1)
        )
      )
    )
  )'''

text = must_replace(text, sw2_old, sw2_new, "SW2 instance")

# 4) Move SW1 GND wire + label down 2.54mm (45.72 -> 43.18) to align with new NO pin (pin C)
text = must_replace(
    text,
    '(wire (pts (xy 233.68 45.72) (xy 236.22 45.72))\n    (stroke (width 0.0))\n    (uuid 8b392950-8122-4c7b-90e8-9c406bd2e338)\n  )',
    '(wire (pts (xy 233.68 43.18) (xy 236.22 43.18))\n    (stroke (width 0.0))\n    (uuid 8b392950-8122-4c7b-90e8-9c406bd2e338)\n  )',
    "SW1 GND wire move",
)
text = must_replace(
    text,
    '(global_label "GND" (shape bidirectional) (at 236.22 45.72 0)\n    (effects (font (size 1.27 1.27)))\n    (uuid a87cbb5a-6999-4bfb-9d05-8b2b7a2fa576)\n  )',
    '(global_label "GND" (shape bidirectional) (at 236.22 43.18 0)\n    (effects (font (size 1.27 1.27)))\n    (uuid a87cbb5a-6999-4bfb-9d05-8b2b7a2fa576)\n  )',
    "SW1 GND label move",
)

# 5) Move SW2 GND wire + label down 2.54mm (45.72 -> 43.18)
text = must_replace(
    text,
    '(wire (pts (xy 271.78 45.72) (xy 274.32 45.72))\n    (stroke (width 0.0))\n    (uuid 1ba5b669-e243-4ea6-99fc-06c0598a9d1f)\n  )',
    '(wire (pts (xy 271.78 43.18) (xy 274.32 43.18))\n    (stroke (width 0.0))\n    (uuid 1ba5b669-e243-4ea6-99fc-06c0598a9d1f)\n  )',
    "SW2 GND wire move",
)
text = must_replace(
    text,
    '(global_label "GND" (shape bidirectional) (at 274.32 45.72 0)\n    (effects (font (size 1.27 1.27)))\n    (uuid cef90687-12d9-4ae6-8592-0fa923ca6850)\n  )',
    '(global_label "GND" (shape bidirectional) (at 274.32 43.18 0)\n    (effects (font (size 1.27 1.27)))\n    (uuid cef90687-12d9-4ae6-8592-0fa923ca6850)\n  )',
    "SW2 GND label move",
)

# 6) Add no_connect markers for the unused NC pins (pin A of SW1 and SW2)
nc1 = str(uuid.uuid4())
nc2 = str(uuid.uuid4())
anchor_nc = '  (no_connect (at 40.64 73.66) (uuid 04c67863-0f3b-43a6-a65f-08200b8edf39))'
new_nc = (anchor_nc +
    f'\n  (no_connect (at 233.68 48.26) (uuid {nc1}))'
    f'\n  (no_connect (at 271.78 48.26) (uuid {nc2}))')
text = must_replace(text, anchor_nc, new_nc, "no_connect insert")

open(path, "w", encoding="utf-8").write(text)
print("OK, wrote", len(text), "bytes (was", orig_len, "), backup at", backup)
