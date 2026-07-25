"""schgen — programmatic KiCad schematic builder for the Glitchwave 567 project.
Authors KiCad 7-compatible .kicad_sch files using kiutils. Connections are made
pin-to-pin with real wires where local, and via global labels for inter-stage /
inter-sheet nets (reviewable, ERC-able, netlist-correct)."""
import copy, math, uuid as uuidlib, os
from kiutils.schematic import Schematic
from kiutils.symbol import SymbolLib
from kiutils.items.schitems import (SchematicSymbol, SymbolInstance, GlobalLabel,
    Connection, Junction, LocalLabel, Text, NoConnect, SymbolProjectInstance, SymbolProjectPath)
from kiutils.items.common import Position, Property, Effects, Font

SYM_DIR = '/usr/share/kicad/symbols'
_libcache = {}

def _lib(name):
    if name not in _libcache:
        _libcache[name] = SymbolLib.from_file(f'{SYM_DIR}/{name}.kicad_sym')
    return _libcache[name]

def get_symbol(libid):
    lib, entry = libid.split(':')
    for s in _lib(lib).symbols:
        if s.entryName == entry:
            sym = copy.deepcopy(s)
            sym.libraryNickname = lib
            return sym
    raise KeyError(libid)

def _resolve_parent(sym, libname):
    """flatten 'extends' inheritance (e.g. L78L09 extends L78L05)"""
    if sym.extends:
        parent = get_symbol(f'{libname}:{sym.extends}')
        parent = _resolve_parent(parent, libname)
        merged = copy.deepcopy(parent)
        oldname = merged.entryName
        merged.entryName = sym.entryName
        merged.libraryNickname = sym.libraryNickname
        merged.extends = None
        for u in merged.units:
            if u.entryName == oldname:
                u.entryName = sym.entryName
            elif u.entryName and u.entryName.startswith(oldname + '_'):
                u.entryName = sym.entryName + u.entryName[len(oldname):]
        # child properties override parent's
        pk = {p.key: p for p in merged.properties}
        for p in sym.properties: pk[p.key] = p
        merged.properties = list(pk.values())
        return merged
    return sym

class Part:
    def __init__(self, sheet, sym, ref, at, angle, mirror, unit):
        self.sheet, self.sym, self.ref = sheet, sym, ref
        self.x, self.y, self.angle, self.mirror, self.unit = at[0], at[1], angle, mirror, unit
    def _unit_pins(self):
        pins = []
        for u in self.sym.units:
            if u.unitId in (0, self.unit):
                pins.extend(u.pins)
        return pins
    def pin(self, number):
        for p in self._unit_pins():
            if str(p.number) == str(number):
                px, py = p.position.X, p.position.Y
                # pin position given at pin base; electrical connect point IS position
                if self.mirror == 'x': py = -py
                if self.mirror == 'y': px = -px
                a = math.radians(self.angle)
                rx = px*math.cos(a) - py*math.sin(a)
                ry = px*math.sin(a) + py*math.cos(a)
                return (round(self.x + rx, 2), round(self.y - ry, 2))
        raise KeyError(f'{self.ref} pin {number}')
    def pins(self):
        return [str(p.number) for p in self._unit_pins()]

GRID = 1.27
def snap(v): return round(round(v / GRID) * GRID, 2)

class Sheet:
    def __init__(self, title, paper='A3', rev='0.1'):
        self.sch = Schematic.create_new()
        pass  # keep create_new default version (20211014, KiCad 6 format - readable by 7/10)
        self.sch.generator = 'schgen'
        self.sch.paper.paperSize = paper
        from kiutils.items.common import TitleBlock
        tb = TitleBlock()
        tb.title = title; tb.company = 'Illicit Apothecary'; tb.revision = rev
        self.sch.titleBlock = tb
        self._embedded = {}
        self._refcount = {}
    def add(self, libid, ref, value, at, angle=0, mirror=None, unit=1, footprint='', fields=None, value_offset=None):
        if libid not in self._embedded:
            sym = _resolve_parent(get_symbol(libid), libid.split(':')[0])
            self._embedded[libid] = sym
            self.sch.libSymbols.append(sym)
        sym = self._embedded[libid]
        inst = SchematicSymbol()
        inst.libraryNickname, inst.entryName = libid.split(':')
        inst.position = Position(X=at[0], Y=at[1], angle=angle)
        inst.unit = unit
        inst.inBom, inst.onBoard, inst.dnp = True, True, False
        inst.mirror = mirror
        inst.uuid = str(uuidlib.uuid4())
        def prop(key, val, pid, hide=False, dy=0):
            p = Property(key=key, value=val, id=pid,
                         position=Position(X=at[0], Y=at[1]+dy, angle=0))
            p.effects = Effects(font=Font(width=1.27, height=1.27), hide=hide)
            return p
        vdy = value_offset if value_offset is not None else 2.54
        inst.properties = [
            prop('Reference', ref, 0, dy=-2.54),
            prop('Value', value, 1, dy=vdy),
            prop('Footprint', footprint, 2, hide=True),
            prop('Datasheet', '', 3, hide=True),
        ]
        if fields:
            for i,(k,v) in enumerate(fields.items()):
                inst.properties.append(prop(k, v, 4+i, hide=True))
        pi = SymbolProjectInstance(name='glitchwave567')
        pp = SymbolProjectPath(sheetInstancePath='/', reference=ref, unit=unit)
        pi.paths = [pp]
        inst.instances = [pi]
        self.sch.schematicSymbols.append(inst)
        return Part(self, sym, ref, at, angle, mirror, unit)
    def wire(self, a, b, elbow='h'):
        """a,b = (x,y). elbow='h': horizontal first; 'v': vertical first; 'd': direct"""
        pts = [a, b] if (a[0]==b[0] or a[1]==b[1] or elbow=='d') else \
              ([a,(b[0],a[1]),b] if elbow=='h' else [a,(a[0],b[1]),b])
        for p,q in zip(pts, pts[1:]):
            if p == q: continue
            c = Connection()
            c.points = [Position(X=p[0], Y=p[1]), Position(X=q[0], Y=q[1])]
            c.uuid = str(uuidlib.uuid4())
            self.sch.graphicalItems.append(c)
    def junction(self, at):
        j = Junction(); j.position = Position(X=at[0], Y=at[1]); j.uuid = str(uuidlib.uuid4())
        self.sch.junctions.append(j)
    def glabel(self, text, at, angle=0, shape='bidirectional'):
        g = GlobalLabel(text=text, shape=shape)
        g.position = Position(X=at[0], Y=at[1], angle=angle)
        g.effects = Effects(font=Font(width=1.27, height=1.27))
        g.uuid = str(uuidlib.uuid4())
        self.sch.globalLabels.append(g)
    def label(self, text, at, angle=0):
        l = LocalLabel(text=text)
        l.position = Position(X=at[0], Y=at[1], angle=angle)
        l.effects = Effects(font=Font(width=1.27, height=1.27))
        l.uuid = str(uuidlib.uuid4())
        self.sch.labels.append(l)
    def text(self, s, at, size=1.7):
        t = Text(text=s)
        t.position = Position(X=at[0], Y=at[1], angle=0)
        t.effects = Effects(font=Font(width=size, height=size))
        t.uuid = str(uuidlib.uuid4())
        self.sch.texts.append(t)
    def nc(self, at):
        n = NoConnect(); n.position = Position(X=at[0], Y=at[1]); n.uuid = str(uuidlib.uuid4())
        self.sch.noConnects.append(n)
    def save(self, path):
        self.sch.filePath = path
        self.sch.to_file(path)

# ---------------------------------------------------------------------------
# custom symbol builder (for parts missing from the stock libraries)
# pins: list of (number, name, side, offset_index, electrical)
#   side: 'L'|'R'|'T'|'B'; offsets place pins every 2.54mm down/across the body
# ---------------------------------------------------------------------------
from kiutils.symbol import Symbol, SymbolPin
from kiutils.items.syitems import SyRect
from kiutils.items.common import Stroke, Fill, ColorRGBA

def make_symbol(entry_name, ref_prefix, pins, body_w=17.78, body_h=None, pin_len=5.08):
    npl = max([p[3] for p in pins if p[2]=='L'] + [-1]) + 1
    npr = max([p[3] for p in pins if p[2]=='R'] + [-1]) + 1
    rows = max(npl, npr)
    if body_h is None:
        body_h = (rows + 1) * 2.54
    hw, hh = body_w/2, body_h/2
    top = Symbol()
    top.entryName = entry_name
    top.pinNamesOffset = 0.508
    top.inBom = top.onBoard = True
    def sprop(key, val, pid, hide=False, dy=0):
        p = Property(key=key, value=val, id=pid, position=Position(X=0, Y=hh+2.54+dy, angle=0))
        p.effects = Effects(font=Font(width=1.27, height=1.27), hide=hide)
        return p
    top.properties = [sprop('Reference', ref_prefix, 0), sprop('Value', entry_name, 1, dy=-(body_h+5.08)),
                      sprop('Footprint', '', 2, hide=True), sprop('Datasheet', '', 3, hide=True)]
    unit = Symbol()
    unit.entryName = entry_name
    unit.unitId, unit.styleId = 1, 1
    rect = SyRect()
    rect.start = Position(X=-hw, Y=hh)
    rect.end = Position(X=hw, Y=-hh)
    rect.stroke = Stroke(width=0.254, type='default')
    rect.fill = Fill(type='background')
    unit.graphicItems = [rect]
    for num, name, side, idx, etype in pins:
        pin = SymbolPin()
        pin.electricalType = etype
        pin.graphicalStyle = 'line'
        pin.length = pin_len
        pin.name, pin.number = str(name), str(num)
        pin.nameEffects = Effects(font=Font(width=1.27, height=1.27))
        pin.numberEffects = Effects(font=Font(width=1.27, height=1.27))
        if side == 'L':
            y = hh - 2.54*(idx+1)
            pin.position = Position(X=-(hw+pin_len), Y=y, angle=0)
        elif side == 'R':
            y = hh - 2.54*(idx+1)
            pin.position = Position(X=hw+pin_len, Y=y, angle=180)
        elif side == 'T':
            x = -hw + 2.54*(idx+1)
            pin.position = Position(X=x, Y=hh+pin_len, angle=270)
        else:
            x = -hw + 2.54*(idx+1)
            pin.position = Position(X=x, Y=-(hh+pin_len), angle=90)
        unit.pins.append(pin)
    top.units = [unit]
    return top

def register_custom(sheet, sym, libname='Glitchwave'):
    sym.libraryNickname = libname
    libid = f'{libname}:{sym.entryName}'
    sheet._embedded[libid] = sym
    sheet.sch.libSymbols.append(sym)
    return libid

# ---------------------------------------------------------------------------
# declarative connection: stub wire + label exactly at each pin
# ---------------------------------------------------------------------------
def _pin_raw(part, number):
    for p in part._unit_pins():
        if str(p.number) == str(number):
            return p
    raise KeyError(number)

class _PartConnect:
    pass

def pin_outward_angle(part, number):
    p = _pin_raw(part, number)
    a = p.position.angle  # direction pin points TOWARD body; outward = a+180... in lib space
    # apply mirror
    if part.mirror == 'y': a = (180 - a) % 360
    if part.mirror == 'x': a = (-a) % 360
    return (a + part.angle) % 360

def connect(sheet, part, mapping, globals_=(), stub=2.54):
    for pinno, net in mapping.items():
        if net is None:
            sheet.nc(part.pin(pinno)); continue
        pt = part.pin(pinno)
        ang = pin_outward_angle(part, pinno)
        dx = {0: -1, 180: 1, 90: 0, 270: 0}[ang] * stub * -1
        dy = {90: 1, 270: -1, 0: 0, 180: 0}[ang] * stub * -1
        # outward direction: pin angle points from connect point toward body,
        # so outward is the opposite
        ox = pt[0] - ( {0:1,180:-1}.get(ang,0) ) * -stub
        end = (round(pt[0] + {0:-stub,180:stub}.get(ang,0),2),
               round(pt[1] + {90:stub,270:-stub}.get(ang,0),2))
        sheet.wire(pt, end)
        lang = {0:180, 180:0, 90:270, 270:90}[ang]
        if net in globals_:
            sheet.glabel(net, end, angle=lang)
        else:
            sheet.label(net, end, angle=lang)

import subprocess, re as _re
def verify_netlist(sch_path, expected, allow_extra_unconnected=True):
    """expected: dict net -> set of 'REF.PIN'. Returns (ok, report)."""
    out = '/tmp/_verify.net'
    r = subprocess.run(['kicad-cli','sch','export','netlist',sch_path,'-o',out],
                       capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out):
        return False, 'netlist export failed: ' + r.stdout + r.stderr
    t = open(out).read(); os.remove(out)
    got = {}
    for name, body in _re.findall(r'\(net \(code "\d+"\) \(name "([^"]+)"\)(.*?)(?=\(net \(code|\Z)', t, _re.S):
        name = name.lstrip('/')
        nodes = {f'{ref}.{pin}' for ref,pin in _re.findall(r'\(ref "([^"]+)"\) \(pin "([^"]+)"\)', body)}
        if name.startswith('unconnected-') or name.startswith('Net-'):
            got.setdefault('_FLOATING', set()).update(nodes)
        else:
            got[name] = nodes
    lines, ok = [], True
    for net, nodes in sorted(expected.items()):
        g = got.get(net, set())
        # power flags are invisible helpers
        g2 = {n for n in g if not n.startswith('#')}
        if g2 != nodes:
            ok = False
            missing, extra = nodes - g2, g2 - nodes
            lines.append(f'NET {net}: missing={sorted(missing)} extra={sorted(extra)}')
    for net in got:
        if net not in expected and net not in ('_FLOATING',):
            ok = False; lines.append(f'UNEXPECTED NET {net}: {sorted(got[net])}')
    floating = {n for n in got.get('_FLOATING', set()) if not n.startswith('#')}
    exp_float = expected.get('_FLOATING', set())
    if floating - exp_float:
        ok = False; lines.append(f'FLOATING PINS: {sorted(floating - exp_float)}')
    return ok, ('\n'.join(lines) if lines else 'NETLIST MATCHES EXPECTED')
