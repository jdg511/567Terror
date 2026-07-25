"""Surgical DRC fix: parse a KiCad DRC report, delete offender tracks/vias
near each violation, move FB100 clear of J5, dedupe co-located same-net vias,
then report affected nets for exact rerouting.

All board reads happen BEFORE any Remove (pcbnew swig proxies go stale after
batched removals). Usage: python3 surgical.py <pcb> <drc.json>
"""
import sys, json, math, re
sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
import pcbnew
from router import POWER
import microroute
from microroute import to_mm, mm

FIX_TYPES = {'shorting_items', 'clearance', 'hole_clearance'}


def width_of(nm):
    return POWER.get(nm.lstrip('/'), POWER.get(nm, 0.25))


def offender_net(n0, n1, k0, k1):
    def rippable(n, k):
        return k in ('Track', 'Via') and n and n != 'GND' and not n.startswith('unconnected-')
    a_ok, b_ok = rippable(n0, k0), rippable(n1, k1)
    if a_ok and not b_ok:
        return n0
    if b_ok and not a_ok:
        return n1
    if not a_ok and not b_ok:
        return None
    a_p, b_p = n0.lstrip('/') in POWER, n1.lstrip('/') in POWER
    if a_p and not b_p:
        return n1
    if b_p and not a_p:
        return n0
    return n0 if width_of(n0) <= width_of(n1) else n1


def parse_item(it):
    d = it['description']
    kind = d.split()[0]
    m = re.search(r'\[([^\]]+)\]', d)
    return kind, (m.group(1) if m else ''), (it['pos']['x'], it['pos']['y'])


def main(pcb, drc_json):
    d = json.load(open(drc_json))
    board = pcbnew.LoadBoard(pcb)
    byname = {str(k): v for k, v in board.GetNetsByName().items()}
    names = {v.GetNetCode(): str(k) for k, v in board.GetNetsByName().items()}

    # ---------- PHASE 1: read everything ----------
    tracks_data = []   # (uuid, nc, is_via, x0,y0,x1,y1)
    uuid_to_obj = {}
    for t in board.GetTracks():
        uid = t.m_Uuid.AsString()
        uuid_to_obj[uid] = t
        if t.GetClass() == 'PCB_VIA':
            p = t.GetPosition()
            tracks_data.append((uid, t.GetNetCode(), True,
                                to_mm(p.x), to_mm(p.y), to_mm(p.x), to_mm(p.y)))
        else:
            s, e = t.GetStart(), t.GetEnd()
            tracks_data.append((uid, t.GetNetCode(), False,
                                to_mm(s.x), to_mm(s.y), to_mm(e.x), to_mm(e.y)))
    fb = None
    for fp in board.GetFootprints():
        if fp.GetReference() == 'FB100':
            fb = fp
    fb_pos = (to_mm(fb.GetPosition().x), to_mm(fb.GetPosition().y)) if fb else None
    fb_nets = [p.GetNet().GetNetname() for p in fb.Pads() if p.GetNet()] if fb else []
    fb_pad_shapes = [(to_mm(p.GetBoundingBox().GetCenter().x),
                      to_mm(p.GetBoundingBox().GetCenter().y),
                      to_mm(p.GetBoundingBox().GetWidth())/2,
                      to_mm(p.GetBoundingBox().GetHeight())/2) for p in fb.Pads()] if fb else []
    other_shapes = []
    if fb:
        fx, fy = fb_pos
        for fp in board.GetFootprints():
            if fp.GetReference() == 'FB100':
                continue
            for p in fp.Pads():
                pp = p.GetPosition()
                if math.hypot(to_mm(pp.x)-fx, to_mm(pp.y)-fy) < 12:
                    other_shapes.append(p.GetEffectiveShape())

    # ---------- PHASE 2: decide ----------
    rip = []
    for v in d['violations']:
        if v['type'] not in FIX_TYPES:
            continue
        if v['type'] == 'clearance':
            m = re.search(r'actual ([\d.]+)', v.get('description', ''))
            if m and float(m.group(1)) >= 0.13:
                continue
        items = v['items']
        if len(items) < 2:
            continue
        k0, n0, p0 = parse_item(items[0])
        k1, n1, p1 = parse_item(items[1])
        off = offender_net(n0, n1, k0, k1)
        if off is None:
            continue
        pos = p0 if (off == n0 and k0 in ('Track', 'Via')) else p1
        rip.append((off, pos))

    RAD = 2.0
    to_remove, affected = set(), set()
    for (off, (px, py)) in rip:
        net = byname.get(off)
        if net is None:
            continue
        nc = net.GetNetCode()
        for (uid, tnc, is_via, x0, y0, x1, y1) in tracks_data:
            if tnc != nc or uid in to_remove:
                continue
            dx, dy = x1-x0, y1-y0
            L2 = dx*dx+dy*dy
            tt = 0 if L2 == 0 else max(0, min(1, ((px-x0)*dx+(py-y0)*dy)/L2))
            if math.hypot(px-(x0+tt*dx), py-(y0+tt*dy)) <= RAD:
                to_remove.add(uid); affected.add(off)

    # co-located / near-duplicate same-net vias
    kept = []
    colo = 0
    for (uid, tnc, is_via, x0, y0, x1, y1) in tracks_data:
        if not is_via or uid in to_remove:
            continue
        dup = False
        for (kx, ky, knc) in kept:
            if knc == tnc and math.hypot(x0-kx, y0-ky) < 0.35:
                dup = True
                break
        if dup:
            to_remove.add(uid); colo += 1
            if names.get(tnc):
                affected.add(names[tnc])
        else:
            kept.append((x0, y0, tnc))

    # FB100 relocation
    fb_target = None
    if fb:
        fx, fy = fb_pos
        def spot_ok(nx, ny):
            ox, oy = nx-fx, ny-fy
            for (cx, cy, hw, hh) in fb_pad_shapes:
                r = min(hw, hh)
                seg = pcbnew.SHAPE_SEGMENT(
                    pcbnew.VECTOR2I(mm(cx+ox-(hw-r)), mm(cy+oy)),
                    pcbnew.VECTOR2I(mm(cx+ox+(hw-r)), mm(cy+oy)), mm(2*r))
                for osh in other_shapes:
                    if osh.Collide(seg, mm(0.4)):
                        return False
            return True
        for r in (1.0, 1.5, 2.0, 2.5, 3.0, 4.0):
            if fb_target:
                break
            for ang in range(0, 360, 30):
                nx = fx + r*math.cos(math.radians(ang))
                ny = fy + r*math.sin(math.radians(ang))
                if microroute.in_board(nx, ny) and spot_ok(nx, ny):
                    fb_target = (nx, ny)
                    break
        if fb_target:
            fb_nc = {byname[n].GetNetCode() for n in fb_nets if n in byname and n != 'GND'}
            for (uid, tnc, is_via, x0, y0, x1, y1) in tracks_data:
                if tnc in fb_nc and not is_via and uid not in to_remove:
                    if math.hypot(x0-fx, y0-fy) < 4 or math.hypot(x1-fx, y1-fy) < 4:
                        to_remove.add(uid)
            affected.update(n for n in fb_nets if n != 'GND')

    # ---------- PHASE 3: mutate ----------
    for uid in to_remove:
        board.Remove(uuid_to_obj[uid])
    if fb and fb_target:
        fb.SetPosition(pcbnew.VECTOR2I(mm(fb_target[0]), mm(fb_target[1])))
        print(f'FB100 moved {fb_pos} -> ({fb_target[0]:.1f},{fb_target[1]:.1f})')
    elif fb:
        print('FB100: no clear spot found!')
    pcbnew.SaveBoard(pcb, board)
    print(f'removed {len(to_remove)} items ({colo} dup vias) across {len(affected)} nets')
    order = sorted(affected, key=lambda n: -width_of(n))
    print('NETS=' + ','.join(order))
    return order


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
