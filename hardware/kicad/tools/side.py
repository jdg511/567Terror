"""Report which physical side a set of footprints sits on, plus pad net order.

Usage: side.py <board> REF [REF ...]
Read-only.
"""
import sys
import pcbnew

TOMM = 1e-6

bd = pcbnew.LoadBoard(sys.argv[1])
want = set(sys.argv[2:])
for f in bd.GetFootprints():
    if f.GetReference() not in want:
        continue
    p = f.GetPosition()
    print("%s  %s" % (f.GetReference(), f.GetValue()))
    print("   lib id   : %s" % f.GetFPIDAsString())
    print("   side     : %s   (layer %s)" % (
        "BACK/bottom" if f.IsFlipped() else "FRONT/top", bd.GetLayerName(f.GetLayer())))
    print("   at       : (%.3f, %.3f) rot %.0f" % (p.x * TOMM, p.y * TOMM,
                                                   f.GetOrientationDegrees()))
    pads = sorted(f.Pads(), key=lambda q: (q.GetNumber().zfill(4)))
    for q in pads:
        qp = q.GetPosition()
        print("   pad %-4s (%8.3f,%8.3f)  %s" % (
            q.GetNumber(), qp.x * TOMM, qp.y * TOMM, q.GetNetname()))
    print()
