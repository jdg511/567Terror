"""Fix the J1 <-> L100 / R60 / R114 courtyard collision on glitchwave567 (main).

WHY
  J1 (PJ-603A 6.35mm input jack, rot -90) has an on-board body spanning
  board x 103.00..117.00, y 1.60..26.60.  That runs into:
      L100  (22uH/3A buck inductor, 12x12x8mm)  9.80 mm^2   <-- the blocker
      R60   (1M)                                2.22 mm^2
      R114  (24k9 EN divider)                   0.92 mm^2
  L100 cannot move: a whole-board free-space scan found only two slots big
  enough (14.31 x 13.79mm), nearest 32.5mm away, and it is the BUCK_SW
  switching node -- relocating it means re-laying a switching loop.

FIX (validated with check_courtyard_overlaps virtual placement before writing)
      J1  (110.0, 1.6) -> (113.0, 1.6)    +3.0mm  clears L100, R60 AND R114
      R1  (119.4, 21.8) -> (122.5, 21.8)  +3.1mm  gets out of J1's new body
  Result: every J1 overlap gone. Overlap count 19 -> 16, remainder is the
  pre-existing benign set (decoupling caps hugging their ICs).

CONSEQUENCE
  J1's x sets the enclosure BACK-WALL DRILL POSITION -- the drill template
  moves +3mm too.  Update hardware/fab drill template / ENCLOSURE_FIT.

SWIG SAFETY
  Read-all-then-mutate, exactly like mover.py: every pcbnew object is read
  into plain Python data first, then all mutations happen in one pass, then
  one SaveBoard.  The KiCAD-MCP server's one-mutation-per-process staleness
  bug ('SwigPyObject' object is not iterable) comes from interleaving reads
  and Remove() calls -- this script does not do that.

USAGE
  <kicad>/bin/python.exe fix_j1_l100.py <path-to-glitchwave567.kicad_pcb> [--dry-run]

  Then reroute the nets it prints, e.g.
      python.exe microroute.py <pcb> IN_TIP
      python.exe microroute.py <pcb> /input_dirt/IN_A
  then refill zones and re-run DRC.
"""
import sys
import math
import pcbnew


def mm(v):
    return pcbnew.FromMM(v)


def to_mm(v):
    return pcbnew.ToMM(v)


# ref -> (new_x_mm, new_y_mm).  Rotation is left untouched.
TARGETS = {
    'J1': (113.0, 1.6),
    'R1': (122.5, 21.8),
}

RIP_MARGIN = 0.3   # mm beyond pad half-extent to consider a track "attached"


def main(pcb_path, dry_run=False):
    board = pcbnew.LoadBoard(pcb_path)

    # ---------------- read phase (no mutation below this line) -------------
    fps = {fp.GetReference(): fp for fp in board.GetFootprints()}
    missing = [r for r in TARGETS if r not in fps]
    if missing:
        print('ERROR: refs not on board: %s' % ', '.join(missing))
        return 2

    netnames = {v.GetNetCode(): str(k) for k, v in board.GetNetsByName().items()}

    # pads of the parts we are moving: (ref, x, y, netcode, halfw, halfh)
    moving_pads = []
    for ref in TARGETS:
        for p in fps[ref].Pads():
            bb = p.GetBoundingBox()
            moving_pads.append((
                ref,
                to_mm(bb.GetCenter().x), to_mm(bb.GetCenter().y),
                p.GetNetCode(),
                to_mm(bb.GetWidth()) / 2.0, to_mm(bb.GetHeight()) / 2.0,
            ))

    # every track/via as plain data + a uuid->object map for the mutate phase
    tracks = []
    uuid_to_obj = {}
    for t in board.GetTracks():
        uid = t.m_Uuid.AsString()
        uuid_to_obj[uid] = t
        if t.GetClass() == 'PCB_VIA':
            pos = t.GetPosition()
            tracks.append((uid, t.GetNetCode(), True,
                           to_mm(pos.x), to_mm(pos.y), to_mm(pos.x), to_mm(pos.y)))
        else:
            s, e = t.GetStart(), t.GetEnd()
            tracks.append((uid, t.GetNetCode(), False,
                           to_mm(s.x), to_mm(s.y), to_mm(e.x), to_mm(e.y)))

    # current positions, for the report
    before = {}
    for ref in TARGETS:
        pos = fps[ref].GetPosition()
        before[ref] = (to_mm(pos.x), to_mm(pos.y), fps[ref].GetOrientationDegrees())

    # ---- decide which tracks to rip: those landing on a moved pad ----
    # GND is skipped -- those pads reconnect through the F.Cu/In1.Cu/B.Cu pours
    # on the next zone refill, so their traces (if any) are not the mechanism.
    to_remove = set()
    affected_nets = set()
    for (ref, px, py, nc, phw, phh) in moving_pads:
        name = netnames.get(nc, '')
        if not nc or name == 'GND':
            continue
        affected_nets.add(name)
        reach = max(phw, phh) + RIP_MARGIN
        for (uid, tnc, is_via, x0, y0, x1, y1) in tracks:
            if tnc != nc:
                continue
            if (math.hypot(x0 - px, y0 - py) < reach or
                    math.hypot(x1 - px, y1 - py) < reach):
                to_remove.add(uid)

    # ---------------- report ----------------
    print('board : %s' % pcb_path)
    print('')
    print('moves:')
    for ref, (nx, ny) in TARGETS.items():
        ox, oy, rot = before[ref]
        print('  %-4s (%8.3f, %7.3f) rot %-7.1f -> (%8.3f, %7.3f)   d=(%+.2f, %+.2f)'
              % (ref, ox, oy, rot, nx, ny, nx - ox, ny - oy))
    print('')
    print('tracks to rip: %d' % len(to_remove))
    for uid in sorted(to_remove):
        for (u, tnc, is_via, x0, y0, x1, y1) in tracks:
            if u == uid:
                print('  %-10s %-22s (%7.3f,%7.3f) -> (%7.3f,%7.3f)'
                      % ('via' if is_via else 'seg', netnames.get(tnc, '?'),
                         x0, y0, x1, y1))
                break

    if dry_run:
        print('')
        print('DRY RUN -- nothing written.')
        print('NETS=' + ','.join(sorted(n for n in affected_nets if n)))
        return 0

    # ---------------- mutate phase (single pass, then one save) ------------
    for uid in to_remove:
        board.Remove(uuid_to_obj[uid])

    for ref, (nx, ny) in TARGETS.items():
        fps[ref].SetPosition(pcbnew.VECTOR2I(mm(nx), mm(ny)))

    pcbnew.SaveBoard(pcb_path, board)

    print('')
    print('WROTE %s' % pcb_path)
    print('moved %d parts, removed %d track items'
          % (len(TARGETS), len(to_remove)))
    print('')
    print('NEXT: reroute these nets, then refill zones, then DRC')
    print('NETS=' + ','.join(sorted(n for n in affected_nets if n)))
    return 0


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    sys.exit(main(sys.argv[1], '--dry-run' in sys.argv))
