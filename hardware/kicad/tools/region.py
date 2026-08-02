"""Read-only region query.

Usage: region.py <board> <x0> <y0> <x1> <y1> [--copper]

Lists every footprint whose courtyard (or pad bbox, if no courtyard) intersects the
window, plus each one's pads and nets. With --copper it also lists every track
segment and via in the window, grouped by layer and net, so you can see what a
through-hole pad would land on.
"""
import sys
import pcbnew

TOMM = 1e-6


def poly_bbox(poly):
    if poly is None or poly.OutlineCount() == 0:
        return None
    b = poly.BBox()
    return (b.GetLeft() * TOMM, b.GetTop() * TOMM,
            b.GetRight() * TOMM, b.GetBottom() * TOMM)


def pad_bbox(f):
    xs, ys = [], []
    for p in f.Pads():
        b = p.GetBoundingBox()
        xs += [b.GetLeft() * TOMM, b.GetRight() * TOMM]
        ys += [b.GetTop() * TOMM, b.GetBottom() * TOMM]
    return (min(xs), min(ys), max(xs), max(ys)) if xs else None


def hit(b, w):
    return b and not (b[2] < w[0] or b[0] > w[2] or b[3] < w[1] or b[1] > w[3])


def main():
    path = sys.argv[1]
    win = tuple(float(v) for v in sys.argv[2:6])
    copper = "--copper" in sys.argv
    bd = pcbnew.LoadBoard(path)
    print("window x %.3f..%.3f  y %.3f..%.3f" % (win[0], win[2], win[1], win[3]))
    print()
    for f in sorted(bd.GetFootprints(), key=lambda f: f.GetReference()):
        cy = poly_bbox(f.GetCourtyard(pcbnew.F_CrtYd)) or poly_bbox(f.GetCourtyard(pcbnew.B_CrtYd))
        pb = pad_bbox(f)
        box = cy or pb
        if not hit(box, win):
            continue
        p = f.GetPosition()
        tht = any(pd.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH)
                  for pd in f.Pads())
        src = "crtyd" if cy else "pads "
        print("%-7s %-24s at(%8.3f,%8.3f) rot%4.0f %s  %s x%8.3f..%8.3f y%8.3f..%8.3f" % (
            f.GetReference(), (f.GetValue() or "")[:24], p.x * TOMM, p.y * TOMM,
            f.GetOrientationDegrees(), "THT" if tht else "SMD", src,
            box[0], box[2], box[1], box[3]))
        for pd in f.Pads():
            pp = pd.GetPosition()
            print("        pad %-4s (%8.3f,%8.3f) net=%s" % (
                pd.GetNumber(), pp.x * TOMM, pp.y * TOMM, pd.GetNetname()))

    if copper:
        print()
        print("COPPER in window:")
        rows = []
        for t in bd.GetTracks():
            b = t.GetBoundingBox()
            bb = (b.GetLeft() * TOMM, b.GetTop() * TOMM, b.GetRight() * TOMM, b.GetBottom() * TOMM)
            if not hit(bb, win):
                continue
            if isinstance(t, pcbnew.PCB_VIA):
                pos = t.GetPosition()
                rows.append(("VIA", t.GetNetname(),
                             "(%8.3f,%8.3f) d=%.2f" % (pos.x * TOMM, pos.y * TOMM,
                                                       t.GetWidth() * TOMM)))
            else:
                s, e = t.GetStart(), t.GetEnd()
                rows.append((bd.GetLayerName(t.GetLayer()), t.GetNetname(),
                             "(%8.3f,%8.3f)->(%8.3f,%8.3f) w=%.2f" % (
                                 s.x * TOMM, s.y * TOMM, e.x * TOMM, e.y * TOMM,
                                 t.GetWidth() * TOMM)))
        for r in sorted(rows):
            print("   %-8s %-26s %s" % r)
        print("   total:", len(rows))


if __name__ == "__main__":
    main()
