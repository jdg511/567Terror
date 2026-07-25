"""Resolve courtyard/pad overlaps by relocating the smaller footprint of each
pair to the nearest clear spot. Read-all-then-mutate (swig staleness safe).

Usage: python3 mover.py <pcb> <pairs e.g. "C122:U18,C102:C100,...">
Movee is listed FIRST in each pair. Prints NETS= list needing reroute.
"""
import sys, math
sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
import pcbnew
import microroute as M
from microroute import to_mm, mm


MAX_R = 8.0
def main(pcb, pairs_arg):
    board = pcbnew.LoadBoard(pcb)
    pairs = [p.split(':') for p in pairs_arg.split(',')]
    fps = {fp.GetReference(): fp for fp in board.GetFootprints()}

    # ---- read phase ----
    # footprint model: ref -> (cx, cy, courtyard halfw, halfh, pads:[(px,py,netcode,hw,hh)])
    model = {}
    for ref, fp in fps.items():
        cyp = fp.GetCourtyard(pcbnew.F_CrtYd)
        if cyp.OutlineCount() > 0:
            bb = cyp.BBox()
            cx, cy = to_mm(bb.GetCenter().x), to_mm(bb.GetCenter().y)
            hw, hh = to_mm(bb.GetWidth())/2, to_mm(bb.GetHeight())/2
        else:
            bb = fp.GetBoundingBox(False, False)
            cx, cy = to_mm(bb.GetCenter().x), to_mm(bb.GetCenter().y)
            hw, hh = to_mm(bb.GetWidth())/2 + 0.25, to_mm(bb.GetHeight())/2 + 0.25
        pads = []
        for p in fp.Pads():
            pb = p.GetBoundingBox()
            pads.append((to_mm(pb.GetCenter().x), to_mm(pb.GetCenter().y),
                         p.GetNetCode(),
                         to_mm(pb.GetWidth())/2, to_mm(pb.GetHeight())/2))
        model[ref] = [cx, cy, hw, hh, pads]
    # all foreign copper segments/vias as data
    tracks_data = []
    uuid_to_obj = {}
    for t in board.GetTracks():
        uid = t.m_Uuid.AsString()
        uuid_to_obj[uid] = t
        if t.GetClass() == 'PCB_VIA':
            p = t.GetPosition()
            tracks_data.append((uid, t.GetNetCode(), True, to_mm(p.x), to_mm(p.y),
                                to_mm(p.x), to_mm(p.y), to_mm(t.GetWidth())/2))
        else:
            s, e = t.GetStart(), t.GetEnd()
            tracks_data.append((uid, t.GetNetCode(), False, to_mm(s.x), to_mm(s.y),
                                to_mm(e.x), to_mm(e.y), to_mm(t.GetWidth())/2))
    names = {v.GetNetCode(): str(k) for k, v in board.GetNetsByName().items()}

    def rects_overlap(ax, ay, ahw, ahh, bx, by, bhw, bhh, margin=0.0):
        return abs(ax-bx) < ahw+bhw+margin and abs(ay-by) < ahh+bhh+margin

    def seg_pt_dist(x0, y0, x1, y1, px, py):
        dx, dy = x1-x0, y1-y0
        L2 = dx*dx+dy*dy
        t = 0 if L2 == 0 else max(0, min(1, ((px-x0)*dx+(py-y0)*dy)/L2))
        return math.hypot(px-(x0+t*dx), py-(y0+t*dy))

    moves = {}       # ref -> (dx, dy)
    soft_rips = set()
    affected = set()

    def cur(ref):
        cx, cy, hw, hh, pads = model[ref]
        dx, dy = moves.get(ref, (0, 0))
        return cx+dx, cy+dy, hw, hh, [(px+dx, py+dy, nc, phw, phh) for (px, py, nc, phw, phh) in pads]

    def spot_ok(ref, dx, dy):
        cx, cy, hw, hh, pads = model[ref]
        ncx, ncy = cx+dx, cy+dy
        my_nets = {nc for (_, _, nc, _, _) in pads if nc}
        # board bounds incl. notches: courtyard corners
        for sx in (-1, 1):
            for sy in (-1, 1):
                if not M.in_board(ncx+sx*hw, ncy+sy*hh):
                    return None
        # courtyard vs all other courtyards (at their current planned pos)
        for oref in model:
            if oref == ref:
                continue
            ox, oy, ohw, ohh, _ = cur(oref)
            if rects_overlap(ncx, ncy, hw, hh, ox, oy, ohw, ohh):
                return None
        # pads vs foreign PADS (hard)
        for (px, py, nc, phw, phh) in pads:
            npx, npy = px+dx, py+dy
            for oref in model:
                if oref == ref:
                    continue
                ox2, oy2, _, _, opads = cur(oref)
                for (opx, opy, onc, ophw, ophh) in opads:
                    if onc and onc in my_nets and onc == nc:
                        continue
                    if rects_overlap(npx, npy, phw, phh, opx, opy, ophw, ophh, margin=0.15):
                        return None
        # pads vs foreign tracks/vias (SOFT: collect conflicts to rip)
        conf = set()
        for (px, py, nc, phw, phh) in pads:
            npx, npy = px+dx, py+dy
            prad = max(phw, phh)
            for (uid, tnc, is_via, x0, y0, x1, y1, thw) in tracks_data:
                if tnc in my_nets:
                    continue
                if seg_pt_dist(x0, y0, x1, y1, npx, npy) < prad + thw + 0.3:
                    conf.add(uid)
        return conf

    for movee, fixed in pairs:
        if movee not in model:
            print('missing', movee)
            continue
        mx, my, mhw, mhh, mpads = cur(movee)
        fx, fy, fhw, fhh, _ = cur(fixed)
        if not rects_overlap(mx, my, mhw, mhh, fx, fy, fhw, fhh):
            print(f'{movee}/{fixed}: already clear')
            continue
        done = False
        best = None
        for r in [x*0.5 for x in range(1, int(MAX_R*2)+1)]:
            for ang in range(0, 360, 10):
                dx = moves.get(movee, (0, 0))[0] + r*math.cos(math.radians(ang))
                dy = moves.get(movee, (0, 0))[1] + r*math.sin(math.radians(ang))
                conf = spot_ok(movee, dx, dy)
                if conf is None:
                    continue
                if len(conf) == 0:
                    best = (dx, dy, conf)
                    break
                if best is None or len(conf) < len(best[2]):
                    best = (dx, dy, conf)
            if best is not None and len(best[2]) == 0:
                break
        if best is not None:
            dx, dy, conf = best
            moves[movee] = (dx, dy)
            soft_rips.update(conf)
            print(f'{movee}: moved by ({dx:+.2f},{dy:+.2f}) to clear {fixed} (rips {len(conf)} foreign segs)')
            done = True
        if not done:
            print(f'{movee}: NO CLEAR SPOT vs {fixed}')

    # rip tracks touching moved parts' old pad positions
    to_remove = set(soft_rips)
    for (uid, tnc, is_via, x0, y0, x1, y1, thw) in tracks_data:
        if uid in soft_rips and names.get(tnc):
            affected.add(names[tnc])
    for ref in moves:
        _, _, _, _, pads = model[ref]
        for (px, py, nc, phw, phh) in pads:
            if not nc or names.get(nc) == 'GND':
                if nc:
                    affected.add('GND')
                continue
            affected.add(names.get(nc, ''))
            for (uid, tnc, is_via, x0, y0, x1, y1, thw) in tracks_data:
                if tnc != nc or uid in to_remove:
                    continue
                if (math.hypot(x0-px, y0-py) < max(phw, phh)+0.3 or
                        math.hypot(x1-px, y1-py) < max(phw, phh)+0.3):
                    to_remove.add(uid)

    # ---- mutate phase ----
    for uid in to_remove:
        board.Remove(uuid_to_obj[uid])
    for ref, (dx, dy) in moves.items():
        fp = fps[ref]
        p = fp.GetPosition()
        fp.SetPosition(pcbnew.VECTOR2I(p.x + mm(dx), p.y + mm(dy)))
    pcbnew.SaveBoard(pcb, board)
    affected.discard('')
    affected.discard('GND')
    print(f'moved {len(moves)} parts, removed {len(to_remove)} track items')
    print('NETS=' + ','.join(sorted(affected)))


if __name__ == '__main__':
    if len(sys.argv) > 3:
        MAX_R = float(sys.argv[3])
    main(sys.argv[1], sys.argv[2])
