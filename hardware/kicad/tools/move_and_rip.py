"""Move footprints, ripping exactly the copper that gets in the way. Generalised
from fix_main_j1_v2.py.

For each moved footprint it:
  * full-rips every track/via on that footprint's own nets, so they can be
    rerouted to the new pad positions - EXCEPT GND (and any --keep net), whose
    connection comes from the zone pour and must never be ripped board-wide;
  * rips foreign-net copper only where it falls inside the new pad keepout, on
    EVERY layer the pad occupies. For a through-hole pad that means all four
    layers - missing this is what produced six shorts on 2026-07-26.

Read-all-then-mutate: pcbnew's swig proxies go stale process-wide after a
batched Remove, so every decision is made before anything is touched.

Usage:
  move_and_rip.py <board> REF:dx,dy [REF:dx,dy ...] [--margin 0.6] [--keep NET] [--apply]

Prints NETS= for the caller to feed to microroute.py. Dry run by default.
"""
import sys
import pcbnew

TOMM = 1e-6
TONM = 1000000.0


def box(item):
    b = item.GetBoundingBox()
    return (b.GetLeft(), b.GetTop(), b.GetRight(), b.GetBottom())


def hit(a, b):
    return not (a[2] < b[0] or a[0] > b[2] or a[3] < b[1] or a[1] > b[3])


def main():
    path = sys.argv[1]
    apply_it = "--apply" in sys.argv
    margin = 0.6
    keep = {"GND"}
    args = sys.argv[2:]
    i = 0
    moves = {}
    while i < len(args):
        a = args[i]
        if a == "--margin":
            margin = float(args[i + 1]); i += 2; continue
        if a == "--keep":
            keep.add(args[i + 1]); i += 2; continue
        if a in ("--apply", "--own-scoped"):
            i += 1; continue
        ref, rest = a.split(":")
        dx, dy = (float(v) for v in rest.split(","))
        moves[ref] = (int(dx * TONM), int(dy * TONM))
        i += 1

    bd = pcbnew.LoadBoard(path)
    fps = {f.GetReference(): f for f in bd.GetFootprints()}
    for r in moves:
        if r not in fps:
            print("no such footprint:", r)
            return 1

    # Keepout covers BOTH the old and the new pad footprint. The old boxes matter:
    # that is where the existing stubs terminate, and they have to go or the net
    # keeps a dead tail pointing at empty board.
    # Each keepout box carries the LAYER SET of the pad that produced it. Without
    # that, moving an SMD part would rip inner- and bottom-layer traces that pass
    # harmlessly underneath it - on this board that was the difference between 31
    # ripped items and 9.
    enabled = set(bd.GetEnabledLayers().CuStack())
    m_nm = int(margin * TONM)
    keepout, own_nets = [], set()
    for ref, (dx, dy) in moves.items():
        for pad in fps[ref].Pads():
            b = box(pad)
            lays = frozenset(l for l in pad.GetLayerSet().CuStack() if l in enabled)
            keepout.append(((b[0] - m_nm, b[1] - m_nm, b[2] + m_nm, b[3] + m_nm), lays))
            keepout.append(((b[0] + dx - m_nm, b[1] + dy - m_nm,
                             b[2] + dx + m_nm, b[3] + dy + m_nm), lays))
            n = pad.GetNetname()
            if n and n not in keep:
                own_nets.add(n)

    def layers_of(t):
        if isinstance(t, pcbnew.PCB_VIA):
            return set(l for l in t.GetLayerSet().CuStack() if l in enabled)
        return {t.GetLayer()}

    # Full-ripping an own net is right when the part barely has any routing (the
    # J1 jack), and wrong when the net spans the board: microroute would have to
    # rebuild 26 segments from scratch and would do a worse job than what is
    # already there. --own-scoped rips own nets only where they touch the keepout,
    # leaving the far end of the net untouched and giving the router a short
    # two-cluster bridge to close.
    own_scoped = "--own-scoped" in sys.argv
    print("moves:", {k: (v[0] * TOMM, v[1] * TOMM) for k, v in moves.items()})
    print("own nets (%s):" % ("keepout only" if own_scoped else "FULL RIP"),
          sorted(own_nets))
    print("never ripped:", sorted(keep))

    doomed = []
    for t in bd.GetTracks():
        n = t.GetNetname()
        tl = layers_of(t)
        inbox = any(hit(box(t), k) and (tl & lays) for k, lays in keepout)
        if n in own_nets:
            if not own_scoped or inbox:
                doomed.append((t, "own" if not own_scoped else "own-in-keepout"))
        elif n not in keep and inbox:
            doomed.append((t, "foreign-in-keepout"))

    per = {}
    for t, why in doomed:
        per.setdefault((t.GetNetname(), why), 0)
        per[(t.GetNetname(), why)] += 1
    print()
    print("%d items to remove:" % len(doomed))
    for (n, why), c in sorted(per.items()):
        print("   %-26s %-20s %d" % (n, why, c))

    nets = sorted(own_nets | {t.GetNetname() for t, w in doomed
                              if w == "foreign-in-keepout"})
    print()
    print("NETS=" + "|".join(nets))
    if not apply_it:
        print("\nDRY RUN - nothing written.")
        return 0

    for t, _ in doomed:
        bd.Remove(t)
    for ref, (dx, dy) in moves.items():
        p = fps[ref].GetPosition()
        fps[ref].SetPosition(pcbnew.VECTOR2I(p.x + dx, p.y + dy))
    bd.Save(path)
    print("\nWROTE", path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
