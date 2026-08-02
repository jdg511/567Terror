"""Query the extracted netlist. Read-only.

  q.py <board> net <NAME|regex>     list every node on matching nets
  q.py <board> ref <REF> [REF...]   full pin map for components
  q.py <board> find <regex>         components whose ref/value/libsource match
  q.py <board> nets                 every net with node count
  q.py <board> ep                   footprints whose pad count > symbol pin count
                                     (catches unconnected exposed/thermal pads)
<board> is 'main' or 'ctrl'.
"""
import json
import os
import re
import sys

HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "extract")


def load(board, kind):
    return json.load(open(os.path.join(HERE, "%s.%s.json" % (board, kind))))


def main():
    board, cmd = sys.argv[1], sys.argv[2]
    if cmd == "net":
        nets = load(board, "nets")
        pat = re.compile(sys.argv[3], re.I)
        for n in sorted(nets):
            if pat.search(n):
                print("%s   (%d nodes)" % (n, len(nets[n])))
                for m in nets[n]:
                    print("    %-7s pin %-4s %-16s %s" % (
                        m["ref"], m["pin"], (m["pinname"] or "")[:16], m["pintype"]))
                print()
    elif cmd == "ref":
        pm, cp = load(board, "pinmap"), load(board, "components")
        for ref in sys.argv[3:]:
            c = cp.get(ref)
            if not c:
                print("no such ref:", ref)
                continue
            print("=" * 68)
            print(ref, "|", c["value"], "|", c["libsource"])
            print("footprint:", c["footprint"])
            print("sheet:", c["sheet"], " datasheet:", c["datasheet"] or "(none)")
            for p in sorted(pm.get(ref, {}), key=lambda x: (len(x), x)):
                d = pm[ref][p]
                print("   %-4s %-16s %-24s -> %s" % (
                    p, (d["name"] or "")[:16], d["type"], d["net"]))
    elif cmd == "find":
        cp = load(board, "components")
        pat = re.compile(sys.argv[3], re.I)
        for r in sorted(cp):
            c = cp[r]
            blob = " ".join([r, c["value"], c["libsource"], c["footprint"]])
            if pat.search(blob):
                print("%-8s %-30s %-32s %s" % (r, c["value"][:30],
                                               c["libsource"][:32], c["footprint"]))
    elif cmd == "nets":
        nets = load(board, "nets")
        for n in sorted(nets, key=lambda k: -len(nets[k])):
            print("%4d  %s" % (len(nets[n]), n))
    elif cmd == "ep":
        # symbol pin count vs footprint pad count, using the PCB for pad counts
        import pcbnew
        # NOTE: pcbnew.LoadBoard() on a path that does not exist returns a NEW EMPTY
        # board instead of raising. An earlier version of this built the path wrong
        # and silently reported "no unconnected pads" for a board it never opened.
        # Resolve absolutely and assert the file is there.
        hw = os.path.abspath(os.path.join(os.path.dirname(HERE), ".."))
        pcb = {"main": os.path.join(hw, "kicad", "glitchwave567",
                                    "glitchwave567.kicad_pcb"),
               "ctrl": os.path.join(hw, "kicad", "glitchwave567_ctrl",
                                    "glitchwave567_ctrl.kicad_pcb")}[board]
        assert os.path.isfile(pcb), "board not found: " + pcb
        bd = pcbnew.LoadBoard(pcb)
        assert len(list(bd.GetFootprints())) > 0, "board loaded empty: " + pcb
        pm = load(board, "pinmap")
        print("%-8s %-38s %5s %5s  unconnected pads" % (
            "REF", "FOOTPRINT", "sym", "pads"))
        for f in sorted(bd.GetFootprints(), key=lambda x: x.GetReference()):
            ref = f.GetReference()
            pads = [p for p in f.Pads()
                    if p.GetAttribute() != pcbnew.PAD_ATTRIB_NPTH]
            dead = [p.GetNumber() or "<blank>" for p in pads if not p.GetNetname()]
            nsym = len(pm.get(ref, {}))
            if dead:
                print("%-8s %-38s %5d %5d  %s" % (
                    ref, f.GetFPIDAsString()[:38], nsym, len(pads), ", ".join(dead)))


if __name__ == "__main__":
    main()
