"""Exact-geometry finalize: GND via-in-pad + stitching with real collision
checks (SHAPE.Collide), island removal, zone fill.

Usage: python3 finalize2.py <pcb>
"""
import sys, math
sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
import pcbnew
from microroute import Obstacles, LAYERS, mm, to_mm, in_board, VIA_D, VIA_DRILL, CLR


def clean_via_spot(obs, gnd_vias, x, y):
    """True if a GND via at (x,y) collides with nothing foreign on any layer,
    keeps hole spacing, and isn't co-located with an existing via."""
    for L in range(len(LAYERS)):
        if not obs.seg_clear(L, x, y, x, y, width=VIA_D, clr=CLR):
            return False
    for (hx, hy, hd) in obs.holes:
        if math.hypot(x-hx, y-hy) < (VIA_DRILL+hd)/2 + 0.3:
            return False
    for (vx, vy) in gnd_vias:
        if math.hypot(x-vx, y-vy) < 0.6:
            return False
    return True


def finalize(pcb, W=138, H=114, stitch_step=9):
    board = pcbnew.LoadBoard(pcb)
    gnd_net = None
    for nm, net in board.GetNetsByName().items():
        if str(nm) == 'GND':
            gnd_net = net
    gnd = gnd_net.GetNetCode()
    # obstacles = all non-GND copper, whole board
    obs = Obstacles(board, gnd, (0, 0, W, H))
    gnd_vias = [(to_mm(t.GetPosition().x), to_mm(t.GetPosition().y))
                for t in board.GetTracks()
                if t.GetClass() == 'PCB_VIA' and t.GetNetCode() == gnd]

    def add_via(x, y):
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
        v.SetDrill(mm(VIA_DRILL)); v.SetWidth(mm(VIA_D))
        v.SetNet(gnd_net); board.Add(v)
        gnd_vias.append((x, y))
        obs.holes.append((x, y, VIA_DRILL))

    added = skipped = 0
    for fp in board.GetFootprints():
        for p in fp.Pads():
            if p.GetNetCode() != gnd or p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
                continue
            sz = p.GetSize()
            if min(to_mm(sz.x), to_mm(sz.y)) < 0.75:
                continue
            pos = p.GetPosition()
            x, y = to_mm(pos.x), to_mm(pos.y)
            if clean_via_spot(obs, gnd_vias, x, y):
                add_via(x, y); added += 1
            else:
                skipped += 1
    stitched = 0
    for ymm in range(6, H-4, stitch_step):
        for xmm in range(6, W-4, stitch_step):
            if not in_board(xmm, ymm):
                continue
            if clean_via_spot(obs, gnd_vias, float(xmm), float(ymm)):
                add_via(float(xmm), float(ymm)); stitched += 1
    for z in board.Zones():
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    filler = pcbnew.ZONE_FILLER(board)
    filler.Fill(board.Zones())
    board.BuildConnectivity()
    un = board.GetConnectivity().GetUnconnectedCount(True)
    pcbnew.SaveBoard(pcb, board)
    return added, skipped, stitched, un


if __name__ == '__main__':
    a, s, st, u = finalize(sys.argv[1])
    print(f'{a} via-in-pad ({s} skipped), {st} stitch, unconnected={u}')
