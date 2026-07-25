"""Targeted rip-up repair on an already-routed board.

Ingests all existing tracks/vias, then routes the named nets with
override enabled (may cross unprotected signal tracks); crossed victims
are ripped and rerouted. Keeps best state via .ckpt.
"""
import sys, math, collections, shutil
sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
import pcbnew
from router import RipupRouter, POWER, SIG_W, GRID


def _ingest(r, board):
    for t in board.GetTracks():
        code = t.GetNetCode()
        if t.GetClass() == 'PCB_VIA':
            p = t.GetPosition()
            gx, gy = r.mm2g(pcbnew.ToMM(p.x), pcbnew.ToMM(p.y))
            for L in (0, 1, 2):
                for ex in (-1, 0, 1):
                    for ey in (-1, 0, 1):
                        if 0 <= gx+ex < r.nx and 0 <= gy+ey < r.ny and r.owner[L, gx+ex, gy+ey] == 0:
                            r.owner[L, gx+ex, gy+ey] = code
            continue
        L = {int(pcbnew.F_Cu): 0, int(pcbnew.In2_Cu): 1, int(pcbnew.B_Cu): 2}.get(int(t.GetLayer()), 2)
        s, e = t.GetStart(), t.GetEnd()
        x0, y0, x1, y1 = pcbnew.ToMM(s.x), pcbnew.ToMM(s.y), pcbnew.ToMM(e.x), pcbnew.ToMM(e.y)
        steps = max(1, int(math.hypot(x1-x0, y1-y0)/(GRID/2)))
        hw = pcbnew.ToMM(t.GetWidth())/2
        rad = 0 if hw <= 0.21 else 1
        for i in range(steps+1):
            gx, gy = r.mm2g(x0+(x1-x0)*i/steps, y0+(y1-y0)*i/steps)
            for ex in range(-rad, rad+1):
                for ey in range(-rad, rad+1):
                    px, py = gx+ex, gy+ey
                    if 0 <= px < r.nx and 0 <= py < r.ny and r.owner[L, px, py] == 0:
                        r.owner[L, px, py] = code


def ripup_repair(pcb_path, W, H, net_names, rounds=4):
    board = pcbnew.LoadBoard(pcb_path)
    r = RipupRouter(board, W, H)
    r.relax_graze = True
    _ingest(r, board)
    netsbn = board.GetNetsByName()
    names = {v.GetNetCode(): str(k) for k, v in netsbn.items()}
    byname = {nm: c for c, nm in names.items()}
    r.nets_cache = {v.GetNetCode(): v for _, v in netsbn.items()}  # pre-cache: board accessors go stale after Removes
    gnd_codes = frozenset(c for c, nm in names.items() if nm == 'GND')
    power_codes = frozenset(c for c, nm in names.items() if nm.lstrip('/') in POWER or nm in POWER)
    big_codes = frozenset(c for c, pads in r.net_pads.items() if len(pads) >= 4)
    vref_codes = frozenset(c for c, nm in names.items() if nm == 'VREF')
    r.protected = power_codes | gnd_codes | big_codes | vref_codes

    def width_for(code):
        nm = names.get(code, '')
        return POWER.get(nm.lstrip('/'), POWER.get(nm, SIG_W))

    def rip_nets(vcs):
        """Rip several nets at once: ONE GetTracks pass before any Remove
        (board.Tracks() goes stale after batched Removes in pcbnew 7 swig)."""
        vcs = set(vcs)
        keep_geo = collections.defaultdict(list)
        to_remove = []
        for t in list(board.GetTracks()):
            vc = t.GetNetCode()
            if vc not in vcs:
                continue
            if t.GetClass() != 'PCB_VIA':
                s, e = t.GetStart(), t.GetEnd()
                x0, y0 = pcbnew.ToMM(s.x), pcbnew.ToMM(s.y)
                x1, y1 = pcbnew.ToMM(e.x), pcbnew.ToMM(e.y)
                if math.hypot(x1-x0, y1-y0) <= 3.6:
                    L = {int(pcbnew.F_Cu): 0, int(pcbnew.In2_Cu): 1, int(pcbnew.B_Cu): 2}.get(int(t.GetLayer()), 2)
                    keep_geo[vc].append((x0, y0, x1, y1, L))
                    continue
            to_remove.append(t)
        for t in to_remove:
            board.Remove(t)
        for vc in vcs:
            mask = (r.owner == vc) & (~r.is_pad)
            r.owner[mask] = 0
        for vc, geos in keep_geo.items():
            for (x0, y0, x1, y1, L) in geos:
                steps = max(1, int(math.hypot(x1-x0, y1-y0)/(GRID/2)))
                for i in range(steps+1):
                    gx, gy = r.mm2g(x0+(x1-x0)*i/steps, y0+(y1-y0)*i/steps)
                    if 0 <= gx < r.nx and 0 <= gy < r.ny and r.owner[L, gx, gy] == 0:
                        r.owner[L, gx, gy] = vc

    # --- ONE net per process: route it (override if needed), rip victims,
    #     reroute victims, save. Prints VICTIM_FAILS for the driver. ---
    nm = net_names[0]
    code = byname.get(nm)
    if code is None:
        cand = [c for c, n in names.items() if n.endswith(nm)]
        code = cand[0] if cand else None
    if code is None:
        print('NO NET:', nm)
        return None
    w = width_for(code)
    ok, paths = r.route_net(code, w)
    used_override = False
    if not ok:
        r.override = True
        ok, paths = r.route_net(code, w)
        r.override = False
        used_override = True
    if not ok:
        print('STILL_FAILED:', nm)
        return [nm]
    victims = set()
    if used_override:
        for path in paths:
            for (L, gx, gy) in path:
                o = r.owner[L, gx, gy]
                if o > 0 and o != code and not r.is_pad[L, gx, gy]:
                    victims.add(int(o))
    if victims:
        rip_nets(victims)
    r.commit(code, w, paths)
    vfails = []
    for vc in victims:
        okv, pv = r.route_net(vc, width_for(vc))
        r.commit(vc, width_for(vc), pv)
        if not okv:
            vfails.append(names[vc])
    pcbnew.SaveBoard(pcb_path, board)
    print('ROUTED:', nm, '| victims:', [names[v] for v in victims], '| VICTIM_FAILS:', vfails)
    return vfails


if __name__ == '__main__':
    pcb = sys.argv[1]
    nets = sys.argv[2].split(',')
    out = ripup_repair(pcb, 138, 114, nets)
    print('REMAINING:', out)
