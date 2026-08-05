"""Refill copper zones and run DRC on a board, in its own process.

Kept separate from fix_j1_l100.py on purpose: zone filling is the step most
likely to crash the pcbnew SWIG bindings, and if it does, the completed move
is already safely on disk.

USAGE
  <kicad>/bin/python.exe refill_and_drc.py <pcb> [--no-fill]
"""
import sys
import pcbnew


def main(pcb_path, do_fill=True):
    board = pcbnew.LoadBoard(pcb_path)

    if do_fill:
        zones = board.Zones()
        print('refilling %d zones ...' % len(zones))
        filler = pcbnew.ZONE_FILLER(board)
        filler.Fill(zones)
        pcbnew.SaveBoard(pcb_path, board)
        print('zones refilled and saved')
        # reload so DRC sees the filled state
        board = pcbnew.LoadBoard(pcb_path)

    rpt = pcb_path + '.drc.rpt'
    ok = pcbnew.WriteDRCReport(board, rpt, pcbnew.EDA_UNITS_MM, True)
    print('DRC report written: %s (ok=%s)' % (rpt, ok))

    # summarise
    counts = {}
    unconnected = 0
    try:
        with open(rpt, 'r', encoding='utf-8', errors='replace') as fh:
            for line in fh:
                s = line.strip()
                if s.startswith('['):
                    key = s.split(']')[0].strip('[')
                    counts[key] = counts.get(key, 0) + 1
                if 'Unconnected items' in s:
                    unconnected += 1
    except OSError as exc:
        print('could not read report: %s' % exc)
        return 1

    print('')
    print('--- DRC summary ---')
    for k in sorted(counts, key=lambda k: -counts[k]):
        print('  %-40s %d' % (k, counts[k]))
    if not counts:
        print('  (no violations parsed -- open the .rpt to confirm)')
    return 0


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    sys.exit(main(sys.argv[1], '--no-fill' not in sys.argv))
