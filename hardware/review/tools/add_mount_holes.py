"""Add M3 standoff mounting holes to a board, with clearance verification.

ENCLOSURE_FIT.md: "Control-board retention must come from standoffs" and its
required-changes list asks for four M3 holes on the CONTROL board. There were none.

Places NPTH (non-plated) holes, which is what you want for a standoff/screw: no
annular ring, no net, no zone connection, and DRC treats them as holes rather than
pads. Verifies each position against the real Edge.Cuts outline (this board has
11 x 11 mm corner notches, so a bounding-box test is not good enough), every
footprint body and every track, before writing anything.

Usage:
  add_mount_holes.py <board.kicad_pcb> x,y [x,y ...] [--drill 3.2] [--keepout 7.0] [--apply]
Dry run by default.
"""
import math
import sys

import pcbnew

TOMM = 1e-6
TONM = 1000000.0
FAB = (pcbnew.F_Fab, pcbnew.B_Fab)


def fp_box(f):
    cy = f.GetCourtyard(pcbnew.F_CrtYd)
    if cy.OutlineCount() == 0:
        cy = f.GetCourtyard(pcbnew.B_CrtYd)
    if cy.OutlineCount() > 0:
        b = cy.BBox()
        return (b.GetLeft() * TOMM, b.GetTop() * TOMM,
                b.GetRight() * TOMM, b.GetBottom() * TOMM)
    xs, ys = [], []
    for p in f.Pads():
        b = p.GetBoundingBox()
        xs += [b.GetLeft() * TOMM, b.GetRight() * TOMM]
        ys += [b.GetTop() * TOMM, b.GetBottom() * TOMM]
    for it in f.GraphicalItems():
        if it.GetLayer() in FAB and not isinstance(it, (pcbnew.PCB_TEXT, pcbnew.PCB_TEXTBOX)):
            b = it.GetBoundingBox()
            xs += [b.GetLeft() * TOMM, b.GetRight() * TOMM]
            ys += [b.GetTop() * TOMM, b.GetBottom() * TOMM]
    return (min(xs), min(ys), max(xs), max(ys)) if xs else None


def main():
    path = sys.argv[1]
    apply_it = "--apply" in sys.argv
    drill, keep = 3.2, 7.0
    pts = []
    args = sys.argv[2:]
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--drill":
            drill = float(args[i + 1]); i += 2; continue
        if a == "--keepout":
            keep = float(args[i + 1]); i += 2; continue
        if a.startswith("--"):
            i += 1; continue
        x, y = (float(v) for v in a.split(","))
        pts.append((x, y)); i += 1
    r = keep / 2.0

    bd = pcbnew.LoadBoard(path)
    outline = pcbnew.SHAPE_POLY_SET()
    try:
        bd.GetBoardPolygonOutlines(outline, False)
    except TypeError:
        bd.GetBoardPolygonOutlines(outline)
    assert outline.OutlineCount() > 0

    blockers = []
    for f in bd.GetFootprints():
        b = fp_box(f)
        if b:
            blockers.append((b, f.GetReference()))
    for t in bd.GetTracks():
        bb = t.GetBoundingBox()
        blockers.append(((bb.GetLeft() * TOMM, bb.GetTop() * TOMM,
                          bb.GetRight() * TOMM, bb.GetBottom() * TOMM),
                         ("VIA " if isinstance(t, pcbnew.PCB_VIA) else "trk ") + t.GetNetname()))

    def edge_dist(cx, cy):
        best = 1e9
        for oi in range(outline.OutlineCount()):
            ol = outline.Outline(oi)
            n = ol.PointCount()
            for k in range(n):
                a, b = ol.CPoint(k), ol.CPoint((k + 1) % n)
                ax, ay, bx, by = a.x * TOMM, a.y * TOMM, b.x * TOMM, b.y * TOMM
                dx, dy = bx - ax, by - ay
                L2 = dx * dx + dy * dy
                t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((cx - ax) * dx + (cy - ay) * dy) / L2))
                best = min(best, math.hypot(cx - (ax + t * dx), cy - (ay + t * dy)))
        return best

    ok = True
    print("drill %.2f mm, required clear circle %.1f mm" % (drill, keep))
    for (x, y) in pts:
        inside = outline.Contains(pcbnew.VECTOR2I(int(x / TONM * 1e6 * TONM / TONM), 0)) \
            if False else outline.Contains(pcbnew.VECTOR2I(int(x / TOMM), int(y / TOMM)))
        ed = edge_dist(x, y)
        worst, who = 1e9, ""
        for (x0, y0, x1, y1), name in blockers:
            nx = min(max(x, x0), x1)
            ny = min(max(y, y0), y1)
            d = math.hypot(x - nx, y - ny)
            if d < worst:
                worst, who = d, name
        good = inside and ed >= r and worst >= r
        ok = ok and good
        print("  (%7.2f,%7.2f) on-board=%-5s edge %5.2f mm  nearest object %5.2f mm (%s)  %s"
              % (x, y, inside, ed, worst, who[:26], "OK" if good else "*** FAIL ***"))

    if not ok:
        print("\nABORT: at least one position fails. Nothing written.")
        return 1
    if not apply_it:
        print("\nDRY RUN - nothing written.")
        return 0

    lib = pcbnew.FootprintLoad(
        r"C:\Users\Jason\AppData\Local\Programs\KiCad\10.0\share\kicad\footprints\MountingHole.pretty",
        "MountingHole_3.2mm_M3")
    assert lib is not None, "could not load MountingHole_3.2mm_M3"

    for n, (x, y) in enumerate(pts, 1):
        fp = pcbnew.FootprintLoad(
            r"C:\Users\Jason\AppData\Local\Programs\KiCad\10.0\share\kicad\footprints\MountingHole.pretty",
            "MountingHole_3.2mm_M3")
        bd.Add(fp)
        fp.SetPosition(pcbnew.VECTOR2I(int(x / TOMM), int(y / TOMM)))
        fp.SetReference("H%d" % n)
        fp.SetValue("M3 standoff")
        fp.Reference().SetVisible(False)
        fp.Value().SetVisible(False)
    bd.Save(path)
    print("\nWROTE %s  (%d holes)" % (path, len(pts)))

    chk = pcbnew.LoadBoard(path)
    for f in chk.GetFootprints():
        if f.GetReference().startswith("H"):
            p = f.GetPosition()
            dr = [round(pd.GetDrillSizeX() * TOMM, 2) for pd in f.Pads()]
            print("  re-read %-3s at (%7.2f,%7.2f) drill %s" % (
                f.GetReference(), p.x * TOMM, p.y * TOMM, dr))
    return 0


if __name__ == "__main__":
    sys.exit(main())
