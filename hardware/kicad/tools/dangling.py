"""Iteratively remove dangling track stubs and orphan vias (non-GND nets).

A track endpoint is supported if it touches (<=0.1mm) a same-net pad shape,
via, or another track (endpoint or body). A via is supported if same-net
copper touches it on >=2 layers, or a pad touches it. GND is skipped (zones).

Usage: python3 dangling.py <pcb>
"""
import sys, math
sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
import pcbnew
from microroute import to_mm, item_layers, LIDX

TOUCH = 0.11


def seg_pt_dist(x0, y0, x1, y1, px, py):
    dx, dy = x1-x0, y1-y0
    L2 = dx*dx+dy*dy
    t = 0 if L2 == 0 else max(0, min(1, ((px-x0)*dx+(py-y0)*dy)/L2))
    return math.hypot(px-(x0+t*dx), py-(y0+t*dy))


def main(pcb):
    board = pcbnew.LoadBoard(pcb)
    gnd = board.GetNetsByName()['GND'].GetNetCode()
    total_removed = 0
    for _ in range(1):
        # snapshot
        tracks = []   # (uuid, nc, layer_idx or None(via), x0,y0,x1,y1, halfw)
        uuid_obj = {}
        for t in board.GetTracks():
            uid = t.m_Uuid.AsString()
            uuid_obj[uid] = t
            if t.GetNetCode() == gnd:
                continue
            if t.GetClass() == 'PCB_VIA':
                p = t.GetPosition()
                tracks.append((uid, t.GetNetCode(), None, to_mm(p.x), to_mm(p.y),
                               to_mm(p.x), to_mm(p.y), to_mm(t.GetWidth())/2))
            else:
                L = LIDX.get(int(t.GetLayer()))
                s, e = t.GetStart(), t.GetEnd()
                tracks.append((uid, t.GetNetCode(), L, to_mm(s.x), to_mm(s.y),
                               to_mm(e.x), to_mm(e.y), to_mm(t.GetWidth())/2))
        pads = []   # (nc, layers, cx, cy, hw, hh)
        for fp in board.GetFootprints():
            for p in fp.Pads():
                if not p.GetNetCode() or p.GetNetCode() == gnd:
                    continue
                bb = p.GetBoundingBox()
                pads.append((p.GetNetCode(), tuple(item_layers(p)),
                             to_mm(bb.GetCenter().x), to_mm(bb.GetCenter().y),
                             to_mm(bb.GetWidth())/2, to_mm(bb.GetHeight())/2))
        by_net = {}
        for rec in tracks:
            by_net.setdefault(rec[1], []).append(rec)
        pads_by_net = {}
        for rec in pads:
            pads_by_net.setdefault(rec[0], []).append(rec)

        def pt_on_pad(nc, L, x, y):
            for (pnc, Ls, cx, cy, hw, hh) in pads_by_net.get(nc, ()):
                if L is not None and L not in Ls:
                    continue
                if abs(x-cx) <= hw+0.05 and abs(y-cy) <= hh+0.05:
                    return True
            return False

        def endpoint_supported(uid, nc, L, x, y):
            if pt_on_pad(nc, L, x, y):
                return True
            for rec in by_net.get(nc, ()):
                if rec[0] == uid:
                    continue
                (uid2, _, L2, ax, ay, bx, by, hw) = rec
                if L2 is None:   # via
                    if math.hypot(x-ax, y-ay) <= hw+0.05:
                        return True
                elif L2 == L:
                    if seg_pt_dist(ax, ay, bx, by, x, y) <= TOUCH:
                        return True
            return False

        remove = set()
        for (uid, nc, L, x0, y0, x1, y1, hw) in tracks:
            if L is None:
                layers_touched = set()
                pad_touch = False
                for Lc in (0, 1, 2):
                    if pt_on_pad(nc, Lc, x0, y0):
                        pad_touch = True
                        layers_touched.add(Lc)
                for rec in by_net.get(nc, ()):
                    (uid2, _, L2, ax, ay, bx, by, hw2) = rec
                    if uid2 == uid or L2 is None:
                        continue
                    if seg_pt_dist(ax, ay, bx, by, x0, y0) <= hw+0.05:
                        layers_touched.add(L2)
                if len(layers_touched) < 2 and not pad_touch:
                    remove.add(uid)
                elif len(layers_touched) < 2 and pad_touch and len(layers_touched) <= 1:
                    # via only touching the pad's own layer set and nothing else useful
                    pass
            else:
                if not endpoint_supported(uid, nc, L, x0, y0) or \
                   not endpoint_supported(uid, nc, L, x1, y1):
                    remove.add(uid)
        if not remove:
            break
        for uid in remove:
            board.Remove(uuid_obj[uid])
        total_removed += len(remove)
        pcbnew.SaveBoard(pcb, board)
    print(f'removed {total_removed} dangling items')


if __name__ == '__main__':
    main(sys.argv[1])
