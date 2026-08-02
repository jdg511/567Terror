"""Whole-board audit that DRC does not do. Read-only.

Usage: audit.py <board.kicad_pcb>

Reports:
  1. EXACT duplicate track segments (same net, layer, endpoints) - a net routed
     twice. KiCad DRC is silent about these; they inflate the file, confuse any
     rip-and-reroute, and mask dangling stubs (two coincident stubs each satisfy
     the other's endpoint-support test).
  2. Physical footprint-body overlaps using true F.CrtYd where present and the
     union of pads + F.Fab GRAPHICS otherwise (never fp_text - a long value
     string inflates a bbox into a phantom obstacle). Same-side pairs only:
     a part on F cannot hit a part on B.
  3. Footprints with no courtyard at all, so you know which entries in (2) are
     estimates rather than the part's declared keepout.
"""
import sys
import pcbnew

TOMM = 1e-6
FAB = (pcbnew.F_Fab, pcbnew.B_Fab)


def poly_bbox(poly):
    if poly is None or poly.OutlineCount() == 0:
        return None
    b = poly.BBox()
    return (b.GetLeft(), b.GetTop(), b.GetRight(), b.GetBottom())


def fp_box(f):
    cy = poly_bbox(f.GetCourtyard(pcbnew.F_CrtYd)) or poly_bbox(f.GetCourtyard(pcbnew.B_CrtYd))
    if cy:
        return cy, True
    xs, ys = [], []
    for p in f.Pads():
        b = p.GetBoundingBox()
        xs += [b.GetLeft(), b.GetRight()]
        ys += [b.GetTop(), b.GetBottom()]
    for it in f.GraphicalItems():
        if it.GetLayer() not in FAB:
            continue
        if isinstance(it, (pcbnew.PCB_TEXT, pcbnew.PCB_TEXTBOX)):
            continue
        b = it.GetBoundingBox()
        xs += [b.GetLeft(), b.GetRight()]
        ys += [b.GetTop(), b.GetBottom()]
    if not xs:
        return None, False
    return (min(xs), min(ys), max(xs), max(ys)), False


def main():
    path = sys.argv[1]
    bd = pcbnew.LoadBoard(path)
    print("=" * 72)
    print("AUDIT:", path)
    print("=" * 72)

    # ---- 1. duplicate segments -------------------------------------------------
    seen, dupes = {}, []
    for t in bd.GetTracks():
        if isinstance(t, pcbnew.PCB_VIA):
            continue
        s, e = t.GetStart(), t.GetEnd()
        a = (round(s.x * TOMM, 4), round(s.y * TOMM, 4))
        b = (round(e.x * TOMM, 4), round(e.y * TOMM, 4))
        key = (t.GetNetCode(), t.GetLayer(), min(a, b), max(a, b))
        if key in seen:
            dupes.append(t)
        else:
            seen[key] = t
    print()
    print("[1] duplicate track segments: %d" % len(dupes))
    per = {}
    for t in dupes:
        per[t.GetNetname()] = per.get(t.GetNetname(), 0) + 1
    for n, c in sorted(per.items(), key=lambda kv: -kv[1]):
        print("      %-28s %d" % (n, c))

    # ---- 2. body overlaps ------------------------------------------------------
    fps = list(bd.GetFootprints())
    boxes = {}
    nocy = []
    for f in fps:
        box, has = fp_box(f)
        boxes[f.GetReference()] = (box, f.IsFlipped())
        if not has:
            nocy.append(f.GetReference())
    hits = []
    refs = sorted(boxes)
    for i, ra in enumerate(refs):
        ba, fa = boxes[ra]
        if ba is None:
            continue
        for rb in refs[i + 1:]:
            bb, fb = boxes[rb]
            if bb is None or fa != fb:
                continue
            x0, y0 = max(ba[0], bb[0]), max(ba[1], bb[1])
            x1, y1 = min(ba[2], bb[2]), min(ba[3], bb[3])
            if x1 > x0 and y1 > y0:
                hits.append(((x1 - x0) * (y1 - y0) * TOMM * TOMM, ra, rb,
                             (x1 - x0) * TOMM, (y1 - y0) * TOMM))
    hits.sort(reverse=True)
    print()
    print("[2] footprint body overlaps (same side): %d" % len(hits))
    for a, ra, rb, w, h in hits:
        est = " (est: no courtyard)" if (ra in nocy or rb in nocy) else ""
        print("      %8.2f mm2  %-6s x %-6s  %.3f x %.3f%s" % (a, ra, rb, w, h, est))

    print()
    print("[3] footprints with NO courtyard: %d" % len(nocy))
    print("      " + ", ".join(sorted(nocy)) if nocy else "      none")


if __name__ == "__main__":
    main()
