"""JACKS sheet — IN/OUT 6.35mm (PJ-603A) + 3x 3.5mm CV (PJ-3410, switched
tips grounded when unplugged). DC jack lives on the POWER sheet."""
import sys; sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
from schgen import Sheet, connect, verify_netlist

G = {'GND','IN_TIP','OUT_TIP','CV1_TIP','CV2_TIP','CVOUT_TIP'}
s = Sheet('Glitchwave 567 — Jacks', paper='A4', rev='0.1')
EXPECT = {}
def C(part, mapping):
    connect(s, part, mapping, globals_=G)
    for pin, net in mapping.items():
        if net is None: EXPECT.setdefault('_FLOATING', set()).add(f'{part.ref}.{pin}')
        else: EXPECT.setdefault(net, set()).add(f'{part.ref}.{pin}')

s.text('All jacks PCB-mounted, poking through enclosure walls. IN/OUT = HOOYA PJ-603A (M12, axis 5.0mm above PCB).', (20.32, 20.32), 1.8)
s.text('CV = XKB PJ-3410 (M7.7, axis 4.5mm); switched tip grounds the input when unplugged (clean 0V, no floating ADC).', (20.32, 24.13), 1.8)
C(s.add('Connector_Audio:AudioJack2_SwitchT','J1','IN 6.35mm PJ-603A',(33.02,45.72),
        footprint='Glitchwave:PJ-603A'), {'T':'IN_TIP', 'TN':'GND', 'S':'GND'})
C(s.add('Connector_Audio:AudioJack2','J2','OUT 6.35mm PJ-603A',(83.82,45.72),
        footprint='Glitchwave:PJ-603A'), {'T':'OUT_TIP', 'S':'GND'})
C(s.add('Connector_Audio:AudioJack2_SwitchT','J3','CV1 IN 3.5mm PJ-3410',(33.02,86.36),
        footprint='Glitchwave:PJ-3410'), {'T':'CV1_TIP', 'TN':'GND', 'S':'GND'})
C(s.add('Connector_Audio:AudioJack2_SwitchT','J4','CV2 IN 3.5mm PJ-3410',(83.82,86.36),
        footprint='Glitchwave:PJ-3410'), {'T':'CV2_TIP', 'TN':'GND', 'S':'GND'})
C(s.add('Connector_Audio:AudioJack2','J6','CV OUT 3.5mm PJ-3410',(134.62,86.36),
        footprint='Glitchwave:PJ-3410'), {'T':'CVOUT_TIP', 'S':'GND'})
s.text('J5 (DC 2.1mm ctr-neg) is on the POWER sheet.', (20.32, 116.84), 1.5)

s.save('/home/claude/work/hardware/kicad/glitchwave567/jacks.kicad_sch')
ok, rep = verify_netlist('/home/claude/work/hardware/kicad/glitchwave567/jacks.kicad_sch', EXPECT)
print('VERIFY:', ok); print(rep)
