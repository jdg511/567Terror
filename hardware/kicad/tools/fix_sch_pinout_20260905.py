import sys, shutil, os

HERE = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(HERE, "..", "glitchwave567_ctrl", "glitchwave567_ctrl.kicad_sch")
backup = os.path.join(HERE, "..", "glitchwave567_ctrl", "glitchwave567_ctrl.kicad_sch.bak_20260905b")
shutil.copy(path, backup)

text = open(path, encoding="utf-8").read()
orig_len = len(text)

def must_replace(text, old, new, label):
    n = text.count(old)
    if n != 1:
        print(f"FAIL {label}: found {n} occurrences (expected 1)")
        sys.exit(1)
    return text.replace(old, new)

# Real datasheet (Alpha SF12 series, page 3) says: pin 1 = N.O., pin 2 = COM, pin 3 = N.C.
# (schematic legend: PUSH 1-2 ON, FREE 2-3 ON -> pin 2 is common to both states).
# My symbol pin "2" (COM) was already wired correctly to STOMP1/STOMP2.
# But GND (N.O.) and no-connect (N.C.) were on the wrong pins - swap them:
#   pin "1" (was no-connect)   -> should get GND
#   pin "3" (was GND)          -> should become no-connect

# SW1: pin1 abs (233.68, 48.26), pin3 abs (233.68, 43.18)
text = must_replace(
    text,
    '(wire (pts (xy 233.68 43.18) (xy 236.22 43.18))\n    (stroke (width 0.0))\n    (uuid 8b392950-8122-4c7b-90e8-9c406bd2e338)\n  )',
    '(wire (pts (xy 233.68 48.26) (xy 236.22 48.26))\n    (stroke (width 0.0))\n    (uuid 8b392950-8122-4c7b-90e8-9c406bd2e338)\n  )',
    "SW1 GND wire -> pin1",
)
text = must_replace(
    text,
    '(global_label "GND" (shape bidirectional) (at 236.22 43.18 0)\n    (effects (font (size 1.27 1.27)))\n    (uuid a87cbb5a-6999-4bfb-9d05-8b2b7a2fa576)\n  )',
    '(global_label "GND" (shape bidirectional) (at 236.22 48.26 0)\n    (effects (font (size 1.27 1.27)))\n    (uuid a87cbb5a-6999-4bfb-9d05-8b2b7a2fa576)\n  )',
    "SW1 GND label -> pin1",
)

# SW2: pin1 abs (271.78, 48.26), pin3 abs (271.78, 43.18)
text = must_replace(
    text,
    '(wire (pts (xy 271.78 43.18) (xy 274.32 43.18))\n    (stroke (width 0.0))\n    (uuid 1ba5b669-e243-4ea6-99fc-06c0598a9d1f)\n  )',
    '(wire (pts (xy 271.78 48.26) (xy 274.32 48.26))\n    (stroke (width 0.0))\n    (uuid 1ba5b669-e243-4ea6-99fc-06c0598a9d1f)\n  )',
    "SW2 GND wire -> pin1",
)
text = must_replace(
    text,
    '(global_label "GND" (shape bidirectional) (at 274.32 43.18 0)\n    (effects (font (size 1.27 1.27)))\n    (uuid cef90687-12d9-4ae6-8592-0fa923ca6850)\n  )',
    '(global_label "GND" (shape bidirectional) (at 274.32 48.26 0)\n    (effects (font (size 1.27 1.27)))\n    (uuid cef90687-12d9-4ae6-8592-0fa923ca6850)\n  )',
    "SW2 GND label -> pin1",
)

# no_connect markers: move from pin1 positions to pin3 positions
import re
m = re.search(r'\(no_connect \(at 233\.68 48\.26\) \(uuid ([0-9a-f-]+)\)\)', text)
assert m, "SW1 no_connect not found"
nc1_uuid = m.group(1)
text = must_replace(
    text,
    f'(no_connect (at 233.68 48.26) (uuid {nc1_uuid}))',
    f'(no_connect (at 233.68 43.18) (uuid {nc1_uuid}))',
    "SW1 no_connect -> pin3",
)

m = re.search(r'\(no_connect \(at 271\.78 48\.26\) \(uuid ([0-9a-f-]+)\)\)', text)
assert m, "SW2 no_connect not found"
nc2_uuid = m.group(1)
text = must_replace(
    text,
    f'(no_connect (at 271.78 48.26) (uuid {nc2_uuid}))',
    f'(no_connect (at 271.78 43.18) (uuid {nc2_uuid}))',
    "SW2 no_connect -> pin3",
)

# Also fix the pin-mapping note in the embedded lib_symbols description
text = must_replace(
    text,
    'Momentary Switch, single pole double throw. Pin 1=A(NC), Pin 2=B(COM), Pin 3=C(NO) as wired in this project.',
    'Momentary Switch, single pole double throw. Per Alpha SF12 series datasheet: Pin 1=N.O., Pin 2=COM, Pin 3=N.C.',
    "lib_symbols description fix",
)

open(path, "w", encoding="utf-8").write(text)
print("OK, wrote", len(text), "bytes (was", orig_len, "), backup at", backup)
