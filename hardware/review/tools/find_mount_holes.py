"""Find clear spots on a board for M3 standoff mounting holes.

ENCLOSURE_FIT.md requires four M3 standoff holes on the CONTROL board
("Control-board retention must come from standoffs") and they were never added.
This grids the board, marks every cell blocked by a footprint body, a pad, a track,
a via or the board edge, and reports the candidate positions with the most clearance
in each quadrant.

An M3 standoff needs the hole plus room for the standoff base / washer, so the
default asks for a 7.0 mm clear circle around a 3.2 mm hole.

Usage: find_mount_holes.py <board.kicad_pcb> [--clear 7.0] [--edge 6.0]
Read-only.
"""
import math
import sys

import pcbnew

TOMM = 1e-6
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
        if it.GetLayer() in FAB and not isinstance(it, (pcbnew.PCB_TEXT,
                                                       pcbnew.PCB_TEXTBOX)):
            b = it.GetBoundingBox()
            xs += [b.GetLeft() * TOMM, b.GetRight() * TOMM]
            ys += [b.GetTop() * TOMM, b.GetBottom() * TOMM]
    return (min(xs), min(ys), max(xs), max(ys)) if xs else None


def main():
    path = sys.argv[1]
    clear = 7.0
    edge = 6.0
    if "--clear" in sys.argv:
        clear = float(sys.argv[sys.argv.index("--clear") + 1])
    if "--edge" in sys.argv:
        edge = float(sys.argv[sys.argv.index("--edge") + 1])
    r = clear / 2.0

    bd = pcbnew.LoadBoard(path)
    eb = bd.GetBoardEdgesBoundingBox()
    L, T = eb.GetLeft() * TOMM, eb.GetTop() * TOMM
    R, B = eb.GetRight() * TOMM, eb.GetBottom() * TOMM

    blockers = []
    for f in bd.GetFootprints():
        b = fp_box(f)
        if b:
            blockers.append((b, f.GetReference()))
    for t in bd.GetTracks():
        bb = t.GetBoundingBox()
        blockers.append(((bb.GetLeft() * TOMM, bb.GetTop() * TOMM,
                          bb.GetRight() * TOMM, bb.GetBottom() * TOMM),
                         "trk/" + t.GetNetname()))

    # The board is NOT its bounding box - this one has 11 x 11 mm notches cut out of
    # all four corners, so a naive bbox test happily proposes holes in thin air.
    # Test against the real Edge.Cuts polygon instead.
    outline = pcbnew.SHAPE_POLY_SET()
    # KiCad 10 added a second required arg (aInferOutlineIfNecessary).
    try:
        bd.GetBoardPolygonOutlines(outline, False)
    except TypeError:
        bd.GetBoardPolygonOutlines(outline)
    assert outline.OutlineCount() > 0, "could not read the Edge.Cuts outline"

    def on_board(cx, cy, margin):
        """Circle of radius `margin` at (cx,cy) fully inside the real board outline."""
        pt = pcbnew.VECTOR2I(int(cx / TOMM), int(cy / TOMM))
        if not outline.Contains(pt):
            return False
        # walk the outline and reject if any edge comes closer than `margin`
        for oi in range(outline.OutlineCount()):
            ol = outline.Outline(oi)
            n = ol.PointCount()
            for i in range(n):
                a = ol.CPoint(i)
                b = ol.CPoint((i + 1) % n)
                ax, ay = a.x * TOMM, a.y * TOMM
                bx, by = b.x * TOMM, b.y * TOMM
                dx, dy = bx - ax, by - ay
                L2 = dx * dx + dy * dy
                t = 0.0 if L2 == 0 else max(0.0, min(1.0,
                                                     ((cx - ax) * dx + (cy - ay) * dy) / L2))
                if math.hypot(cx - (ax + t * dx), cy - (ay + t * dy)) < margin:
                    return False
        return True

    def free(cx, cy):
        """True if a circle of radius r at (cx,cy) hits nothing, incl. board edge."""
        if not on_board(cx, cy, r + edge):
            return False
        for (x0, y0, x1, y1), _ in blockers:
            # circle-vs-rect: nearest point on rect to circle centre
            nx = min(max(cx, x0), x1)
            ny = min(max(cy, y0), y1)
            if math.hypot(cx - nx, cy - ny) < r:
                return False
        return True

    step = 0.5
    cands = []
    y = T + edge
    while y <= B - edge:
        x = L + edge
        while x <= R - edge:
            if free(x, y):
                cands.append((x, y))
            x += step
        y += step

    print("board %.1f x %.1f   asking for a %.1f mm clear circle, %.1f mm from edge"
          % (R - L, B - T, clear, edge))
    print("clear candidate points: %d" % len(cands))
    if not cands:
        print("NONE - loosen --clear or --edge")
        return

    mx, my = (L + R) / 2.0, (T + B) / 2.0
    quads = {"TL": [], "TR": [], "BL": [], "BR": []}
    for (x, y) in cands:
        k = ("T" if y < my else "B") + ("L" if x < mx else "R")
        quads[k].append((x, y))

    print()
    for k in ("TL", "TR", "BL", "BR"):
        pts = quads[k]
        if not pts:
            print("%s: no clear point" % k)
            continue
        # the point furthest into its own corner, i.e. closest to the board corner
        cx = L if "L" in k else R
        cy = T if "T" in k else B
        pts.sort(key=lambda p: math.hypot(p[0] - cx, p[1] - cy))
        best = pts[0]
        # how much room does it actually have? grow the radius until it fails
        rr = r
        while rr < 12.0:
            saved = rr + 0.25
            r_old = rr
            globals()  # noqa
            rr = saved
            # re-test with bigger radius
            ok = True
            if best[0] - rr < L + 1 or best[0] + rr > R - 1 or \
               best[1] - rr < T + 1 or best[1] + rr > B - 1:
                ok = False
            else:
                for (x0, y0, x1, y1), _ in blockers:
                    nx = min(max(best[0], x0), x1)
                    ny = min(max(best[1], y0), y1)
                    if math.hypot(best[0] - nx, best[1] - ny) < rr:
                        ok = False
                        break
            if not ok:
                rr = r_old
                break
        print("%s: (%7.2f, %7.2f)   %d clear points in quadrant, "
              "largest clear radius here %.2f mm" % (k, best[0], best[1], len(pts), rr))


if __name__ == "__main__":
    main()
