"""CONTROL board — same 138x114 notched outline (same enclosure frame as
main, no mirror: header drops straight down onto main J10 at identical XY).
Pots 3+3 grid, section LEDs between rows, stomps at the front."""
import sys; sys.path.insert(0,'/home/claude/work/hardware/kicad/tools')
import pcbgen, pcbnew

W, H = 138.0, 114.0
comps, padnets, nets = pcbgen.parse_netlist('/tmp/ctrl.net')
b = pcbgen.Board('/home/claude/work/hardware/kicad/glitchwave567_ctrl/glitchwave567_ctrl.kicad_pcb')
b.make_nets(nets)
b.outline_notched(W, H)

def place(ref, x, y, rot=0):
    c = comps[ref]
    return b.add_part(ref, c['fp'], c['val'], padnets.get(ref, {}), x, y, rot)

# pots: shaft = footprint origin -> these ARE the drill-template knob centers
for ref, x, y in [('RV1',34,22),('RV2',69,22),('RV3',104,22),
                  ('RV4',34,50),('RV5',69,50),('RV6',104,50)]:
    place(ref, x, y, 0)
# WS2812 sections between rows; tempo/bypass/gate row
for ref, x, y in [('LED1',34,36),('LED2',69,36),('LED3',104,36),
                  ('LED4',34,76),('LED6',69,76),('LED5',104,76)]:
    place(ref, x, y, 0)
# stomps: button = origin -> drill-template stomp centers (D12.2 + keyway)
place('SW1', 34, 96, 0)
place('SW2', 104, 96, 0)
# header: SAME enclosure XY as main J10 (96,110) - bottom-mounted, mates down
place('J1', 96, 104, 90)
place('C7', 122, 76, 0)   # 100u bulk

placed = set(b.fps.keys())
def anchor_for(ref):
    for pin, net in padnets.get(ref, {}).items():
        if net in ('GND','+5V','3V3'): continue
        for r2, pn2 in padnets.items():
            if r2 in placed and r2 != ref and net in pn2.values():
                p = b.fps[r2].GetPosition()
                return (pcbnew.ToMM(p.x), pcbnew.ToMM(p.y))
    return (69, 60)
for ref in sorted(r for r in comps if r not in placed):
    c = comps[ref]
    b.auto_place(ref, c['fp'], c['val'], padnets.get(ref, {}), anchor_for(ref), W, H)
    placed.add(ref)

b.gnd_zones(W, H)
b.text('GLITCHWAVE 567 - CONTROL  rev0.1  Illicit Apothecary', W/2, H-3)
b.text('FREQ        GAIN        MIX', 69, 14, size=1.5)
b.text('FIZZ         Q          VOL', 69, 58, size=1.5)
b.text('TAP', 34, 104, size=1.5); b.text('BYPASS', 104, 104, size=1.5)
b.save()
print('ctrl board written:', len(b.fps), 'footprints')
