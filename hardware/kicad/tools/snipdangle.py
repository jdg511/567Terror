"""Iteratively remove dangling track stubs, restricted to a NET WHITELIST.

tools/dangling.py already does this board-wide. That is the wrong tool right
after a local rip-and-reroute: it would also delete stubs elsewhere on the
board that were not part of this change, so a regression could not be told
apart from a pre-existing one. This version only ever touches nets you name.

A segment endpoint counts as supported when it lands on a same-net pad, a
same-net via, or any point of another same-net segment on the same layer.
Removing a stub can expose the next one, so it loops until nothing changes.

Skips GND by default (zone-connected copper legitimately looks unsupported).

Usage: snipdangle.py <board> "net1|net2|..." [--apply] [--max-passes 12]
Dry run by default.
"""
import math
import sys
import pcbnew

TOMM = 1e-6
TOUCH = 0.11


def seg_pt_dist(x0, y0, x1, y1, px, py):
    dx, dy = x1 - x0, y1 - y0
    L2 = dx * dx + dy * dy
    t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((px - x0) * dx + (py - y0) * dy) / L2))
    return math.hypot(px - (x0 + t * dx), py - (y0 + t * dy))


def one_pass(bd, nets, verbose):
    """Return list of track objects to remove this pass."""
    segs, vias, pads = [], [], []
    for t in bd.GetTracks():
        if t.GetNetname() not in nets:
            continue
        if isinstance(t, pcbnew.PCB_VIA):
            p = t.GetPosition()
            vias.append((t, t.GetNetCode(), p.x * TOMM, p.y * TOMM, t.GetWidth() * TOMM / 2))
        else:
            s, e = t.GetStart(), t.GetEnd()
            segs.append((t, t.GetNetCode(), t.GetLayer(),
                         s.x * TOMM, s.y * TOMM, e.x * TOMM, e.y * TOMM))
    for f in bd.GetFootprints():
        for p in f.Pads():
            if p.GetNetname() not in nets:
                continue
            b = p.GetBoundingBox()
            pads.append((p.GetNetCode(), set(p.GetLayerSet().CuStack()),
                         b.GetCenter().x * TOMM, b.GetCenter().y * TOMM,
                         b.GetWidth() * TOMM / 2, b.GetHeight() * TOMM / 2))

    def supported(me, nc, lay, x, y):
        for (pnc, plays, cx, cy, hw, hh) in pads:
            if pnc == nc and lay in plays and abs(x - cx) <= hw + 0.05 and abs(y - cy) <= hh + 0.05:
                return True
        for (vt, vnc, vx, vy, vr) in vias:
            if vnc == nc and math.hypot(x - vx, y - vy) <= vr + 0.05:
                return True
        for (t2, nc2, lay2, ax, ay, bx, by) in segs:
            if t2 is me or nc2 != nc or lay2 != lay:
                continue
            if seg_pt_dist(ax, ay, bx, by, x, y) <= TOUCH:
                return True
        return False

    doomed = []
    for (t, nc, lay, x0, y0, x1, y1) in segs:
        if not supported(t, nc, lay, x0, y0) or not supported(t, nc, lay, x1, y1):
            doomed.append((t, (x0, y0, x1, y1)))
            if verbose:
                print("      %-22s %-7s (%8.3f,%8.3f)->(%8.3f,%8.3f) len %.4f" % (
                    t.GetNetname(), bd.GetLayerName(lay), x0, y0, x1, y1,
                    math.hypot(x1 - x0, y1 - y0)))
    return [d[0] for d in doomed]


def main():
    path = sys.argv[1]
    nets = set(n for n in sys.argv[2].split("|") if n and n != "GND")
    apply_it = "--apply" in sys.argv
    max_passes = 12
    if "--max-passes" in sys.argv:
        max_passes = int(sys.argv[sys.argv.index("--max-passes") + 1])

    bd = pcbnew.LoadBoard(path)
    print("nets in scope:", sorted(nets))
    total = 0

    # Pass 0: drop EXACT duplicate segments (same net, same layer, same two
    # endpoints). /mix_svf/SVF_SUM on this board carries several - a net that was
    # rerouted without deleting the old path. They matter here beyond tidiness:
    # two coincident stubs each satisfy the other's endpoint-support test, so a
    # dangling pair is invisible to the loop below until one of them is gone.
    seen, dupes = {}, []
    for t in bd.GetTracks():
        if isinstance(t, pcbnew.PCB_VIA) or t.GetNetname() not in nets:
            continue
        s, e = t.GetStart(), t.GetEnd()
        a = (round(s.x * TOMM, 4), round(s.y * TOMM, 4))
        b = (round(e.x * TOMM, 4), round(e.y * TOMM, 4))
        key = (t.GetNetCode(), t.GetLayer(), min(a, b), max(a, b))
        if key in seen:
            dupes.append(t)
        else:
            seen[key] = t
    print("   pass 0: %d exact duplicate segment(s)" % len(dupes))
    for t in dupes:
        s, e = t.GetStart(), t.GetEnd()
        print("      %-22s %-7s (%8.3f,%8.3f)->(%8.3f,%8.3f)" % (
            t.GetNetname(), bd.GetLayerName(t.GetLayer()),
            s.x * TOMM, s.y * TOMM, e.x * TOMM, e.y * TOMM))
    # ONE removal batch per process, then exit. pcbnew's swig proxies go stale
    # process-wide after a batched Remove, so a second pass in the same process
    # crashes on the next GetTracks() walk. The caller re-invokes until CLEAN.
    if dupes:
        if apply_it:
            for t in dupes:
                bd.Remove(t)
            bd.Save(path)
            print("CHANGED %d (duplicates) - WROTE %s" % (len(dupes), path))
        else:
            print("DRY RUN: would remove %d duplicates" % len(dupes))
        return

    print("   dangle pass:")
    doomed = one_pass(bd, nets, verbose=True)
    if not doomed:
        print("CLEAN - no duplicates, nothing dangling")
        return
    if apply_it:
        for t in doomed:
            bd.Remove(t)
        bd.Save(path)
        print("CHANGED %d (dangling) - WROTE %s" % (len(doomed), path))
    else:
        print("DRY RUN: would remove %d dangling" % len(doomed))


if __name__ == "__main__":
    main()
