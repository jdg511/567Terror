"""Delete specific track segments by exact endpoint match. Surgical, auditable.

Usage:
  snip.py <board> "x0,y0,x1,y1" ["x0,y0,x1,y1" ...] [--apply]

Matches either direction. Refuses to run if any spec matches zero or more than
one segment, so a typo can never silently delete the wrong copper.
Dry run by default.
"""
import sys
import pcbnew

TOMM = 1e-6
TOL = 0.005


def main():
    path = sys.argv[1]
    apply_it = "--apply" in sys.argv
    specs = []
    for a in sys.argv[2:]:
        if a.startswith("--"):
            continue
        specs.append(tuple(float(v) for v in a.split(",")))

    bd = pcbnew.LoadBoard(path)
    doomed = []
    ok = True
    for sp in specs:
        hits = []
        for t in bd.GetTracks():
            if isinstance(t, pcbnew.PCB_VIA):
                continue
            s, e = t.GetStart(), t.GetEnd()
            a = (s.x * TOMM, s.y * TOMM, e.x * TOMM, e.y * TOMM)
            b = (e.x * TOMM, e.y * TOMM, s.x * TOMM, s.y * TOMM)
            for cand in (a, b):
                if all(abs(cand[i] - sp[i]) < TOL for i in range(4)):
                    hits.append(t)
                    break
        expect = int(sp[4]) if len(sp) > 4 else 1
        print("spec %s -> %d match(es), expected %d" % (str(sp[:4]), len(hits), expect))
        for t in hits:
            print("     %-22s %-7s len %.4f" % (
                t.GetNetname(), bd.GetLayerName(t.GetLayer()),
                t.GetLength() * TOMM))
        if len(hits) != expect:
            ok = False
        doomed += hits

    if not ok:
        print("\nABORT: every spec must match exactly one segment.")
        return 1
    if not apply_it:
        print("\nDRY RUN - nothing written.")
        return 0
    for t in doomed:
        bd.Remove(t)
    bd.Save(path)
    print("\nremoved %d, WROTE %s" % (len(doomed), path))
    return 0


if __name__ == "__main__":
    sys.exit(main())
