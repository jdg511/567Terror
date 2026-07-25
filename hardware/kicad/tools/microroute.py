"""Exact-geometry single-net micro-router.

Routes the smallest disconnected cluster of a net to the rest using A* on a
fine lattice with EXACT collision checks via pcbnew SHAPE.Collide(), so any
found path respects real clearances (rotation-aware pads included).

Usage: python3 microroute.py <pcb> <netname> [<step> <clearance>]
"""
import sys, math, heapq, collections
import pcbnew

STEP = 0.25          # lattice pitch mm
CLR = 0.15           # clearance margin (board rule 0.13)
TRACK_W = 0.25
VIA_D, VIA_DRILL = 0.5, 0.3
EDGE = 0.65          # track centre to board edge
W, H, NOTCH = 138.0, 114.0, 11.0
LAYERS = [pcbnew.F_Cu, pcbnew.In2_Cu, pcbnew.B_Cu]
LIDX = {int(l): i for i, l in enumerate(LAYERS)}


def mm(v): return pcbnew.FromMM(v)
def to_mm(v): return pcbnew.ToMM(v)


def net_clusters(board, con, nc):
    """BFS clusters of a net's pads+tracks+vias using direct-neighbour APIs."""
    pads = [p for fp in board.GetFootprints() for p in fp.Pads() if p.GetNetCode() == nc]
    tracks = [t for t in board.GetTracks() if t.GetNetCode() == nc]
    items = {it.m_Uuid.AsString(): it for it in pads + tracks}
    adj = collections.defaultdict(set)
    for it in list(items.values()):
        uid = it.m_Uuid.AsString()
        for nb in list(con.GetConnectedTracks(it)) + list(con.GetConnectedPads(it)):
            nuid = nb.m_Uuid.AsString()
            if nuid in items:
                adj[uid].add(nuid); adj[nuid].add(uid)
    seen, clusters = set(), []
    for uid in items:
        if uid in seen:
            continue
        q, comp = [uid], set()
        while q:
            u = q.pop()
            if u in seen:
                continue
            seen.add(u); comp.add(u)
            q.extend(adj[u] - seen)
        clusters.append([items[u] for u in comp])
    return clusters


def item_layers(it):
    if it.GetClass() == 'PCB_VIA':
        return list(range(len(LAYERS)))
    if it.GetClass() == 'PAD':
        if it.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
            return list(range(len(LAYERS)))
        return [i for i, l in enumerate(LAYERS) if it.IsOnLayer(l)]
    return [LIDX[int(it.GetLayer())]] if int(it.GetLayer()) in LIDX else []


class Obstacles:
    """Per-layer bucketed exact shapes of foreign copper."""
    def __init__(self, board, nc, bbox):
        self.buck = [collections.defaultdict(list) for _ in LAYERS]
        self.holes = []      # (x, y, dia) all layers
        self.cell = 2.0
        x0, y0, x1, y1 = bbox
        def add(shape, layers, bb):
            bx0, by0 = to_mm(bb.GetLeft())-1, to_mm(bb.GetTop())-1
            bx1, by1 = to_mm(bb.GetRight())+1, to_mm(bb.GetBottom())+1
            if bx1 < x0 or bx0 > x1 or by1 < y0 or by0 > y1:
                return
            for L in layers:
                for cx in range(int(bx0/self.cell), int(bx1/self.cell)+1):
                    for cy in range(int(by0/self.cell), int(by1/self.cell)+1):
                        self.buck[L][(cx, cy)].append(shape)
        for fp in board.GetFootprints():
            for p in fp.Pads():
                if p.GetNetCode() == nc and p.GetAttribute() != pcbnew.PAD_ATTRIB_NPTH:
                    if p.GetDrillSize().x:
                        self.holes.append((to_mm(p.GetPosition().x), to_mm(p.GetPosition().y),
                                           to_mm(p.GetDrillSize().x)))
                    continue
                sh = p.GetEffectiveShape()
                add(sh, item_layers(p), p.GetBoundingBox())
                if p.GetDrillSize().x:
                    self.holes.append((to_mm(p.GetPosition().x), to_mm(p.GetPosition().y),
                                       to_mm(p.GetDrillSize().x)))
        for t in board.GetTracks():
            if t.GetNetCode() == nc:
                if t.GetClass() == 'PCB_VIA':
                    self.holes.append((to_mm(t.GetPosition().x), to_mm(t.GetPosition().y),
                                       to_mm(t.GetDrill())))
                continue
            sh = t.GetEffectiveShape()
            if t.GetClass() == 'PCB_VIA':
                add(sh, list(range(len(LAYERS))), t.GetBoundingBox())
                self.holes.append((to_mm(t.GetPosition().x), to_mm(t.GetPosition().y),
                                   to_mm(t.GetDrill())))
            else:
                add(sh, item_layers(t), t.GetBoundingBox())

    def seg_clear(self, L, ax, ay, bx, by, width=TRACK_W, clr=CLR):
        seg = pcbnew.SHAPE_SEGMENT(pcbnew.VECTOR2I(mm(ax), mm(ay)),
                                   pcbnew.VECTOR2I(mm(bx), mm(by)), mm(width))
        cells = set()
        for f in (0.0, 0.5, 1.0):
            cells.add((int((ax+(bx-ax)*f)/self.cell), int((ay+(by-ay)*f)/self.cell)))
        tested = set()
        for (cx, cy) in cells:
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for sh in self.buck[L].get((cx+dx, cy+dy), ()):
                        if id(sh) in tested:
                            continue
                        tested.add(id(sh))
                        if sh.Collide(seg, mm(clr)):
                            return False
        return True

    def via_clear(self, x, y):
        for L in range(len(LAYERS)):
            if not self.seg_clear(L, x, y, x, y, width=VIA_D, clr=CLR):
                return False
        for (hx, hy, hd) in self.holes:
            if math.hypot(x-hx, y-hy) < (VIA_DRILL+hd)/2 + 0.55:
                return False
        return True


def in_board(x, y):
    if not (EDGE <= x <= W-EDGE and EDGE <= y <= H-EDGE):
        return False
    n = NOTCH + 0.65
    if (x < n or x > W-n) and (y < n or y > H-n):
        return False
    return True


def terminals(cluster):
    """(x, y, layerset) anchor points of a cluster: pad centres + track ends."""
    out = []
    for it in cluster:
        if it.GetClass() == 'PAD':
            p = it.GetPosition()
            out.append((to_mm(p.x), to_mm(p.y), tuple(item_layers(it))))
        elif it.GetClass() == 'PCB_VIA':
            p = it.GetPosition()
            out.append((to_mm(p.x), to_mm(p.y), tuple(range(len(LAYERS)))))
        else:
            Ls = tuple(item_layers(it))
            for pt in (it.GetStart(), it.GetEnd()):
                out.append((to_mm(pt.x), to_mm(pt.y), Ls))
    return out


def route_pair(board, obs, src_terms, dst_terms, max_nodes=400000):
    def node_of(x, y):
        return (round(x/STEP), round(y/STEP))
    dsts = {}
    for (x, y, Ls) in dst_terms:
        gx, gy = node_of(x, y)
        for L in Ls:
            dsts[(L, gx, gy)] = (x, y)
    dgxs = [k[1] for k in dsts]; dgys = [k[2] for k in dsts]
    bx0, bx1, by0, by1 = min(dgxs), max(dgxs), min(dgys), max(dgys)
    def h(L, gx, gy):
        dx = max(bx0-gx, 0, gx-bx1); dy = max(by0-gy, 0, gy-by1)
        return (dx+dy)*STEP*0.99
    pq, seen, came = [], {}, {}
    for (x, y, Ls) in src_terms:
        gx, gy = node_of(x, y)
        for L in Ls:
            st = (L, gx, gy)
            if st not in seen or seen[st] > 0:
                seen[st] = 0.0
                came[st] = (None, (x, y))
                heapq.heappush(pq, (h(L, gx, gy), 0.0, st))
    DIRS = [(1,0,1),(0,1,1),(-1,0,1),(0,-1,1),(1,1,1.414),(1,-1,1.414),(-1,1,1.414),(-1,-1,1.414)]
    expanded = 0
    while pq and expanded < max_nodes:
        f, g, st = heapq.heappop(pq)
        if seen.get(st, 1e18) < g - 1e-9:
            continue
        expanded += 1
        L, gx, gy = st
        if st in dsts:
            # reconstruct
            path = [st]
            cur = st
            while came[cur][0] is not None:
                cur = came[cur][0]
                path.append(cur)
            start_anchor = came[cur][1]
            return path[::-1], start_anchor, dsts[st]
        x, y = gx*STEP, gy*STEP
        for (dx, dy, cost) in DIRS:
            nx_, ny_ = gx+dx, gy+dy
            x2, y2 = nx_*STEP, ny_*STEP
            if not in_board(x2, y2):
                continue
            nst = (L, nx_, ny_)
            ng = g + cost*STEP
            if seen.get(nst, 1e18) <= ng + 1e-9:
                continue
            if not obs.seg_clear(L, x, y, x2, y2):
                continue
            seen[nst] = ng
            came[nst] = (st, None)
            heapq.heappush(pq, (ng + h(L, nx_, ny_), ng, nst))
        # layer change (via)
        for L2 in range(len(LAYERS)):
            if L2 == L:
                continue
            nst = (L2, gx, gy)
            ng = g + 1.4   # via cost
            if seen.get(nst, 1e18) <= ng + 1e-9:
                continue
            if not obs.via_clear(x, y):
                continue
            seen[nst] = ng
            came[nst] = (st, None)
            heapq.heappush(pq, (ng + h(L2, gx, gy), ng, nst))
    return None, None, None


def emit(board, ni, path, start_anchor, end_anchor):
    pts = []
    for (L, gx, gy) in path:
        pts.append((L, gx*STEP, gy*STEP))
    def add_track(L, ax, ay, bx, by):
        if abs(ax-bx) < 1e-6 and abs(ay-by) < 1e-6:
            return
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(pcbnew.VECTOR2I(mm(ax), mm(ay)))
        t.SetEnd(pcbnew.VECTOR2I(mm(bx), mm(by)))
        t.SetWidth(mm(TRACK_W)); t.SetLayer(LAYERS[L]); t.SetNet(ni)
        board.Add(t)
    def add_via(x, y):
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
        v.SetDrill(mm(VIA_DRILL)); v.SetWidth(mm(VIA_D)); v.SetNet(ni)
        board.Add(v)
    add_track(pts[0][0], start_anchor[0], start_anchor[1], pts[0][1], pts[0][2])
    i = 0
    while i < len(pts)-1:
        L, ax, ay = pts[i]
        L2, bx, by = pts[i+1]
        if L != L2:
            add_via(ax, ay)
        else:
            # merge collinear run
            j = i+1
            while j+1 < len(pts) and pts[j+1][0] == L:
                x0, y0 = pts[i][1], pts[i][2]
                x1, y1 = pts[j][1], pts[j][2]
                x2, y2 = pts[j+1][1], pts[j+1][2]
                if abs((x1-x0)*(y2-y0)-(y1-y0)*(x2-x0)) > 1e-9:
                    break
                j += 1
            add_track(L, ax, ay, pts[j][1], pts[j][2])
            i = j
            continue
        i += 1
    add_track(pts[-1][0], pts[-1][1], pts[-1][2], end_anchor[0], end_anchor[1])


def microroute(pcb, netname, save=True):
    board = pcbnew.LoadBoard(pcb)
    board.BuildConnectivity()
    con = board.GetConnectivity()
    net = None
    for k, v in board.GetNetsByName().items():
        if str(k) == netname:
            net = v
    if net is None:
        print('NO NET', netname); return False
    nc = net.GetNetCode()
    joined = 0
    for _ in range(12):
        clusters = net_clusters(board, con, nc)
        if len(clusters) <= 1:
            break
        clusters.sort(key=len)
        src = clusters[0]
        dst = [it for cl in clusters[1:] for it in cl]
        st = terminals(src); dt = terminals(dst)
        # bbox for obstacles
        xs = [p[0] for p in st+dt]; ys = [p[1] for p in st+dt]
        bbox = (min(xs)-10, min(ys)-10, max(xs)+10, max(ys)+10)
        obs = Obstacles(board, nc, bbox)
        path, sa, ea = route_pair(board, obs, st, dt)
        if path is None:
            print(f'FAIL cluster {len(clusters)} left'); break
        emit(board, net, path, sa, ea)
        joined += 1
        board.BuildConnectivity()
        con = board.GetConnectivity()
    clusters = net_clusters(board, con, nc)
    print(f'{netname}: joined {joined}, clusters now {len(clusters)}')
    if save and joined:
        pcbnew.SaveBoard(pcb, board)
    return len(clusters) <= 1


if __name__ == '__main__':
    pcb, netname = sys.argv[1], sys.argv[2]
    if len(sys.argv) > 3: STEP = float(sys.argv[3])
    if len(sys.argv) > 4: CLR = float(sys.argv[4])
    ok = microroute(pcb, netname)
    print('OK' if ok else 'INCOMPLETE')
