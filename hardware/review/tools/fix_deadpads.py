"""Connect pads that exist on the footprint but have no schematic pin behind them.

Two cases on this project, both found by review/tools/q.py <board> ep:

  MAIN  U19  MP1584EN, SOIC-8-1EP. The exposed thermal pad (pad "9") and four
        unnumbered thermal sub-pads carry NO NET. MPS's own datasheet (Rev 1.0,
        PIN FUNCTIONS table p.4) lists ONE row: "5 | GND / Exposed Pad | ...
        Connect exposed pad to GND plane for optimal thermal performance."
        i.e. GND and the exposed pad are the same node. For a 3 A switcher with
        theta-JA 50 C/W this is the primary heat path, and an unnetted pad also
        means the GND pour never stitches to it.

  CTRL  RV1..RV6  ALPS_RK09K1130. The two "MP" snap-in mounting legs carry no
        net, leaving each pot's metal frame floating next to the audio path.
        All six pots have pin 3 = GND (verified), so the frame belongs there.

Fix: give the orphan pads the SAME PAD NUMBER as the part's existing ground pin
and set their net to match. That is the standard KiCad idiom for an exposed pad
whose datasheet treats it as the ground pin, and it keeps schematic/PCB parity
intact - no symbol edit, no netlist re-import, nothing for a future "update from
schematic" to undo.

Usage: fix_deadpads.py <board.kicad_pcb> <REF>:<padnumber> [...] [--apply]
Dry run by default. Refuses if the target pad number is not already present on
that footprint with a real net, so a typo cannot invent a connection.
"""
import sys
import pcbnew

TOMM = 1e-6


def main():
    path = sys.argv[1]
    apply_it = "--apply" in sys.argv
    targets = {}
    for a in sys.argv[2:]:
        if a.startswith("--"):
            continue
        ref, num = a.split(":")
        targets[ref] = num

    bd = pcbnew.LoadBoard(path)
    plan, ok = [], True
    for f in bd.GetFootprints():
        ref = f.GetReference()
        if ref not in targets:
            continue
        want = targets[ref]
        donors = [p for p in f.Pads() if p.GetNumber() == want and p.GetNetname()]
        if not donors:
            print("ABORT %s: no existing pad '%s' with a net on this footprint" % (ref, want))
            ok = False
            continue
        net = donors[0].GetNet()
        orphans = [p for p in f.Pads()
                   if not p.GetNetname()
                   and p.GetAttribute() != pcbnew.PAD_ATTRIB_NPTH]
        if not orphans:
            print("%-6s nothing to do (no netless pads)" % ref)
            continue
        print("%-6s donor pad '%s' net=%s   -> %d orphan pad(s)" % (
            ref, want, net.GetNetname(), len(orphans)))
        for p in orphans:
            pos = p.GetPosition()
            print("        pad %-8r at (%8.3f,%8.3f) size %.2fx%.2f" % (
                p.GetNumber(), pos.x * TOMM, pos.y * TOMM,
                p.GetSizeX() * TOMM, p.GetSizeY() * TOMM))
            plan.append((p, want, net))

    if not ok:
        return 1
    if not plan:
        print("\nnothing to change")
        return 0
    if not apply_it:
        print("\nDRY RUN - nothing written. Re-run with --apply.")
        return 0

    for p, num, net in plan:
        p.SetNumber(num)
        p.SetNet(net)
    bd.Save(path)
    print("\nWROTE", path)

    # independent re-read
    chk = pcbnew.LoadBoard(path)
    for f in chk.GetFootprints():
        if f.GetReference() in targets:
            dead = [p.GetNumber() or "<blank>" for p in f.Pads()
                    if not p.GetNetname() and p.GetAttribute() != pcbnew.PAD_ATTRIB_NPTH]
            nets = sorted({p.GetNetname() for p in f.Pads()
                           if p.GetNumber() == targets[f.GetReference()]})
            print("re-read %-6s pads on '%s' -> %s ; still netless: %s" % (
                f.GetReference(), targets[f.GetReference()], nets, dead or "none"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
