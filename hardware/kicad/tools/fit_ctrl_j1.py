"""Put the CONTROL board's J1 on B.Cu so it actually mates main-board J10.

Background: fab/README_PCBWAY.md says the 2x8 stack header on the control board
"mounts on B.Cu"; it is currently on F.Cu, same side as the pots and stomps, so
the two headers point away from each other and cannot mate. It is also why J1's
body overlaps SW2 by 63 mm2 - on the correct side that collision does not exist.

Simply flipping the stock footprint mirrors the pad GRID onto itself but permutes
the PIN NUMBERS (1<->15, 2<->16, 3<->13, ...), silently reversing the pinout.
Instead we use a pre-mirrored footprint that cancels the flip.

This script does not guess which way KiCad mirrors. It builds every candidate
(stock vs mirrored footprint) x (each flip direction), computes the resulting pad
positions IN MEMORY, and scores each against main-board J10's real pad positions
per pin number. Only an exact 16/16 match is accepted.

Usage:
  fit_ctrl_j1.py <ctrl.kicad_pcb> <main.kicad_pcb> <lib.pretty> [--apply]
"""
import sys
import pcbnew

TOMM = 1e-6
STOCK_LIB = (r"C:\Users\Jason\AppData\Local\Programs\KiCad\10.0\share\kicad"
             r"\footprints\Connector_PinHeader_2.54mm.pretty")
STOCK = "PinHeader_2x08_P2.54mm_Vertical"
MIRRORED = "PinHeader_2x08_P2.54mm_Vertical_Mirrored"


def pad_map(fp):
    return {p.GetNumber(): (round(p.GetPosition().x * TOMM, 4),
                            round(p.GetPosition().y * TOMM, 4))
            for p in fp.Pads()}


def flip_dirs():
    """Return [(label, arg)] for whatever the running pcbnew exposes."""
    out = []
    for nm in ("FLIP_DIRECTION_LEFT_RIGHT", "FLIP_DIRECTION_TOP_BOTTOM"):
        if hasattr(pcbnew, nm):
            out.append((nm, getattr(pcbnew, nm)))
    if not out:
        out.append(("legacy-bool-True", True))
        out.append(("legacy-bool-False", False))
    return out


def main():
    ctrl_path, main_path, lib = sys.argv[1], sys.argv[2], sys.argv[3]
    apply_it = "--apply" in sys.argv

    main_bd = pcbnew.LoadBoard(main_path)
    j10 = [f for f in main_bd.GetFootprints() if f.GetReference() == "J10"][0]
    target = pad_map(j10)
    j10_nets = {p.GetNumber(): p.GetNetname() for p in j10.Pads()}
    print("main J10 at (%.3f, %.3f) rot %.0f, side %s" % (
        j10.GetPosition().x * TOMM, j10.GetPosition().y * TOMM,
        j10.GetOrientationDegrees(), "BACK" if j10.IsFlipped() else "FRONT"))

    ctrl = pcbnew.LoadBoard(ctrl_path)
    old = [f for f in ctrl.GetFootprints() if f.GetReference() == "J1"][0]
    pos, rot = old.GetPosition(), old.GetOrientationDegrees()
    old_nets = {p.GetNumber(): p.GetNetCode() for p in old.Pads()}
    old_netnames = {p.GetNumber(): p.GetNetname() for p in old.Pads()}
    print("ctrl J1  at (%.3f, %.3f) rot %.0f, side %s, lib %s" % (
        pos.x * TOMM, pos.y * TOMM, rot,
        "BACK" if old.IsFlipped() else "FRONT", old.GetFPIDAsString()))

    # sanity: the schematic pin->net map must already agree between the boards
    bad = [n for n in target
           if j10_nets[n].split("/")[-1] != old_netnames[n].split("/")[-1]]
    print("pin->net agreement ctrl vs main: %d/16 match%s" % (
        16 - len(bad), "" if not bad else "  MISMATCHED: %s" % bad))

    print()
    print("candidates (pads matching J10 by pin number, out of 16):")

    def try_candidate(fpname, libdir, flipped, dval):
        """Build the candidate ON A THROWAWAY BOARD and return its pad map.

        Flip() on a footprint with no parent board hangs - it needs the board's
        layer mapping. So each candidate gets a freshly loaded copy of the ctrl
        board, which is never saved.
        """
        scratch = pcbnew.LoadBoard(ctrl_path)
        victim = [f for f in scratch.GetFootprints() if f.GetReference() == "J1"][0]
        vpos = victim.GetPosition()
        scratch.Remove(victim)
        cand = pcbnew.FootprintLoad(libdir, fpname)
        if cand is None:
            return None
        scratch.Add(cand)
        cand.SetPosition(vpos)
        cand.SetOrientationDegrees(rot)
        if flipped:
            cand.Flip(vpos, dval)
        return pad_map(cand)

    # ONE candidate per process. pcbnew's swig proxies go stale process-wide after
    # a Remove/Add, so the second LoadBoard in the same process returns an object
    # whose methods are missing. The caller loops over --probe values.
    probe = None
    if "--probe" in sys.argv:
        i = sys.argv.index("--probe")
        probe = (sys.argv[i + 1], sys.argv[i + 2])   # (STOCK|MIRRORED, none|LR|TB)

    def combos_for(fpname):
        all_c = [("no-flip", None, False)] + [(d, v, True) for d, v in flip_dirs()]
        if probe is None:
            return all_c
        want_fp, want_d = probe
        if (want_fp == "STOCK") != (fpname == STOCK):
            return []
        key = {"none": "no-flip", "LR": "FLIP_DIRECTION_LEFT_RIGHT",
               "TB": "FLIP_DIRECTION_TOP_BOTTOM"}[want_d]
        return [c for c in all_c if c[0] == key]

    best = None
    for fpname, libdir in ((STOCK, STOCK_LIB), (MIRRORED, lib)):
        for dlabel, dval, flipped in combos_for(fpname):
            try:
                got = try_candidate(fpname, libdir, flipped, dval)
            except Exception as ex:
                print("   %-44s %-24s ERROR %s" % (fpname, dlabel, ex))
                continue
            if got is None:
                print("   %-44s %-24s LOAD FAILED" % (fpname, dlabel))
                continue
            score = sum(1 for n, xy in target.items() if got.get(n) == xy)
            side = "B.Cu" if flipped else "F.Cu"
            print("   %-44s %-22s %-5s %2d/16%s" % (
                fpname, dlabel, side, score, "   <== EXACT" if score == 16 else ""))
            if score == 16 and flipped and best is None:
                best = (fpname, libdir, flipped, dlabel, dval)

    if best is None:
        print("\nNo candidate reproduces J10's pad grid exactly. Nothing written.")
        return 1
    fpname, libdir, flipped, dlabel, dval = best
    print("\nCHOSEN: %s, flip=%s (%s)" % (fpname, flipped, dlabel))

    if not apply_it:
        print("DRY RUN - nothing written.")
        return 0

    new = pcbnew.FootprintLoad(libdir, fpname)
    ctrl.Remove(old)
    ctrl.Add(new)                 # parent it BEFORE flipping - see try_candidate()
    new.SetPosition(pos)
    new.SetOrientationDegrees(rot)
    if flipped:
        new.Flip(pos, dval)
    new.SetReference("J1")
    new.SetValue("to MAIN 2x8 (B.Cu, mirrored)")
    for p in new.Pads():
        p.SetNetCode(old_nets[p.GetNumber()])
    ctrl.Save(ctrl_path)
    print("WROTE", ctrl_path)

    # independent re-read: never trust the in-memory object we just built
    chk = pcbnew.LoadBoard(ctrl_path)
    got = [f for f in chk.GetFootprints() if f.GetReference() == "J1"][0]
    gm, gn = pad_map(got), {p.GetNumber(): p.GetNetname() for p in got.Pads()}
    okpos = sum(1 for n, xy in target.items() if gm.get(n) == xy)
    oknet = sum(1 for n in old_netnames if gn.get(n) == old_netnames[n])
    print("re-read: side=%s  pads matching J10 %d/16  nets preserved %d/16" % (
        "BACK" if got.IsFlipped() else "FRONT", okpos, oknet))
    return 0 if (okpos == 16 and oknet == 16 and got.IsFlipped()) else 1


if __name__ == "__main__":
    sys.exit(main())
