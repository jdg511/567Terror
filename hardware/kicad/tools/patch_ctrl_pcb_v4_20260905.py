import sys, uuid, shutil, os

HERE = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(HERE, "..", "glitchwave567_ctrl", "glitchwave567_ctrl.kicad_pcb")
backup = os.path.join(HERE, "..", "glitchwave567_ctrl", "glitchwave567_ctrl.kicad_pcb.bak_20260905d")
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


# --- SW1 bridge: replace straight-line bridge (crosses the new GND pad at
# 31.5,96) with a dogleg that stays 2.5mm+ clear of the pad row (y=96) until
# it is directly over the COM pad, then drops straight down onto it.
sw1_old_bridge = '''\t(segment
\t\t(start 30.8 96.25)
\t\t(end 34 96)
\t\t(width 0.25)
\t\t(layer "F.Cu")
\t\t(net "/STOMP1")
\t\t(uuid "054e2a70-f73f-4a95-931d-9867913026f7")
\t)'''
sw1_new_bridge = "\n".join([
    seg((30.8, 96.25), (30.8, 93.0), 0.25, "F.Cu", "/STOMP1", "054e2a70-f73f-4a95-931d-9867913026f7"),
    seg((30.8, 93.0), (34, 93.0), 0.25, "F.Cu", "/STOMP1", u()),
    seg((34, 93.0), (34, 96), 0.25, "F.Cu", "/STOMP1", u()),
])
text = must_replace(text, sw1_old_bridge, sw1_new_bridge, "SW1 bridge dogleg")

# --- SW2 bridge: same fix (was crossing the GND pad at 101.5,96).
sw2_old_bridge = '''\t(segment
\t\t(start 101.2 96.25)
\t\t(end 104 96)
\t\t(width 0.25)
\t\t(layer "F.Cu")
\t\t(net "/STOMP2")
\t\t(uuid "35e02238-8bb4-49ca-8a55-81c6eeb706cf")
\t)'''
sw2_new_bridge = "\n".join([
    seg((101.2, 96.25), (101.2, 93.0), 0.25, "F.Cu", "/STOMP2", "35e02238-8bb4-49ca-8a55-81c6eeb706cf"),
    seg((101.2, 93.0), (104, 93.0), 0.25, "F.Cu", "/STOMP2", u()),
    seg((104, 93.0), (104, 96), 0.25, "F.Cu", "/STOMP2", u()),
])
text = must_replace(text, sw2_old_bridge, sw2_new_bridge, "SW2 bridge dogleg")

# --- /POT6_W long vertical run: it passes at x=103.95, only 0.05mm from the
# new SW2 COM pad center (104,96) -- a real short (thru-hole pad copper is on
# every layer, including B.Cu where this trace runs). Detour it left around
# the whole SW2 pad row (x 99.6..107.4 at y=96) via x=99, at y=91.5 (clear of
# the SW1/SW2 bridge doglegs above, which live at y=93).
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
