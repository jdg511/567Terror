"""Mechanical readiness check: mounting holes, NPTH, and internal Edge.Cuts cutouts.

ENCLOSURE_FIT.md's "Required changes before ordering" list calls for two 27 x 14.5 mm
cutouts and four M3 standoff holes on the CONTROL board. This reports whether they are
actually in the board files. Read-only.
"""
import pcbnew

BOARDS = [
    ("MAIN", r"C:\Users\Jason\source\repos\Glitchwave\hardware\kicad\glitchwave567\glitchwave567.kicad_pcb"),
    ("CTRL", r"C:\Users\Jason\source\repos\Glitchwave\hardware\kicad\glitchwave567_ctrl\glitchwave567_ctrl.kicad_pcb"),
]

for name, path in BOARDS:
    bd = pcbnew.LoadBoard(path)
    print("=" * 60)
    print(name, path.split("\\")[-1])

    mh = [f for f in bd.GetFootprints()
          if "ount" in f.GetFPIDAsString() or "Hole" in f.GetFPIDAsString()]
    print("  mounting-hole footprints :",
          [(f.GetReference(), f.GetFPIDAsString()) for f in mh] or "NONE")

    npth = []
    for f in bd.GetFootprints():
        for pad in f.Pads():
            if pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                p = pad.GetPosition()
                npth.append((f.GetReference(), round(pad.GetDrillSizeX() * 1e-6, 2),
                             round(p.x * 1e-6, 1), round(p.y * 1e-6, 1)))
    print("  NPTH (non-plated) pads   :", npth or "NONE")

    ec = [d for d in bd.GetDrawings() if d.GetLayer() == pcbnew.Edge_Cuts]
    print("  Edge.Cuts graphic items  :", len(ec))
    # An internal cutout shows up as Edge.Cuts geometry well inside the outline.
    eb = bd.GetBoardEdgesBoundingBox()
    L, T = eb.GetLeft() * 1e-6, eb.GetTop() * 1e-6
    R, B = eb.GetRight() * 1e-6, eb.GetBottom() * 1e-6
    print("  board extents            : x %.1f..%.1f  y %.1f..%.1f" % (L, R, T, B))
    inner = []
    for d in ec:
        b = d.GetBoundingBox()
        x0, y0 = b.GetLeft() * 1e-6, b.GetTop() * 1e-6
        x1, y1 = b.GetRight() * 1e-6, b.GetBottom() * 1e-6
        if x0 > L + 12 and x1 < R - 12 and y0 > T + 12 and y1 < B - 12:
            inner.append("x%.1f..%.1f y%.1f..%.1f" % (x0, x1, y0, y1))
    print("  INTERNAL cutouts         :", inner or "NONE")
