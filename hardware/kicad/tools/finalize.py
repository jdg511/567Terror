"""Post-route finalize: GND via-in-pad, stitching, zone fill, save."""
import sys, math; sys.path.insert(0,'/home/claude/work/hardware/kicad/tools')
import pcbnew, router

def finalize(pcb, W=138, H=114, stitch_step=9):
    board = pcbnew.LoadBoard(pcb)
    r = router.RipupRouter(board, W, H)
    for t in board.GetTracks():
        code = t.GetNetCode()
        if t.GetClass() == 'PCB_VIA':
            p = t.GetPosition(); gx, gy = r.mm2g(pcbnew.ToMM(p.x), pcbnew.ToMM(p.y))
            for L in (0,1,2):
                for ex in (-1,0,1):
                    for ey in (-1,0,1):
                        if 0<=gx+ex<r.nx and 0<=gy+ey<r.ny and r.owner[L,gx+ex,gy+ey]==0:
                            r.owner[L,gx+ex,gy+ey]=code
            continue
        L = {int(pcbnew.F_Cu):0, int(pcbnew.In2_Cu):1, int(pcbnew.B_Cu):2}.get(int(t.GetLayer()), 2)
        s,e = t.GetStart(), t.GetEnd()
        x0,y0,x1,y1 = pcbnew.ToMM(s.x),pcbnew.ToMM(s.y),pcbnew.ToMM(e.x),pcbnew.ToMM(e.y)
        steps = max(1,int(math.hypot(x1-x0,y1-y0)/0.25))
        for i in range(steps+1):
            gx, gy = r.mm2g(x0+(x1-x0)*i/steps, y0+(y1-y0)*i/steps)
            if 0<=gx<r.nx and 0<=gy<r.ny and r.owner[L,gx,gy]==0:
                r.owner[L,gx,gy] = code or -1
    gnd_net = None
    for nm, net in board.GetNetsByName().items():
        if str(nm)=='GND': gnd_net = net
    gnd = gnd_net.GetNetCode()
    added = 0
    for fp in board.GetFootprints():
        for p in fp.Pads():
            if p.GetNetCode() != gnd or p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH: continue
            pos = p.GetPosition()
            sz = p.GetSize()
            if min(pcbnew.ToMM(sz.x), pcbnew.ToMM(sz.y)) < 0.75: continue  # too small for via-in-pad
            v = pcbnew.PCB_VIA(board)
            v.SetPosition(pos)   # via-in-pad, dead center
            v.SetDrill(pcbnew.FromMM(0.3)); v.SetWidth(pcbnew.FromMM(0.5))
            v.SetNet(gnd_net); board.Add(v)
            added += 1
    count = 0
    for ymm in range(6, H-4, stitch_step):
        for xmm in range(6, W-4, stitch_step):
            gx, gy = r.mm2g(xmm, ymm)
            ok = all(0<=gx+ex<r.nx and 0<=gy+ey<r.ny and
                     all(r.owner[L,gx+ex,gy+ey]==0 for L in (0,1,2))
                     for ex in (-1,0,1) for ey in (-1,0,1))
            if not ok: continue
            v = pcbnew.PCB_VIA(board)
            v.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(xmm), pcbnew.FromMM(ymm)))
            v.SetDrill(pcbnew.FromMM(0.3)); v.SetWidth(pcbnew.FromMM(0.5))
            v.SetNet(gnd_net); board.Add(v)
            for L in (0,1,2):
                for ex in (-1,0,1):
                    for ey in (-1,0,1):
                        r.owner[L,gx+ex,gy+ey] = gnd
            count += 1
    filler = pcbnew.ZONE_FILLER(board); filler.Fill(board.Zones())
    board.BuildConnectivity()
    un = board.GetConnectivity().GetUnconnectedCount(True)
    pcbnew.SaveBoard(pcb, board)
    return added, count, un

if __name__ == '__main__':
    import sys as s2
    a, c, u = finalize(s2.argv[1])
    print(f'{a} via-in-pad, {c} stitch, unconnected={u}')
