"""Read-only what-if placement checker.

Answers the question that killed the 2026-07-26 J1 attempt:
  "if I move this footprint here, do its pads land on foreign copper on ANY layer?"

Usage:
  whatif.py <board> --move REF:dx,dy [--move REF:dx,dy ...] [--clear 0.25]
  whatif.py <board> --scan REF --dx a:b:step --dy a:b:step [--also REF:dx,dy]

Checks, for the moved set:
  1. courtyard vs courtyard against every other footprint (true F.CrtYd polys;
     falls back to pad bbox for footprints that have none)
  2. every moved pad vs every TRACK and VIA of a DIFFERENT net, on every layer
     the pad occupies (through-hole pads occupy all copper layers)
  3. every moved pad vs every FOREIGN pad of every other footprint
  4. pads vs board edge

Zone/pour copper is deliberately ignored: pours re-flow on refill.
Nothing is written. Ever.
"""
import sys
import pcbnew

TOMM = 1e-6
TONM = 1000000.0


def poly_bbox(poly):
    if poly is None or poly.OutlineCount() == 0:
        return None
    b = poly.BBox()
    return (b.GetLeft(), b.GetTop(), b.GetRight(), b.GetBottom())


FAB_LAYERS = (pcbnew.F_Fab, pcbnew.B_Fab)


def fp_box(f):
    """Best available physical-extent box in nm.

    Priority: real F.CrtYd/B.CrtYd polygon -> union of pads and F.Fab/B.Fab
    GRAPHICS (never fp_text: a long value string like R114's inflates the bbox
    to 28 mm and invents phantom obstacles) -> pads alone.
    """
    cy = poly_bbox(f.GetCourtyard(pcbnew.F_CrtYd)) or poly_bbox(f.GetCourtyard(pcbnew.B_CrtYd))
    if cy:
        return cy, "crtyd"
    xs, ys = [], []
    for p in f.Pads():
        b = p.GetBoundingBox()
        xs += [b.GetLeft(), b.GetRight()]
        ys += [b.GetTop(), b.GetBottom()]
    nfab = 0
    for it in f.GraphicalItems():
        if it.GetLayer() not in FAB_LAYERS:
            continue
        if isinstance(it, (pcbnew.PCB_TEXT, pcbnew.PCB_TEXTBOX)):
            continue
        b = it.GetBoundingBox()
        xs += [b.GetLeft(), b.GetRight()]
        ys += [b.GetTop(), b.GetBottom()]
        nfab += 1
    if not xs:
        return None, "none"
    return (min(xs), min(ys), max(xs), max(ys)), ("pads+fab" if nfab else "pads")


def shift(box, dx, dy):
    return (box[0] + dx, box[1] + dy, box[2] + dx, box[3] + dy)


def overlap(a, b):
    """Return (w,h) of intersection in nm, or None."""
    if a is None or b is None:
        return None
    x0, y0 = max(a[0], b[0]), max(a[1], b[1])
    x1, y1 = min(a[2], b[2]), min(a[3], b[3])
    if x1 <= x0 or y1 <= y0:
        return None
    return (x1 - x0, y1 - y0)


def seg_box(t):
    b = t.GetBoundingBox()
    return (b.GetLeft(), b.GetTop(), b.GetRight(), b.GetBottom())


def check(bd, moves, clear_nm, verbose=True):
    """moves: {ref: (dx_nm, dy_nm)}. Returns list of conflict strings."""
    fps = {f.GetReference(): f for f in bd.GetFootprints()}
    conflicts = []

    # ---- 1. courtyard vs courtyard -------------------------------------------------
    moved_boxes = {}
    for ref, (dx, dy) in moves.items():
        f = fps[ref]
        box, src = fp_box(f)
        moved_boxes[ref] = shift(box, dx, dy)
    for ref, mbox in moved_boxes.items():
        for other in bd.GetFootprints():
            oref = other.GetReference()
            if oref == ref:
                continue
            obox, _ = fp_box(other)
            if oref in moves:
                obox = moved_boxes[oref]
            ov = overlap(mbox, obox)
            if ov:
                conflicts.append("COURTYARD %s vs %s  %.3f x %.3f mm = %.2f mm2" % (
                    ref, oref, ov[0] * TOMM, ov[1] * TOMM, ov[0] * ov[1] * TOMM * TOMM))

    # ---- 2/3. moved pads vs foreign copper on every layer --------------------------
    tracks = list(bd.GetTracks())
    for ref, (dx, dy) in moves.items():
        f = fps[ref]
        for pad in f.Pads():
            pnet = pad.GetNetCode()
            pname = pad.GetNetname()
            pb = pad.GetBoundingBox()
            pbox = shift((pb.GetLeft(), pb.GetTop(), pb.GetRight(), pb.GetBottom()), dx, dy)
            pbox = (pbox[0] - clear_nm, pbox[1] - clear_nm,
                    pbox[2] + clear_nm, pbox[3] + clear_nm)
            players = set(pad.GetLayerSet().Seq())

            for t in tracks:
                if t.GetNetCode() == pnet:
                    continue
                if isinstance(t, pcbnew.PCB_VIA):
                    tl = set(t.GetLayerSet().Seq())
                else:
                    tl = {t.GetLayer()}
                if not (players & tl):
                    continue
                ov = overlap(pbox, seg_box(t))
                if not ov:
                    continue
                kind = "VIA" if isinstance(t, pcbnew.PCB_VIA) else bd.GetLayerName(t.GetLayer())
                pos = t.GetPosition()
                conflicts.append(
                    "COPPER   %s.%s(%s) vs %s %s at (%.2f,%.2f)" % (
                        ref, pad.GetNumber(), pname or "<none>",
                        kind, t.GetNetname() or "<none>",
                        pos.x * TOMM, pos.y * TOMM))

            for other in bd.GetFootprints():
                if other.GetReference() == ref:
                    continue
                odx, ody = moves.get(other.GetReference(), (0, 0))
                for opad in other.Pads():
                    if opad.GetNetCode() == pnet and pnet != 0:
                        continue
                    if not (players & set(opad.GetLayerSet().Seq())):
                        continue
                    ob = opad.GetBoundingBox()
                    obox = shift((ob.GetLeft(), ob.GetTop(), ob.GetRight(), ob.GetBottom()),
                                 odx, ody)
                    ov = overlap(pbox, obox)
                    if ov:
                        conflicts.append("PAD      %s.%s(%s) vs %s.%s(%s)" % (
                            ref, pad.GetNumber(), pname or "<none>",
                            other.GetReference(), opad.GetNumber(),
                            opad.GetNetname() or "<none>"))

    # ---- 4. board edge --------------------------------------------------------------
    eb = bd.GetBoardEdgesBoundingBox()
    edge = (eb.GetLeft(), eb.GetTop(), eb.GetRight(), eb.GetBottom())
    for ref, (dx, dy) in moves.items():
        f = fps[ref]
        for pad in f.Pads():
            pb = pad.GetBoundingBox()
            pbox = shift((pb.GetLeft(), pb.GetTop(), pb.GetRight(), pb.GetBottom()), dx, dy)
            if (pbox[0] < edge[0] or pbox[2] > edge[2]
                    or pbox[1] < edge[1] or pbox[3] > edge[3]):
                conflicts.append("EDGE     %s.%s outside board outline" % (
                    ref, pad.GetNumber()))
    return conflicts


def parse_move(s):
    ref, rest = s.split(":")
    dx, dy = (float(v) for v in rest.split(","))
    return ref, (int(dx * TONM), int(dy * TONM))


def main():
    path = sys.argv[1]
    args = sys.argv[2:]
    clear = 0.25
    if "--clear" in args:
        clear = float(args[args.index("--clear") + 1])
    clear_nm = int(clear * TONM)
    bd = pcbnew.LoadBoard(path)

    if "--scan" in args:
        ref = args[args.index("--scan") + 1]
        def rng(flag):
            a, b, st = (float(v) for v in args[args.index(flag) + 1].split(":"))
            out, v = [], a
            while v <= b + 1e-9:
                out.append(round(v, 3))
                v += st
            return out
        also = {}
        for i, a in enumerate(args):
            if a == "--also":
                r, d = parse_move(args[i + 1])
                also[r] = d
        print("scanning %s  clearance=%.2f mm  also=%s" % (ref, clear, list(also)))
        clean = []
        for dx in rng("--dx"):
            for dy in rng("--dy"):
                mv = dict(also)
                mv[ref] = (int(dx * TONM), int(dy * TONM))
                c = check(bd, mv, clear_nm)
                tag = "OK  " if not c else "%3d " % len(c)
                print("  dx=%+7.3f dy=%+7.3f  %s%s" % (
                    dx, dy, tag, "" if not c else c[0][:88]))
                if not c:
                    clean.append((dx, dy))
        print()
        print("CLEAN OFFSETS:", clean if clean else "NONE")
        return

    moves = {}
    for i, a in enumerate(args):
        if a == "--move":
            r, d = parse_move(args[i + 1])
            moves[r] = d
    print("what-if:", {k: (v[0] * TOMM, v[1] * TOMM) for k, v in moves.items()},
          " clearance=%.2f mm" % clear)
    c = check(bd, moves, clear_nm)
    if not c:
        print("\n*** NO CONFLICTS ***")
    else:
        print("\n%d conflicts:" % len(c))
        for x in c:
            print("  ", x)


if __name__ == "__main__":
    main()
