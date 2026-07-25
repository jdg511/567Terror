"""MAIN board placement — 138x114 corner-notched, jacks on walls, audio chain
right-to-left (in->dirt->567->SVF->output), digital bottom, buck top-center."""
import sys; sys.path.insert(0,'/home/claude/work/hardware/kicad/tools')
import pcbgen, pcbnew, collections

W, H = 138.0, 114.0
comps, padnets, nets = pcbgen.parse_netlist('/tmp/main.net')
b = pcbgen.Board('/home/claude/work/hardware/kicad/glitchwave567/glitchwave567.kicad_pcb')
b.make_nets(nets)
b.outline_notched(W, H)

POWER_NETS = {'GND','VA','VREF','+5V','3V3','V567','VDIRT','V9RAW','VIN_RAW','VPROT','PICO_VSYS'}

def place(ref, x, y, rot=0):
    c = comps[ref]
    return b.add_part(ref, c['fp'], c['val'], padnets.get(ref, {}), x, y, rot)

def place_wall_jack(ref, x, y, want):
    """try rotations; keep the one whose pads' centroid is deepest inside per 'want' axis"""
    c = comps[ref]
    best, bestscore = 0, -1e9
    for rot in (0, 90, 180, 270):
        fp = pcbgen.load_fp(c['fp'])
        fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
        fp.SetOrientationDegrees(rot)
        cx = cy = n = 0
        for p in fp.Pads():
            pos = p.GetPosition(); cx += pcbnew.ToMM(pos.x); cy += pcbnew.ToMM(pos.y); n += 1
        cx /= n; cy /= n
        score = {'down': cy, 'up': -cy, 'right': cx, 'left': -cx}[want]
        if score > bestscore: bestscore, best = score, rot
    return place(ref, x, y, best)

# ---- wall jacks (back wall y=0: OUT / DC / IN; CV left; CVOUT right) -------
place_wall_jack('J2', 28, 0.8, 'down')     # OUT 6.35
place_wall_jack('J5', 69, 2.0, 'down')     # DC (nose to wall)
place_wall_jack('J1', 110, 0.8, 'down')    # IN 6.35
place_wall_jack('J3', 0.8, 40, 'right')    # CV1
place_wall_jack('J4', 0.8, 75, 'right')    # CV2
place_wall_jack('J6', 137.2, 57, 'left')   # CV OUT

# ---- power region (top center) --------------------------------------------
place('Q10', 55, 12, 90)      # reverse P-FET
place('FB100', 61, 16, 90)
place('D106', 50, 20, 0)      # TVS
place('U19', 88, 16, 0)       # MP1584
place('L100', 101, 22, 0)
place('D101', 94, 26, 90)
place('U18', 80, 27, 0)       # 78L09
place('D105', 87, 31, 90)
place('U6', 55, 30, 0)        # util TL074 (VREF + starve)
place('Q11', 64, 34, 0)       # BCP56

# ---- audio chain: right wall -> left --------------------------------------
place('U1', 120, 42, 0)       # input TL074
place('U2', 120, 62, 0)       # trim + dirt I-V TL074
place('U3', 108, 70, 0)       # dirt VCA LM13700
place('Q1', 128, 76, 0)       # Bazz Fuss darlington
place('U5', 97, 50, 0)        # LM567  <-- the heart
place('U7', 97, 66, 0)        # timing OTA
place('U8', 97, 82, 0)        # CD4052 timing caps
place('U9', 74, 46, 0)        # mix/SVF TL074
place('U10', 74, 60, 0)       # mix VCAs
place('U11', 62, 52, 0)       # SVF integrators
place('U12', 62, 66, 0)       # Q VCA
place('U13', 74, 74, 0)       # CD4051 mode
place('U14', 38, 44, 0)       # output voicing TL074
place('U15', 38, 60, 0)       # gate/bypass VCAs
place('U16', 24, 52, 0)       # out I-V + buffer
place('SW1', 18, 76, 0); place('SW2', 26, 76, 0); place('SW3', 34, 76, 0)

# ---- digital / interface (bottom) -----------------------------------------
place('U20', 46, 97, 90)      # Pico, long axis horizontal
place('U21', 78, 96, 0)       # 74HC4067
place('U22', 66, 108, 0)      # AHCT1G125
place('U17', 97, 99, 0)       # env follower TL074
place('J10', 96, 104, 90)      # control header
place('RV1', 116, 96, 0); place('RV2', 124, 96, 0); place('RV3', 132, 96, 0)

# ---- auto-place everything else near an anchor ----------------------------
placed = set(b.fps.keys())
def anchor_for(ref):
    for pin, net in padnets.get(ref, {}).items():
        if net in POWER_NETS: continue
        for r2, pn2 in padnets.items():
            if r2 in placed and r2 != ref and net in pn2.values():
                p = b.fps[r2].GetPosition()
                return (pcbnew.ToMM(p.x), pcbnew.ToMM(p.y))
    # power-only part: near center of power region
    return (70, 25)

order = sorted([r for r in comps if r not in placed],
               key=lambda r: (0 if comps[r]['fp'].startswith('Capacitor') else 1, r))
for ref in order:
    c = comps[ref]
    b.auto_place(ref, c['fp'], c['val'], padnets.get(ref, {}), anchor_for(ref), W, H)
    placed.add(ref)

b.gnd_zones(W, H)
b.text('GLITCHWAVE 567 - MAIN  rev0.1  Illicit Apothecary', W/2, H-3)
b.text('BACK WALL: OUT / DC 9-18V ctr-neg / IN', 69, 5.5, size=1.0)
b.save()
print('main board written:', len(b.fps), 'footprints placed')

# ---- audit: pads outside board or overlapping ------------------------------
bad = []
for ref, fp in b.fps.items():
    for p in fp.Pads():
        x, y = pcbnew.ToMM(p.GetPosition().x), pcbnew.ToMM(p.GetPosition().y)
        if not (0.3 < x < W-0.3 and 0.3 < y < H-0.3):
            bad.append((ref, p.GetNumber(), round(x,1), round(y,1)))
print('PADS OUTSIDE BOARD:', bad if bad else 'none')
