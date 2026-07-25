"""pcbgen — programmatic KiCad PCB authoring via pcbnew API.
Netlist-driven: footprints instantiated per component, pads bound to nets,
key parts placed from a table, passives auto-placed near their owners."""
import pcbnew, re, os, math, collections

STOCK = '/usr/share/kicad/footprints'
CUSTOM = '/home/claude/work/hardware/kicad'   # Glitchwave.pretty lives here

def parse_netlist(path):
    t = open(path).read()
    comps = {}
    for ref, body in re.findall(r'\(comp \(ref "([^"]+)"\)(.*?)(?=\(comp \(ref|\(libparts)', t, re.S):
        if ref.startswith('#'): continue
        fp = re.search(r'\(footprint "([^"]*)"\)', body)
        val = re.search(r'\(value "([^"]*)"\)', body)
        dnp = '(dnp' in body
        comps[ref] = {'fp': fp.group(1) if fp else '', 'val': val.group(1) if val else '', 'dnp': dnp}
    padnets = collections.defaultdict(dict)   # ref -> {pin: net}
    nets = set()
    for name, body in re.findall(r'\(net \(code "\d+"\) \(name "([^"]+)"\)(.*?)(?=\(net \(code|\Z)', t, re.S):
        nets.add(name)
        for r, p in re.findall(r'\(ref "([^"]+)"\) \(pin "([^"]+)"\)', body):
            padnets[r][p] = name
    return comps, padnets, nets

def load_fp(fpid):
    lib, name = fpid.split(':')
    path = f'{CUSTOM}/{lib}.pretty' if lib == 'Glitchwave' else f'{STOCK}/{lib}.pretty'
    fp = pcbnew.FootprintLoad(path, name)
    if fp is None: raise KeyError(fpid)
    return fp

class Board:
    def __init__(self, out_path):
        self.b = pcbnew.CreateEmptyBoard()
        self.out_path = out_path
        self.netmap = {}
        self.fps = {}
        self.occupied = []   # (x0,y0,x1,y1) bounding boxes in mm

    def make_nets(self, netnames):
        for n in sorted(netnames):
            ni = pcbnew.NETINFO_ITEM(self.b, n)
            self.b.Add(ni)
            self.netmap[n] = ni

    def add_part(self, ref, fpid, val, padnet, x, y, rot=0, flip=False):
        fp = load_fp(fpid)
        fp.SetReference(ref); fp.SetValue(val)
        self.b.Add(fp)
        fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
        if flip: fp.Flip(fp.GetPosition(), False)
        fp.SetOrientationDegrees(rot)
        for pad in fp.Pads():
            pn = pad.GetNumber()
            if pn in padnet:
                pad.SetNet(self.netmap[padnet[pn]])
        self.fps[ref] = fp
        self.occupied.append(self._pad_bbox(fp))
        return fp

    @staticmethod
    def _pad_bbox(fp, pad_margin=0.5):
        xs, ys = [], []
        for p in fp.Pads():
            pos = p.GetPosition(); sz = p.GetSize()
            xs += [pcbnew.ToMM(pos.x) - pcbnew.ToMM(sz.x)/2, pcbnew.ToMM(pos.x) + pcbnew.ToMM(sz.x)/2]
            ys += [pcbnew.ToMM(pos.y) - pcbnew.ToMM(sz.y)/2, pcbnew.ToMM(pos.y) + pcbnew.ToMM(sz.y)/2]
        if not xs:
            bb = fp.GetBoundingBox()
            return (pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop()),
                    pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom()))
        return (min(xs)-pad_margin, min(ys)-pad_margin, max(xs)+pad_margin, max(ys)+pad_margin)

    def collides(self, x0,y0,x1,y1, margin=0.4):
        for (a0,b0,a1,b1) in self.occupied:
            if x0-margin < a1 and x1+margin > a0 and y0-margin < b1 and y1+margin > b0:
                return True
        return False

    def auto_place(self, ref, fpid, val, padnet, anchor_xy, board_w, board_h, keep=8.0):
        """spiral search around anchor for a free spot"""
        fp = load_fp(fpid)
        bx0, by0, bx1, by1 = self._pad_bbox(fp, 0.3)
        w, h = bx1 - bx0, by1 - by0
        ax, ay = anchor_xy
        best = None
        for r in [x*0.8 for x in range(1, 70)]:
            steps = max(8, int(r*4))
            for i in range(steps):
                th = 2*math.pi*i/steps
                x = ax + r*math.cos(th); y = ay + r*math.sin(th)
                if not (2+w/2 < x < board_w-2-w/2 and 2+h/2 < y < board_h-2-h/2):
                    continue
                if not self.collides(x-w/2, y-h/2, x+w/2, y+h/2, margin=1.05):
                    best = (x, y); break
            if best: break
        if best is None:
            for yy in range(4, int(board_h)-4):
                for xx in range(4, int(board_w)-4, 2):
                    if not self.collides(xx-w/2, yy-h/2, xx+w/2, yy+h/2):
                        best = (xx, yy); break
                if best: break
        if best is None: best = (board_w/2, board_h/2)
        return self.add_part(ref, fpid, val, padnet, best[0], best[1])

    def outline_notched(self, W, H, notch=11.0):
        """rectangle with 4 corner notches (for enclosure screw bosses)"""
        pts = [(notch,0),(W-notch,0),(W-notch,notch),(W,notch),(W,H-notch),
               (W-notch,H-notch),(W-notch,H),(notch,H),(notch,H-notch),(0,H-notch),
               (0,notch),(notch,notch),(notch,0)]
        for (x0,y0),(x1,y1) in zip(pts, pts[1:]):
            seg = pcbnew.PCB_SHAPE(self.b)
            seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
            seg.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(x0), pcbnew.FromMM(y0)))
            seg.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(x1), pcbnew.FromMM(y1)))
            seg.SetLayer(pcbnew.Edge_Cuts)
            seg.SetWidth(pcbnew.FromMM(0.1))
            self.b.Add(seg)

    def gnd_zones(self, W, H):
        for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
            z = pcbnew.ZONE(self.b)
            z.SetLayer(layer)
            z.SetNet(self.netmap['GND'])
            pts = [(1,1),(W-1,1),(W-1,H-1),(1,H-1)]
            chain = pcbnew.SHAPE_LINE_CHAIN()
            for (x,y) in pts:
                chain.Append(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
            chain.SetClosed(True)
            z.Outline().AddOutline(chain)
            z.SetLocalClearance(pcbnew.FromMM(0.3))
            z.SetMinThickness(pcbnew.FromMM(0.25))
            z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
            z.SetIsFilled(False)
            self.b.Add(z)

    def text(self, s, x, y, layer=pcbnew.F_SilkS, size=1.2):
        t = pcbnew.PCB_TEXT(self.b)
        t.SetText(s)
        t.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
        t.SetLayer(layer)
        t.SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(size), pcbnew.FromMM(size)))
        self.b.Add(t)

    def save(self):
        pcbnew.SaveBoard(self.out_path, self.b)
