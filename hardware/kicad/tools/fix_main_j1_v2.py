"""Main board: move J1 (input jack) clear of L100 / R60 / R114, and R1 out of J1's way.

This is the SECOND attempt. The first (2026-07-26) moved J1 +3.0 mm and produced six
`shorting_items` because J1 is THROUGH-HOLE: its pads live on *.Cu, i.e. all four
layers, and they landed on existing B.Cu traces of /mcu/PWM_MIXD and /mix_svf/SVF_SUM.
That attempt only ripped copper on the SAME net as the moved pads.

This version rips FOREIGN-net copper too, on every layer, inside the footprint's new
pad keepout - the set of segments predicted by tools/whatif.py, which reproduces the
old failure exactly when run against the baseline board.

Read-all-then-mutate throughout (swig proxies go stale after a batched Remove).

Usage:
  fix_main_j1_v2.py <board.kicad_pcb> [--apply]

Default is a dry run: it prints every deletion and move and writes nothing.
Prints NETS= for the caller to feed to microroute.py afterwards.
"""
import sys
import pcbnew

TOMM = 1e-6
TONM = 1000000.0

# Verified by tools/whatif.py against the baseline board:
#   J1 +3.5 / R1 +3.0 leaves ZERO courtyard conflicts and exactly the ten
#   copper conflicts ripped below.
MOVES = {"J1": (3.5, 0.0), "R1": (3.0, 0.0)}

# Nets belonging to the moved parts: rip entirely, reroute from scratch.
OWN_NETS = ("IN_TIP", "/input_dirt/IN_A")
# Foreign nets that cross the new footprint area: rip only inside the keepout.
FOREIGN_NETS = ("/mix_svf/SVF_SUM", "/mcu/PWM_MIXD")
KEEPOUT_MARGIN = 0.6  # mm around each moved pad


def box_of(item):
    b = item.GetBoundingBox()
    return (b.GetLeft(), b.GetTop(), b.GetRight(), b.GetBottom())


def inflate(b, n):
    return (b[0] - n, b[1] - n, b[2] + n, b[3] + n)


def hit(a, b):
    return not (a[2] < b[0] or a[0] > b[2] or a[3] < b[1] or a[1] > b[3])


def main():
    path = sys.argv[1]
    apply_it = "--apply" in sys.argv
    bd = pcbnew.LoadBoard(path)
    fps = {f.GetReference(): f for f in bd.GetFootprints()}

    margin = int(KEEPOUT_MARGIN * TONM)
    keepout = []
    for ref, (dx, dy) in MOVES.items():
        f = fps[ref]
        dxn, dyn = int(dx * TONM), int(dy * TONM)
        for pad in f.Pads():
            b = box_of(pad)
            keepout.append(inflate((b[0] + dxn, b[1] + dyn, b[2] + dxn, b[3] + dyn), margin))
    print("keepout boxes (mm):")
    for k in keepout:
        print("   x%8.3f..%8.3f y%8.3f..%8.3f" % (k[0] * TOMM, k[2] * TOMM,
                                                  k[1] * TOMM, k[3] * TOMM))

    # ---------- READ PHASE: decide everything before touching the board ----------
    doomed = []
    for t in bd.GetTracks():
        net = t.GetNetname()
        if net in OWN_NETS:
            doomed.append((t, "own-net full rip"))
            continue
        if net in FOREIGN_NETS:
            tb = box_of(t)
            if any(hit(tb, k) for k in keepout):
                doomed.append((t, "foreign in keepout"))

    print()
    print("%d track/via items to remove:" % len(doomed))
    by_net = {}
    for t, why in doomed:
        kind = "VIA" if isinstance(t, pcbnew.PCB_VIA) else bd.GetLayerName(t.GetLayer())
        s, e = t.GetStart(), t.GetEnd()
        print("   %-22s %-7s (%8.3f,%8.3f)->(%8.3f,%8.3f)  %s" % (
            t.GetNetname(), kind, s.x * TOMM, s.y * TOMM, e.x * TOMM, e.y * TOMM, why))
        by_net[t.GetNetname()] = by_net.get(t.GetNetname(), 0) + 1
    print("   per net:", by_net)

    print()
    for ref, (dx, dy) in MOVES.items():
        p = fps[ref].GetPosition()
        print("MOVE %-4s (%8.3f,%8.3f) -> (%8.3f,%8.3f)" % (
            ref, p.x * TOMM, p.y * TOMM, p.x * TOMM + dx, p.y * TOMM + dy))

    nets = sorted(set(list(OWN_NETS) + [n for n in by_net if n in FOREIGN_NETS]))
    print()
    print("NETS=" + "|".join(nets))

    if not apply_it:
        print("\nDRY RUN - nothing written. Re-run with --apply.")
        return 0

    # ---------- MUTATE PHASE ----------
    for t, _ in doomed:
        bd.Remove(t)
    for ref, (dx, dy) in MOVES.items():
        f = fps[ref]
        p = f.GetPosition()
        f.SetPosition(pcbnew.VECTOR2I(p.x + int(dx * TONM), p.y + int(dy * TONM)))
    bd.Save(path)
    print("\nWROTE", path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
