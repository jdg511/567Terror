"""Fragment-aware GND stitching: after zone fill, place one via inside every
outer-layer GND fill fragment that contains no GND via / PTH pad, so each
fragment links to the In1 plane. Exact-geometry checks throughout.

Usage: python3 finalize3.py <pcb>
"""
import sys, math
sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
import pcbnew
from microroute import Obstacles, mm, to_mm, in_board, VIA_D, VIA_DRILL


def finalize3(pcb, W=138, H=114):
    board = pcbnew.LoadBoard(pcb)
    gnd_net = None
    for nm, net in board.GetNetsByName().items():
        if str(nm) == 'GND':
            gnd_net = net
    gnd = gnd_net.GetNetCode()
    # fill first so fragments are current
    for z in board.Zones():
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())

    obs = Obstacles(board, gnd, (0, 0, W, H))
    anchors = []   # existing GND vias + PTH GND pad centres
    for t in board.GetTracks():
        if t.GetClass() == 'PCB_VIA' and t.GetNetCode() == gnd:
            p = t.GetPosition()
            anchors.append((to_mm(p.x), to_mm(p.y)))
    for fp in board.GetFootprints():
        for p in fp.Pads():
            if p.GetNetCode() == gnd and p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
                pp = p.GetPosition()
                anchors.append((to_mm(pp.x), to_mm(pp.y)))

    def clean(x, y):
        for L in range(3):
            if not obs.seg_clear(L, x, y, x, y, width=VIA_D, clr=0.15):
                return False
        for (hx, hy, hd) in obs.holes:
            if math.hypot(x-hx, y-hy) < (VIA_DRILL+hd)/2 + 0.3:
                return False
        return True

    added = 0
    unfixed = []
    for z in board.Zones():
        if z.GetNetCode() != gnd:
            continue
        for lay in (pcbnew.F_Cu, pcbnew.B_Cu):
            if not z.IsOnLayer(lay):
                continue
            polys = z.GetFilledPolysList(lay)
            for i in range(polys.OutlineCount()):
                outline = polys.Outline(i)
                bb = outline.BBox()
                x0, y0 = to_mm(bb.GetLeft()), to_mm(bb.GetTop())
                x1, y1 = to_mm(bb.GetRight()), to_mm(bb.GetBottom())
                # does the fragment already contain an anchor?
                has = False
                for (ax, ay) in anchors:
                    if x0-0.1 <= ax <= x1+0.1 and y0-0.1 <= ay <= y1+0.1:
                        if outline.PointInside(pcbnew.VECTOR2I(mm(ax), mm(ay))):
                            has = True
                            break
                if has:
                    continue
                # find a clean via spot inside the fragment
                placed = False
                step = 1.0
                sx = x0
                while not placed and step >= 0.4:
                    y = y0
                    while y <= y1 and not placed:
                        x = sx
                        while x <= x1 and not placed:
                            if in_board(x, y) and \
                               outline.PointInside(pcbnew.VECTOR2I(mm(x), mm(y))) and \
                               clean(x, y):
                                v = pcbnew.PCB_VIA(board)
                                v.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
                                v.SetDrill(mm(VIA_DRILL)); v.SetWidth(mm(VIA_D))
                                v.SetNet(gnd_net); board.Add(v)
                                anchors.append((x, y))
                                obs.holes.append((x, y, VIA_DRILL))
                                added += 1
                                placed = True
                            x += step
                        y += step
                    step /= 2
                if not placed:
                    unfixed.append((board.GetLayerName(lay), round((x0+x1)/2, 1),
                                    round((y0+y1)/2, 1), round((x1-x0)*(y1-y0), 1)))
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.BuildConnectivity()
    un = board.GetConnectivity().GetUnconnectedCount(True)
    pcbnew.SaveBoard(pcb, board)
    return added, unfixed, un


if __name__ == '__main__':
    a, unfixed, un = finalize3(sys.argv[1])
    print(f'{a} fragment vias, unconnected={un}')
    for u in unfixed:
        print('  UNFIXED fragment:', u)
