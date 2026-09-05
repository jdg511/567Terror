import sys, uuid, shutil, os

HERE = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(HERE, "..", "glitchwave567_ctrl", "glitchwave567_ctrl.kicad_pcb")
backup = os.path.join(HERE, "..", "glitchwave567_ctrl", "glitchwave567_ctrl.kicad_pcb.bak_20260905e")
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


def seg(start, end, width, layer, net, uid):
    return f'''\t(segment
\t\t(start {start[0]} {start[1]})
\t\t(end {end[0]} {end[1]})
\t\t(width {width})
\t\t(layer "{layer}")
\t\t(net "{net}")
\t\t(uuid "{uid}")
\t)'''


# --- SW1: the old diagonal trace (65f04cba) has its free end at (30.8,96.25),
# which sits INSIDE the new GND pad's copper (pad center 31.5,96, radius 0.9mm,
# distance only 0.7mm). No dogleg starting from that point can avoid the pad --
# the point itself is inside it. Since (30.8,96.25) is a dead-end (not a
# component pad, just where the old bridge used to attach), replace the whole
# diagonal with a fresh path from its OTHER end (40.7,106.15, a real junction
# with the rest of the /STOMP1 route -- must stay put) around to the new COM
# pad (34,96), staying 2.5mm+ clear of the pad row until dropping straight
# down onto COM.
sw1_diagonal_old = '''\t(segment
\t\t(start 30.8 96.25)
\t\t(end 40.7 106.15)
\t\t(width 0.25)
\t\t(layer "F.Cu")
\t\t(net "/STOMP1")
\t\t(uuid "65f04cba-3d11-4763-b548-140574828203")
\t)'''
sw1_new_path = "\n".join([
    seg((40.7, 106.15), (40.7, 93.0), 0.25, "F.Cu", "/STOMP1", "65f04cba-3d11-4763-b548-140574828203"),
    seg((40.7, 93.0), (34, 93.0), 0.25, "F.Cu", "/STOMP1", u()),
    seg((34, 93.0), (34, 96), 0.25, "F.Cu", "/STOMP1", u()),
])
text = must_replace(text, sw1_diagonal_old, sw1_new_path, "SW1 diagonal -> fresh path to COM")

# Remove the old (now orphaned/redundant) straight bridge from v3 -- it used
# the doomed (30.8,96.25) anchor point and is no longer needed.
sw1_old_bridge = '''\t(segment
\t\t(start 30.8 96.25)
\t\t(end 34 96)
\t\t(width 0.25)
\t\t(layer "F.Cu")
\t\t(net "/STOMP1")
\t\t(uuid "054e2a70-f73f-4a95-931d-9867913026f7")
\t)
'''
n = text.count(sw1_old_bridge)
if n != 1:
    print(f"FAIL removing SW1 old bridge: found {n} occurrences (expected 1)")
    sys.exit(1)
text = text.replace(sw1_old_bridge, "")

# --- SW2: same problem, same fix. Free end (101.2,96.25) is only 0.39mm from
# the new GND pad center (101.5,96) -- inside the pad. Replace the diagonal
# (b6f9ae45) with a fresh path from its junction end (104.5,99.55, connects
# onward to the rest of /STOMP2 toward J1 -- must stay put) around to COM
# (104,96).
sw2_diagonal_old = '''\t(segment
\t\t(start 101.2 96.25)
\t\t(end 104.5 99.55)
\t\t(width 0.25)
\t\t(layer "F.Cu")
\t\t(net "/STOMP2")
\t\t(uuid "b6f9ae45-b49f-4441-964a-a223ba874a21")
\t)'''
sw2_new_path = "\n".join([
    seg((104.5, 99.55), (104.5, 93.0), 0.25, "F.Cu", "/STOMP2", "b6f9ae45-b49f-4441-964a-a223ba874a21"),
    seg((104.5, 93.0), (104, 93.0), 0.25, "F.Cu", "/STOMP2", u()),
    seg((104, 93.0), (104, 96), 0.25, "F.Cu", "/STOMP2", u()),
])
text = must_replace(text, sw2_diagonal_old, sw2_new_path, "SW2 diagonal -> fresh path to COM")

sw2_old_bridge = '''\t(segment
\t\t(start 101.2 96.25)
\t\t(end 104 96)
\t\t(width 0.25)
\t\t(layer "F.Cu")
\t\t(net "/STOMP2")
\t\t(uuid "35e02238-8bb4-49ca-8a55-81c6eeb706cf")
\t)
'''
n = text.count(sw2_old_bridge)
if n != 1:
    print(f"FAIL removing SW2 old bridge: found {n} occurrences (expected 1)")
    sys.exit(1)
text = text.replace(sw2_old_bridge, "")

# --- /POT6_W long vertical run passes at x=103.95, only 0.05mm from the new
# SW2 COM pad center (104,96) -- a real short (thru-hole pad copper exists on
# every layer, including B.Cu where this trace runs). Detour it left around
# the whole SW2 pad row via x=99, rejoining at y=91.5 (clear of the SW1/SW2
# reroutes above, which live at y=93).
pot6_old = '''\t(segment
\t\t(start 103.95 97.9)
\t\t(end 103.95 57.2)
\t\t(width 0.25)
\t\t(layer "B.Cu")
\t\t(net "/POT6_W")
\t\t(uuid "2b258872-4d12-483f-bec0-fadbc73c9b12")
\t)'''
pot6_new = "\n".join([
    seg((103.95, 97.9), (99.0, 97.9), 0.25, "B.Cu", "/POT6_W", u()),
    seg((99.0, 97.9), (99.0, 91.5), 0.25, "B.Cu", "/POT6_W", u()),
    seg((99.0, 91.5), (103.95, 91.5), 0.25, "B.Cu", "/POT6_W", u()),
    seg((103.95, 91.5), (103.95, 57.2), 0.25, "B.Cu", "/POT6_W", "2b258872-4d12-483f-bec0-fadbc73c9b12"),
])
text = must_replace(text, pot6_old, pot6_new, "POT6_W detour around SW2 pad")

open(path, "w", encoding="utf-8").write(text)
print("OK, wrote", len(text), "bytes (was", orig_len, "), backup at", backup)
