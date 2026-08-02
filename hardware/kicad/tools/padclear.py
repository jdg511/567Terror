"""True-geometry clearance probe: named footprints' pads vs FOREIGN-net copper.

Why this exists: whatif.py screens with bounding boxes, so once a router lays
45-degree track it over-reports badly (a diagonal segment's bbox is far larger
than the segment). This measures actual shape-to-shape distance, the same thing
DRC uses, so it can confirm or dismiss a whatif hit.

Two KiCad 10 API traps this file works around, both of which made an earlier
version silently report "none" for everything:
  * PAD::GetEffectiveShape() lost its zero-argument overload - a layer is required.
  * SHAPE::Collide(other, clearance) returns a PLAIN BOOL in the swig binding,
    not the (ok, actual) tuple the C++ signature suggests. Unpacking it raises
    TypeError, which a bare `except` will hide. So we bracket the distance by
    stepping the clearance instead.

Usage: padclear.py <board> REF [REF ...] [--limit 0.6] [--step 0.02]
Read-only.
"""
import sys
import pcbnew

TOMM = 1e-6
TONM = 1000000.0


def main():
    path = sys.argv[1]
    limit, step = 0.6, 0.02
    argv = sys.argv[2:]
    skip = set()
    if "--limit" in argv:
        i = argv.index("--limit")
        limit = float(argv[i + 1])
        skip.update((i, i + 1))
    if "--step" in argv:
        i = argv.index("--step")
        step = float(argv[i + 1])
        skip.update((i, i + 1))
    refs = [a for i, a in enumerate(argv) if i not in skip and not a.startswith("--")]

    bd = pcbnew.LoadBoard(path)
    enabled = set(bd.GetEnabledLayers().CuStack())
    tracks = list(bd.GetTracks())
    rows = []
    n_pairs = 0

    for f in bd.GetFootprints():
        if f.GetReference() not in refs:
            continue
        for pad in f.Pads():
            pnet = pad.GetNetCode()
            players = [l for l in pad.GetLayerSet().CuStack() if l in enabled]
            for t in tracks:
                if t.GetNetCode() == pnet:
                    continue
                if isinstance(t, pcbnew.PCB_VIA):
                    tl = [l for l in t.GetLayerSet().CuStack() if l in enabled]
                else:
                    tl = [t.GetLayer()]
                shared = [l for l in players if l in tl]
                if not shared:
                    continue
                n_pairs += 1
                for lay in shared:
                    ps = pad.GetEffectiveShape(lay)
                    ts = t.GetEffectiveShape(lay)
                    if not ps.Collide(ts, int(limit * TONM)):
                        continue
                    lo = 0.0
                    while lo <= limit and not ps.Collide(ts, int(lo * TONM)):
                        lo += step
                    kind = "VIA" if isinstance(t, pcbnew.PCB_VIA) else "trk"
                    rows.append((round(lo, 3), f.GetReference(), pad.GetNumber(),
                                 pad.GetNetname() or "<none>", kind,
                                 bd.GetLayerName(lay), t.GetNetname() or "<none>"))
                    break

    rows.sort()
    print("board:", path)
    print("pad/track pairs sharing a layer, checked: %d" % n_pairs)
    print("foreign copper within %.2f mm of pads of %s:" % (limit, refs))
    if not rows:
        print("   NONE - nothing foreign is even close")
    for r in rows:
        print("   ~%5.2f mm  %s.%s (%s) vs %s %s on %s" % (
            r[0], r[1], r[2], r[3], r[4], r[6], r[5]))
    print("   count:", len(rows))


if __name__ == "__main__":
    main()
