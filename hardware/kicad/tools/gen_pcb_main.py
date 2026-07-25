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
place_wall_jack('J2', 28, 1.6, 'down')     # OUT 6.35
place_wall_jack('J5', 69, 2.6, 'down')     # DC (nose to wall)
place_wall_jack('J1', 110, 1.6, 'down')    # IN 6.35
place_wall_jack('J3', 1.6, 40, 'right')    # CV1
place_wall_jack('J4', 1.6, 75, 'right')    # CV2
place_wall_jack('J6', 136.4, 57, 'left')   # CV OUT

# ---- power region (top center) --------------------------------------------
place('Q10', 55, 12, 90)      # reverse P-FET
place('FB100', 61, 16, 90)
place('D106', 50, 20, 0)      # TVS
place('U19', 88, 16, 0)       # MP1584
place('L100', 99, 30, 0)
place('D101', 90, 24, 90)
place('U18', 80, 27, 0)       # 78L09
place('D105', 87, 31, 90)
place('U6', 55, 30, 0)        # util TL074 (VREF + starve)
place('Q11', 64, 34, 0)       # BCP56

# ---- audio chain: right wall -> left --------------------------------------
place('U1', 120, 42, 0)       # input TL074
place('U2', 120, 62, 0)       # trim + dirt I-V TL074
place('U3', 106, 72, 0)       # dirt VCA LM13700
place('Q1', 120, 86, 0)       # Bazz Fuss darlington
place('U5', 97, 50, 0)        # LM567  <-- the heart
place('U7', 97, 66, 0)        # timing OTA
place('U8', 96, 86, 0)        # CD4052 timing caps
place('U9', 70, 42, 0)        # mix/SVF TL074
place('U10', 70, 64, 0)       # mix VCAs
place('U11', 48, 50, 0)       # SVF integrators
place('U12', 48, 74, 0)       # Q VCA
place('U13', 72, 86, 0)       # CD4051 mode
place('U14', 38, 44, 0)       # output voicing TL074
place('U15', 38, 60, 0)       # gate/bypass VCAs
place('U16', 24, 52, 0)       # out I-V + buffer
place('SW1', 14, 80, 0); place('SW2', 23, 80, 0); place('SW3', 32, 80, 0)

# ---- digital / interface (bottom) -----------------------------------------
place('U20', 38, 98, 90)      # Pico, long axis horizontal (left zone)
place('U21', 86, 96, 0)       # 74HC4067 (beside header)
place('U22', 84, 110, 0)      # AHCT1G125
place('U17', 124, 78, 0)       # env follower TL074
place('J10', 96, 104, 90)      # control header
place('RV1', 108, 109, 0); place('RV2', 116, 109, 0); place('RV3', 124, 109, 0)
# env follower output chain - explicit with routing room (ENV_B was walled)
place('R137', 113, 99, 90)
place('R138', 119, 102, 90)
place('R139', 119, 96, 90)
place('D83', 124, 99, 90)

# ---- auto-place everything else near an anchor ----------------------------
placed = set(b.fps.keys())
def anchor_for(ref):
    xs, ys = [], []
    for pin, net in padnets.get(ref, {}).items():
        if net in POWER_NETS: continue
        for r2, pn2 in padnets.items():
            if r2 in placed and r2 != ref and net in pn2.values():
                p = b.fps[r2].GetPosition()
                xs.append(pcbnew.ToMM(p.x)); ys.append(pcbnew.ToMM(p.y))
    if xs:
        return (sum(xs)/len(xs), sum(ys)/len(ys))
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
