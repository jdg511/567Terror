"""ROOT sheet — hierarchy container for the 8 flat sheets (all inter-sheet
connectivity is via global labels; sheets have no hierarchical pins)."""
import sys, uuid as uuidlib; sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
from schgen import Sheet
from kiutils.items.schitems import HierarchicalSheet, HierarchicalSheetInstance
from kiutils.items.common import Property, Position, Effects, Font, Stroke

s = Sheet('Glitchwave 567 — Main Board (root)', paper='A3', rev='0.1')
s.text('GLITCHWAVE 567 — digitally-controlled analog. All inter-sheet nets are GLOBAL LABELS; sheets below are containers.', (25.4, 25.4), 2.2)
s.text('Signal: jacks -> input_dirt -> core567 -> mix_svf -> output -> jacks. Control: mcu PWM CVs everywhere. Power: power.', (25.4, 30.48), 1.8)
sheets = [('power','power.kicad_sch'), ('input_dirt','input_dirt.kicad_sch'),
          ('core567','core567.kicad_sch'), ('mix_svf','mix_svf.kicad_sch'),
          ('output','output.kicad_sch'), ('env_cv','env_cv.kicad_sch'),
          ('mcu','mcu.kicad_sch'), ('jacks','jacks.kicad_sch')]
insts = []
for i,(name,fn) in enumerate(sheets):
    x = 33.02 + (i%4)*88.9; y = 45.72 + (i//4)*63.5
    hs = HierarchicalSheet()
    hs.position = Position(X=x, Y=y)
    hs.width, hs.height = 63.5, 38.1
    hs.stroke = Stroke(width=0.1524, type='solid')
    hs.uuid = str(uuidlib.uuid4())
    hs.sheetName.value = name
    hs.sheetName.position = Position(X=x, Y=y-0.8, angle=0)
    hs.sheetName.effects = Effects(font=Font(width=1.6, height=1.6))
    hs.fileName.value = fn
    hs.fileName.position = Position(X=x, Y=y+38.9, angle=0)
    hs.fileName.effects = Effects(font=Font(width=1.27, height=1.27))
    s.sch.sheets.append(hs)
    si = HierarchicalSheetInstance(instancePath=f'/{hs.uuid}', page=str(i+2))
    insts.append(si)
try:
    s.sch.sheetInstances = insts
except Exception as e:
    print('sheetInstances:', e)
s.save('/home/claude/work/hardware/kicad/glitchwave567/glitchwave567.kicad_sch')
print('root written')
