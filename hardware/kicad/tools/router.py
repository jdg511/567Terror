"""Grid A* autorouter for pcbnew boards (2-layer).
0.635mm grid; F.Cu prefers horizontal, B.Cu vertical; 45-deg moves; via cost.
GND left to the pours + stitching vias."""
import pcbnew, heapq, math, collections
import numpy as np

GRID = 0.55
VIA_COST = 22.0
POWER = {'VA':0.3,'+5V':0.3,'3V3':0.25,'V567':0.3,'VDIRT':0.3,'V9RAW':0.3,
         'VIN_RAW':0.4,'VPROT':0.4,'PICO_VSYS':0.3,'BUCK_SW':0.4}
SIG_W = 0.25
CLR = 0.22

class Router:
    relax_graze = False
    def __init__(self, board, W, H):
        self.b, self.W, self.H = board, W, H
        self.nx, self.ny = int(W/GRID)+1, int(H/GRID)+1
        # owner[layer, x, y] = net code blocking that cell (0 = free, -1 = hard block)
        self.owner = np.zeros((3, self.nx, self.ny), dtype=np.int32)
        self.is_pad = np.zeros((3, self.nx, self.ny), dtype=bool)
        self.netmap = {}
        for n in board.GetNetsByName().items():
            self.netmap[str(n[0])] = n[1].GetNetCode()
        self._edges()
        self._pads()

    def g2mm(self, gx, gy): return gx*GRID, gy*GRID
    def mm2g(self, x, y): return int(round(x/GRID)), int(round(y/GRID))

    def _block_rect(self, layer, x0, y0, x1, y1, code):
        gx0, gy0 = max(0,int((x0)/GRID)), max(0,int((y0)/GRID))
        gx1, gy1 = min(self.nx-1,int(math.ceil(x1/GRID))), min(self.ny-1,int(math.ceil(y1/GRID)))
        if gx1 < gx0 or gy1 < gy0: return
        region = self.owner[layer, gx0:gx1+1, gy0:gy1+1]
        region[region == 0] = code

    def _edges(self):
        m = 1.0; n = 11.0
        for L in (0,1,2):
            self._block_rect(L, 0,0, self.W, m, -1); self._block_rect(L, 0,self.H-m, self.W, self.H, -1)
            self._block_rect(L, 0,0, m, self.H, -1); self._block_rect(L, self.W-m,0, self.W, self.H, -1)
            for cx,cy in ((0,0),(self.W,0),(0,self.H),(self.W,self.H)):
                x0 = 0 if cx==0 else self.W-n-m; x1 = n+m if cx==0 else self.W
                y0 = 0 if cy==0 else self.H-n-m; y1 = n+m if cy==0 else self.H
                self._block_rect(L, x0,y0,x1,y1, -1)

    def _pads(self):
        self.net_pads = collections.defaultdict(list)
        for fp in self.b.GetFootprints():
            for p in fp.Pads():
                pos = p.GetPosition(); x, y = pcbnew.ToMM(pos.x), pcbnew.ToMM(pos.y)
                bb = p.GetBoundingBox()   # rotation-aware, absolute
                bx0, by0 = pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop())
                bx1, by1 = pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom())
                code = p.GetNetCode()
                tht = p.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH)
                layers = (0,1,2) if tht else ((0,) if p.IsOnLayer(pcbnew.F_Cu) else (2,))
                for L in layers:
                    self._block_rect(L, bx0-CLR, by0-CLR, bx1+CLR, by1+CLR, code if code else -1)
                    gx0i, gy0i = max(0,int((bx0-CLR)/GRID)), max(0,int((by0-CLR)/GRID))
                    gx1i, gy1i = min(self.nx-1,int(math.ceil((bx1+CLR)/GRID))), min(self.ny-1,int(math.ceil((by1+CLR)/GRID)))
                    self.is_pad[L, gx0i:gx1i+1, gy0i:gy1i+1] = True
                if code:
                    gx, gy = self.mm2g(x, y)
                    if 0 <= gx < self.nx and 0 <= gy < self.ny:
                        self.net_pads[code].append((gx, gy, 0 if 0 in layers else 2, tht))
                        for L in layers:
                            self.owner[L, gx, gy] = code

    def _passable(self, L, gx, gy, code):
        o = self.owner[L, gx, gy]
        return o == 0 or o == code

    def route_net(self, code, width, merged=None):
        pads = self.net_pads.get(code, [])
        if len(pads) < 2: return True, []
        halo = 1 if width > 0.45 else 0
        no_diag = width > 0.28
        merged = merged or set()
        tree = set()
        def join(p):
            gx, gy, L, tht = p
            tree.add((L, gx, gy))
            if tht:
                for L2 in (0,1,2): tree.add((L2, gx, gy))
        join(pads[0])
        in_group = lambda p: (p[0], p[1]) in merged
        group_joined = in_group(pads[0])
        remaining = []
        for p in pads[1:]:
            if group_joined and in_group(p):
                join(p)
            else:
                remaining.append(p)
        all_paths = []
        while remaining:
            # nearest target by heuristic
            targets = {}
            for i,(gx,gy,L,tht) in enumerate(remaining):
                targets[(L,gx,gy)] = i
                if tht:
                    for L2 in (0,1,2): targets[(L2,gx,gy)] = i
            best = self._astar(tree, targets, code, halo, no_diag)
            if best is None: return False, all_paths
            path, ti = best
            all_paths.append(path)
            for node in path:
                tree.add(node)
                L, gx, gy = node
            reached = remaining.pop(ti)
            join(reached)
            if in_group(reached) and not group_joined:
                group_joined = True
                still = [p for p in remaining if not in_group(p)]
                for p in remaining:
                    if in_group(p): join(p)
                remaining = still
        return True, all_paths

    def _astar(self, sources, targets, code, halo, no_diag=False):
        tgt_cells = set(targets.keys())
        def h(node):
            L, gx, gy = node
            best = 1e18
            for (tL, tx, ty) in tgt_cells:
                dx, dy = abs(gx-tx), abs(gy-ty)
                d = max(dx,dy) + 0.42*min(dx,dy)
                if d < best: best = d
            return best
        openq = []
        gscore = {}
        came = {}
        for s in sources:
            gscore[s] = 0.0
            heapq.heappush(openq, (h(s), 0.0, s, None))
        visited = set()
        expansions = 0
        while openq:
            f, g, node, parent = heapq.heappop(openq)
            if node in visited: continue
            visited.add(node); came[node] = parent
            expansions += 1
            if expansions > 250000: return None
            if node in tgt_cells:
                path = []
                n = node
                while n is not None:
                    path.append(n); n = came[n]
                return path, targets[node]
            L, gx, gy = node
            moves = ((1,0,1),(-1,0,1),(0,1,1),(0,-1,1)) if no_diag else \
                    ((1,0,1),(-1,0,1),(0,1,1),(0,-1,1),(1,1,1.45),(1,-1,1.45),(-1,1,1.45),(-1,-1,1.45))
            for dx, dy, base in moves:
                nxx, nyy = gx+dx, gy+dy
                if not (0 <= nxx < self.nx and 0 <= nyy < self.ny): continue
                if dx and dy:
                    p1 = self._passable(L, gx+dx, gy, code); p2 = self._passable(L, gx, gy+dy, code)
                    if not p1 and not p2: continue
                    if not self.relax_graze and ((not p1 and self.is_pad[L, gx+dx, gy]) or (not p2 and self.is_pad[L, gx, gy+dy])): continue

                ok = True
                for hx in range(-halo, halo+1):
                    for hy in range(-halo, halo+1):
                        px, py = nxx+hx, nyy+hy
                        if not (0 <= px < self.nx and 0 <= py < self.ny) or not self._passable(L, px, py, code):
                            ok = False; break
                    if not ok: break
                if not ok: continue
                # directional preference: F.Cu (0) horizontal, B.Cu vertical
                cost = base
                if dx and not dy: cost *= (1.0 if L==0 else (1.15 if L==1 else 1.35))
                if dy and not dx: cost *= (1.0 if L==2 else (1.15 if L==1 else 1.35))
                nn = (L, nxx, nyy)
                ng = g + cost
                if ng < gscore.get(nn, 1e18):
                    gscore[nn] = ng
                    heapq.heappush(openq, (ng + h(nn), ng, nn, node))
            # through-via: must be clear on ALL routing layers at this cell
            via_clear = True
            for L2 in (0,1,2):
                c0 = self._passable(L2, gx, gy, code) and \
                     (self.owner[L2, gx, gy] == code or not self.is_pad[L2, gx, gy])
                if not c0: via_clear = False; break
                for ex, ey in ((1,0),(-1,0),(0,1),(0,-1)):
                    px, py = gx+ex, gy+ey
                    if 0 <= px < self.nx and 0 <= py < self.ny:
                        o = self.owner[L2, px, py]
                        if o > 0 and o != code and not self.is_pad[L2, px, py]:
                            via_clear = False; break
                if not via_clear: break
            if via_clear:
                for oL in (0,1,2):
                    if oL == L: continue
                    nn = (oL, gx, gy)
                    ng = g + VIA_COST
                    if ng < gscore.get(nn, 1e18):
                        gscore[nn] = ng
                        heapq.heappush(openq, (ng + h(nn), ng, nn, node))
        return None

    def commit(self, code, width, paths):
        netinfo = self.b.FindNet(code) if hasattr(self.b,'FindNet') else None
        nets = {i.GetNetCode(): i for _, i in self.b.GetNetsByName().items()}
        ni = nets[code]
        for path in paths:
            # mark ownership + emit segments
            for (L, gx, gy) in path:
                self.owner[L, gx, gy] = code
            # split path into per-layer runs; vias at layer changes
            runs = []
            cur = [path[0]]
            for a, b2 in zip(path, path[1:]):
                if a[0] != b2[0]:
                    runs.append(cur); runs.append('VIA'); cur = [b2]
                else:
                    cur.append(b2)
            runs.append(cur)
            prev_end = None
            for r in runs:
                if r == 'VIA':
                    v = pcbnew.PCB_VIA(self.b)
                    x, y = self.g2mm(prev_end[1], prev_end[2])
                    v.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
                    v.SetDrill(pcbnew.FromMM(0.3)); v.SetWidth(pcbnew.FromMM(0.5))
                    v.SetNet(ni); self.b.Add(v)
                    for L2 in (0,1,2):
                        for ex, ey in ((0,0),(1,0),(-1,0),(0,1),(0,-1)):
                            px, py = prev_end[1]+ex, prev_end[2]+ey
                            if 0 <= px < self.nx and 0 <= py < self.ny and self.owner[L2,px,py]==0:
                                self.owner[L2,px,py] = code
                    continue
                # simplify collinear
                pts = [r[0]]
                for a, b2, c in zip(r, r[1:], r[2:]):
                    if (b2[1]-a[1], b2[2]-a[2]) != (c[1]-b2[1], c[2]-b2[2]):
                        pts.append(b2)
                if len(r) > 1: pts.append(r[-1])
                layer = {0: pcbnew.F_Cu, 1: pcbnew.In2_Cu, 2: pcbnew.B_Cu}[r[0][0]]
                for a, b2 in zip(pts, pts[1:]):
                    t = pcbnew.PCB_TRACK(self.b)
                    ax, ay = self.g2mm(a[1], a[2]); bx, by = self.g2mm(b2[1], b2[2])
                    t.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(ax), pcbnew.FromMM(ay)))
                    t.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(bx), pcbnew.FromMM(by)))
                    t.SetWidth(pcbnew.FromMM(width)); t.SetLayer(layer)
                    t.SetNet(ni); self.b.Add(t)
                prev_end = r[-1]

def route_board(pcb_path, W, H, skip_nets=('GND',''), priority=(), order_mode='asc'):
    board = pcbnew.LoadBoard(pcb_path)
    r = Router(board, W, H)
    names = {code: name for name, code in ((str(k), v.GetNetCode()) for k, v in board.GetNetsByName().items())}
    # pre-route same-net pad pairs that are close together (follower feedbacks
    # on adjacent SOIC pins etc.) with direct segments - A* can't thread these
    nets_obj = {i.GetNetCode(): i for _, i in board.GetNetsByName().items()}
    prerouted = collections.defaultdict(set)
    for fp in board.GetFootprints():
        pads = [(p, p.GetNetCode()) for p in fp.Pads() if p.GetNetCode()]
        for i in range(len(pads)):
            for j in range(i+1, len(pads)):
                pa, ca = pads[i]; pb, cb = pads[j]
                if ca != cb: continue
                nm = names.get(ca, '')
                if nm in skip_nets: continue
                A, B = pa.GetPosition(), pb.GetPosition()
                d = math.hypot(pcbnew.ToMM(A.x-B.x), pcbnew.ToMM(A.y-B.y))
                if d > 3.5 or d < 0.05: continue
                la = pa.IsOnLayer(pcbnew.F_Cu); lb = pb.IsOnLayer(pcbnew.F_Cu)
                if not (la and lb): continue
                # corridor check: no foreign cells along the segment
                ax, ay = pcbnew.ToMM(A.x), pcbnew.ToMM(A.y)
                bx2, by2 = pcbnew.ToMM(B.x), pcbnew.ToMM(B.y)
                steps = max(2, int(d / (GRID/2)))
                clearP = True
                for si in range(steps+1):
                    gx, gy = r.mm2g(ax+(bx2-ax)*si/steps, ay+(by2-ay)*si/steps)
                    for ex in (-1,0,1):
                        for ey in (-1,0,1):
                            px, py = gx+ex, gy+ey
                            if 0<=px<r.nx and 0<=py<r.ny and r.owner[0,px,py] not in (0, ca):
                                clearP = False
                    if not clearP: break
                if not clearP: continue
                t = pcbnew.PCB_TRACK(board)
                t.SetStart(A); t.SetEnd(B)
                t.SetWidth(pcbnew.FromMM(SIG_W)); t.SetLayer(pcbnew.F_Cu)
                t.SetNet(nets_obj[ca]); board.Add(t)
                for si in range(steps+1):
                    gx, gy = r.mm2g(ax+(bx2-ax)*si/steps, ay+(by2-ay)*si/steps)
                    if 0<=gx<r.nx and 0<=gy<r.ny and r.owner[0,gx,gy]==0:
                        r.owner[0,gx,gy] = ca
                prerouted[ca].add((r.mm2g(ax, ay)))
                prerouted[ca].add((r.mm2g(bx2, by2)))
    # order: power first, then by pad-spread ascending
    jobs = []
    for code, pads in r.net_pads.items():
        nm = names.get(code, '')
        if nm in skip_nets or len(pads) < 2: continue
        w = POWER.get(nm, SIG_W)
        xs = [p[0] for p in pads]; ys = [p[1] for p in pads]
        spread = (max(xs)-min(xs)) + (max(ys)-min(ys))
        pri = 0 if nm in POWER else (1 if nm in priority else 2)
        jobs.append((pri, spread if order_mode=='asc' else -spread, code, nm, w))
    jobs.sort()
    fails = []
    for _, _, code, nm, w in jobs:
        ok, paths = r.route_net(code, w)
        r.commit(code, w, paths)
        if not ok: fails.append(nm)
    pcbnew.SaveBoard(pcb_path, board)
    return fails, len(jobs)

def repair(pcb_path, W, H, net_names):
    """route specific nets on an already-routed board (tracks ingested)"""
    board = pcbnew.LoadBoard(pcb_path)
    r = Router(board, W, H)
    r.relax_graze = True
    # ingest existing tracks/vias as owned cells
    for t in board.GetTracks():
        code = t.GetNetCode()
        if t.GetClass() == 'PCB_VIA':
            p = t.GetPosition()
            gx, gy = r.mm2g(pcbnew.ToMM(p.x), pcbnew.ToMM(p.y))
            for L in (0,1,2):
                for ex in (-1,0,1):
                    for ey in (-1,0,1):
                        if 0<=gx+ex<r.nx and 0<=gy+ey<r.ny:
                            if r.owner[L,gx+ex,gy+ey]==0: r.owner[L,gx+ex,gy+ey]=code
            continue
        L = {int(pcbnew.F_Cu):0, int(pcbnew.In2_Cu):1, int(pcbnew.B_Cu):2}.get(int(t.GetLayer()), 2)
        s, e = t.GetStart(), t.GetEnd()
        x0,y0,x1,y1 = (pcbnew.ToMM(s.x),pcbnew.ToMM(s.y),pcbnew.ToMM(e.x),pcbnew.ToMM(e.y))
        steps = max(1,int(math.hypot(x1-x0,y1-y0)/ (GRID/2)))
        hw = pcbnew.ToMM(t.GetWidth())/2
        rad = 0 if hw <= 0.21 else 1
        for i in range(steps+1):
            x = x0+(x1-x0)*i/steps; y = y0+(y1-y0)*i/steps
            gx, gy = r.mm2g(x,y)
            for ex in range(-rad,rad+1):
                for ey in range(-rad,rad+1):
                    px,py = gx+ex, gy+ey
                    if 0<=px<r.nx and 0<=py<r.ny and r.owner[L,px,py]==0:
                        r.owner[L,px,py]=code
    names = {str(k): v.GetNetCode() for k, v in board.GetNetsByName().items()}
    fails = []
    for nm in net_names:
        code = names.get(nm) or names.get('/'+nm)
        if code is None:
            cand = [c for n,c in names.items() if n.endswith(nm)]
            code = cand[0] if cand else None
        if code is None: fails.append(nm); continue
        w = POWER.get(nm.lstrip('/'), SIG_W)
        ok, paths = r.route_net(code, w)
        r.commit(code, w, paths)
        if not ok: fails.append(nm)
    pcbnew.SaveBoard(pcb_path, board)
    return fails

# ---------------------------------------------------------------------------
# rip-up & reroute completion pass
# ---------------------------------------------------------------------------
class RipupRouter(Router):
    override = False
    protected = frozenset()
    def _passable(self, L, gx, gy, code):
        o = self.owner[L, gx, gy]
        if o == 0 or o == code: return True
        if self.override and o > 0 and o not in self.protected and not self.is_pad[L, gx, gy]:
            return True     # may cross foreign signal TRACKS (they'll be ripped)
        return False

def full_route(pcb_path, W, H, order_mode='asc', priority=(), max_rip_rounds=6):
    board = pcbnew.LoadBoard(pcb_path)
    r = RipupRouter(board, W, H)
    names = {code: name for name, code in ((str(k), v.GetNetCode()) for k, v in board.GetNetsByName().items())}
    nets_obj = {i.GetNetCode(): i for _, i in board.GetNetsByName().items()}
    # --- preroute close same-net pairs (corridor-checked) ---
    prerouted = collections.defaultdict(set)
    for fp in board.GetFootprints():
        pads = [(p, p.GetNetCode()) for p in fp.Pads() if p.GetNetCode()]
        for i in range(len(pads)):
            for j in range(i+1, len(pads)):
                pa, ca = pads[i]; pb, cb = pads[j]
                if ca != cb or names.get(ca,'') in ('GND',''): continue
                A, B = pa.GetPosition(), pb.GetPosition()
                d = math.hypot(pcbnew.ToMM(A.x-B.x), pcbnew.ToMM(A.y-B.y))
                if d > 3.5 or d < 0.05: continue
                if not (pa.IsOnLayer(pcbnew.F_Cu) and pb.IsOnLayer(pcbnew.F_Cu)): continue
                ax, ay = pcbnew.ToMM(A.x), pcbnew.ToMM(A.y); bx2, by2 = pcbnew.ToMM(B.x), pcbnew.ToMM(B.y)
                steps = max(2, int(d/(GRID/2))); okc = True
                for si in range(steps+1):
                    gx, gy = r.mm2g(ax+(bx2-ax)*si/steps, ay+(by2-ay)*si/steps)
                    for ex in (-1,0,1):
                        for ey in (-1,0,1):
                            px, py = gx+ex, gy+ey
                            if 0<=px<r.nx and 0<=py<r.ny and r.owner[0,px,py] not in (0, ca): okc = False
                    if not okc: break
                if not okc: continue
                t = pcbnew.PCB_TRACK(board); t.SetStart(A); t.SetEnd(B)
                t.SetWidth(pcbnew.FromMM(SIG_W)); t.SetLayer(pcbnew.F_Cu)
                t.SetNet(nets_obj[ca]); board.Add(t)
                for si in range(steps+1):
                    gx, gy = r.mm2g(ax+(bx2-ax)*si/steps, ay+(by2-ay)*si/steps)
                    if 0<=gx<r.nx and 0<=gy<r.ny and r.owner[0,gx,gy]==0: r.owner[0,gx,gy]=ca
                prerouted[ca].add(r.mm2g(ax,ay)); prerouted[ca].add(r.mm2g(bx2,by2))
    # --- job list ---
    jobs = []
    for code, pads in r.net_pads.items():
        nm = names.get(code, '')
        if nm in ('GND','') or len(pads) < 2: continue
        w = POWER.get(nm, SIG_W)
        xs = [p[0] for p in pads]; ys = [p[1] for p in pads]
        spread = (max(xs)-min(xs))+(max(ys)-min(ys))
        pri = 0 if nm in POWER else (1 if nm in priority else 2)
        jobs.append((pri, spread if order_mode=='asc' else -spread, code, nm, w))
    jobs.sort()
    width_of = {code: w for _,_,code,_,w in jobs}
    r.protected = frozenset(code for _,_,code,nm,_ in jobs if nm in POWER)
    def rip_net(vc):
        keep = []
        for t in list(board.GetTracks()):
            if t.GetNetCode() != vc: continue
            if t.GetClass() != 'PCB_VIA':
                s, e = t.GetStart(), t.GetEnd()
                if math.hypot(pcbnew.ToMM(s.x-e.x), pcbnew.ToMM(s.y-e.y)) <= 3.6:
                    keep.append(t); continue
            board.Remove(t)
        mask = (r.owner == vc) & (~r.is_pad)
        r.owner[mask] = 0
        for t in keep:   # re-ingest kept short segments
            s, e = t.GetStart(), t.GetEnd()
            x0,y0,x1,y1 = pcbnew.ToMM(s.x),pcbnew.ToMM(s.y),pcbnew.ToMM(e.x),pcbnew.ToMM(e.y)
            L = {int(pcbnew.F_Cu):0, int(pcbnew.In2_Cu):1, int(pcbnew.B_Cu):2}.get(int(t.GetLayer()), 2)
            steps = max(1,int(math.hypot(x1-x0,y1-y0)/(GRID/2)))
            for i in range(steps+1):
                gx, gy = r.mm2g(x0+(x1-x0)*i/steps, y0+(y1-y0)*i/steps)
                if 0<=gx<r.nx and 0<=gy<r.ny and r.owner[L,gx,gy]==0:
                    r.owner[L,gx,gy] = vc
    fails = []
    for _,_,code,nm,w in jobs:
        ok, paths = r.route_net(code, w, merged=prerouted.get(code))
        r.commit(code, w, paths)
        if not ok: fails.append(code)
    # --- rip-up rounds with best-state checkpointing ---
    best_n = len(fails)
    pcbnew.SaveBoard(pcb_path + '.ckpt', board)
    best_fails = list(fails)
    worse_streak = 0
    for rnd in range(max_rip_rounds):
        if not fails: break
        print(f'  rip round {rnd}: {len(fails)} to fix:', [names[c] for c in fails][:8])
        next_fails = []
        seen_nf = set()
        for code in fails:
            w = width_of[code]
            r.override = True
            ok, paths = r.route_net(code, w, merged=prerouted.get(code))
            r.override = False
            if not ok:
                if code not in seen_nf: next_fails.append(code); seen_nf.add(code)
                continue
            # victims = nets whose track cells the new paths cross
            victims = set()
            for path in paths:
                for (L,gx,gy) in path:
                    o = r.owner[L,gx,gy]
                    if o > 0 and o != code and not r.is_pad[L,gx,gy]:
                        victims.add(int(o))
            for vc in victims:
                rip_net(vc)
            r.commit(code, w, paths)
            for vc in victims:
                okv, pv = r.route_net(vc, width_of.get(vc, SIG_W), merged=prerouted.get(vc))
                r.commit(vc, width_of.get(vc, SIG_W), pv)
                if not okv and vc not in seen_nf:
                    next_fails.append(vc); seen_nf.add(vc)
        fails = next_fails
        if len(fails) < best_n:
            best_n = len(fails); best_fails = list(fails); worse_streak = 0
            pcbnew.SaveBoard(pcb_path + '.ckpt', board)
        else:
            worse_streak += 1
            if worse_streak >= 2: break
    import shutil as _sh
    _sh.copy(pcb_path + '.ckpt', pcb_path)
    return [names[c] for c in best_fails]
