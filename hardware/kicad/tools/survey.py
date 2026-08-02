"""Read-only board survey. Usage: survey.py <board.kicad_pcb>

Prints, in mm:
  - board outline extents (Edge.Cuts)
  - every footprint: ref, value, pos, rot, THT/SMD, true courtyard bbox, F.Fab bbox, pad bbox
  - layer names
Never mutates. Safe to run any time.
"""
import sys
import pcbnew

TOMM = 1e-6


def bbox_of(shapes):
    xs, ys = [], []
    for s in shapes:
        b = s.GetBoundingBox()
        xs += [b.GetLeft() * TOMM, b.GetRight() * TOMM]
        ys += [b.GetTop() * TOMM, b.GetBottom() * TOMM]
    if not xs:
        return None
    return (min(xs), min(ys), max(xs), max(ys))


def poly_bbox(poly):
    if poly is None or poly.OutlineCount() == 0:
        return None
    b = poly.BBox()
    return (b.GetLeft() * TOMM, b.GetTop() * TOMM,
            b.GetRight() * TOMM, b.GetBottom() * TOMM)


def fmt(b):
    if b is None:
        return "        -                    "
    return "x%8.3f..%8.3f y%8.3f..%8.3f" % (b[0], b[2], b[1], b[3])


def main(path):
    bd = pcbnew.LoadBoard(path)
    print("BOARD:", path)
    print("layers:", bd.GetCopperLayerCount(), "copper")
    names = {}
    for lid in bd.GetEnabledLayers().Seq():
        names[lid] = bd.GetLayerName(lid)
    print("enabled layers:", {k: v for k, v in sorted(names.items())})

    eb = bd.GetBoardEdgesBoundingBox()
    print("edge.cuts extents: x %.3f..%.3f  y %.3f..%.3f  (%.2f x %.2f mm)" % (
        eb.GetLeft() * TOMM, eb.GetRight() * TOMM,
        eb.GetTop() * TOMM, eb.GetBottom() * TOMM,
        eb.GetWidth() * TOMM, eb.GetHeight() * TOMM))

    print()
    print("%-8s %-26s %9s %9s %5s %4s  %-42s %-42s" % (
        "REF", "VALUE", "X", "Y", "ROT", "TYPE", "COURTYARD (F.CrtYd)", "PADS bbox"))
    fps = sorted(bd.GetFootprints(), key=lambda f: f.GetReference())
    for f in fps:
        p = f.GetPosition()
        tht = any(pad.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH)
                  for pad in f.Pads())
        cy = poly_bbox(f.GetCourtyard(pcbnew.F_CrtYd))
        if cy is None:
            cy = poly_bbox(f.GetCourtyard(pcbnew.B_CrtYd))
        pb = bbox_of(list(f.Pads()))
        print("%-8s %-26s %9.3f %9.3f %5.0f %4s  %s %s" % (
            f.GetReference(), (f.GetValue() or "")[:26],
            p.x * TOMM, p.y * TOMM, f.GetOrientationDegrees(),
            "THT" if tht else "SMD", fmt(cy), fmt(pb)))

    print()
    print("track/via counts by layer:")
    counts = {}
    for t in bd.GetTracks():
        ln = bd.GetLayerName(t.GetLayer())
        key = "VIA" if isinstance(t, pcbnew.PCB_VIA) else ln
        counts[key] = counts.get(key, 0) + 1
    for k in sorted(counts):
        print("   %-10s %d" % (k, counts[k]))

    print()
    print("zones:")
    for z in bd.Zones():
        zb = z.GetBoundingBox()
        lays = [bd.GetLayerName(l) for l in z.GetLayerSet().Seq()]
        print("   net=%-24s layers=%-24s x%.1f..%.1f y%.1f..%.1f" % (
            z.GetNetname(), ",".join(lays),
            zb.GetLeft() * TOMM, zb.GetRight() * TOMM,
            zb.GetTop() * TOMM, zb.GetBottom() * TOMM))


if __name__ == "__main__":
    main(sys.argv[1])
