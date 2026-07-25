"""ENV + CV sheet — analog Mu-Tron-ballistics envelope follower -> Pico ADC,
2x CV input conditioning (clamped, slewed) -> ADC, CV out PWM -> 0-5V buffer."""
import sys; sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
from schgen import Sheet, connect, verify_netlist

G = {'VA','VREF','GND','+5V','3V3','DRY_CLEAN','ENV_ADC','CV1_TIP','CV2_TIP',
     'CV1_ADC','CV2_ADC','CVOUT_PWM','CVOUT_TIP'}
s = Sheet('Glitchwave 567 — Envelope & CV', paper='A3', rev='0.1')
EXPECT = {}
def C(part, mapping):
    connect(s, part, mapping, globals_=G)
    for pin, net in mapping.items():
        if net is None: EXPECT.setdefault('_FLOATING', set()).add(f'{part.ref}.{pin}')
        else: EXPECT.setdefault(net, set()).add(f'{part.ref}.{pin}')

# ---- envelope follower (analog ballistics stay analog) ---------------------
s.text('ENV FOLLOWER: precision full-wave rectifier + Mu-Tron ballistics (atk ~4ms, rel ~150ms) -> buffered, clamped -> Pico ADC.', (25.4, 25.4), 1.8)
s.text('Routing/assignment of the envelope is FIRMWARE (matches plugin mod system); only the FEEL is analog.', (25.4, 29.21), 1.8)
C(s.add('Device:R','R130','100k',(33.02,45.72),angle=90), {1:'DRY_CLEAN', 2:'FW_IN'})
C(s.add('Amplifier_Operational:TL074','U17','TL074',(58.42,45.72),unit=1), {2:'FW_IN', 3:'VREF', 1:'FW_A'})
C(s.add('Device:D','D80','1N4148W',(73.66,38.1),angle=90), {2:'FW_A', 1:'FW_N'})
C(s.add('Device:D','D81','1N4148W',(73.66,55.88),angle=90), {1:'FW_A', 2:'FW_IN'})
C(s.add('Device:R','R131','100k',(86.36,38.1)), {1:'FW_N', 2:'FW_IN'})
C(s.add('Device:R','R132','100k',(99.06,45.72),angle=90), {1:'FW_N', 2:'FW_SUM'})
C(s.add('Device:R','R133','200k',(119.38,45.72),angle=90), {1:'FW_IN', 2:'FW_SUM'})
C(s.add('Amplifier_Operational:TL074','U17','TL074',(137.16,45.72),unit=2), {6:'FW_SUM', 5:'VREF', 7:'RECT_OUT'})
C(s.add('Device:R','R134','100k',(149.86,63.5)), {1:'RECT_OUT', 2:'FW_SUM'})
# ballistics: attack 4.7k into 1u, release 150k bleed
C(s.add('Device:D','D82','1N4148W',(165.1,45.72),angle=90), {2:'RECT_OUT', 1:'BAL_A'})
C(s.add('Device:R','R135','3.9k',(177.8,45.72),angle=90), {1:'BAL_A', 2:'ENV_C'})
C(s.add('Device:C','C80','1u',(190.5,63.5)), {1:'ENV_C', 2:'VREF'})
C(s.add('Device:R','R136','150k',(203.2,63.5)), {1:'ENV_C', 2:'VREF'})
C(s.add('Amplifier_Operational:TL074','U17','TL074',(228.6,45.72),unit=3), {10:'ENV_C', 9:'ENV_B', 8:'ENV_B'})
# level shift VREF-referenced env down to 0..3.3 for ADC: divider + clamp
C(s.add('Device:R','R137','100k',(243.84,45.72),angle=90), {1:'ENV_B', 2:'ENV_DIV'})
C(s.add('Device:R','R138','47k',(256.54,63.5)), {1:'ENV_DIV', 2:'GND'})
C(s.add('Device:D_Schottky','D83','1N5819W (clamp 3V3)',(269.24,38.1),angle=90), {2:'ENV_DIV', 1:'3V3'})
C(s.add('Device:R','R139','1k',(281.94,45.72),angle=90), {1:'ENV_DIV', 2:'ENV_ADC'})
s.text('VERIFY at review: rectifier is VREF-referenced; divider maps env swing into 0-3.3V; BAT54 hard clamp.', (215.9, 78.74), 1.5)

# ---- CV inputs x2 ----------------------------------------------------------
s.text('CV IN x2: 100k series, /3 divider (accepts up to ~10V), BAT54S rail clamps, RC slew -> ADC1/ADC2 direct (fast).', (25.4, 96.52), 1.8)
for i,(tip, adc, y) in enumerate([('CV1_TIP','CV1_ADC',116.84), ('CV2_TIP','CV2_ADC',146.05)]):
    p = 130+10*i
    C(s.add('Device:R',f'R14{i*3}','100k',(33.02,y),angle=90), {1:tip, 2:f'CVD{i}'})
    C(s.add('Device:R',f'R14{i*3+1}','47k',(45.72,y+17.78)), {1:f'CVD{i}', 2:'GND'})
    C(s.add('Device:D_Schottky',f'D8{4+i}','1N5819W',(58.42,y-7.62),angle=90), {2:f'CVD{i}', 1:'3V3'})
    C(s.add('Device:R',f'R14{i*3+2}','10k',(71.12,y),angle=90), {1:f'CVD{i}', 2:adc})
    C(s.add('Device:C',f'C8{1+i}','10n',(83.82,y+17.78)), {1:adc, 2:'GND'})

# ---- CV out ----------------------------------------------------------------
s.text('CV OUT: Pico PWM -> 2-pole RC -> x1.52 (0-3.3 -> 0-5V) on the +5V-capable VA op-amp -> 1k -> jack.', (152.4, 96.52), 1.8)
C(s.add('Device:R','R146','10k',(165.1,116.84),angle=90), {1:'CVOUT_PWM', 2:'CVF1'})
C(s.add('Device:C','C83','100n',(177.8,134.62)), {1:'CVF1', 2:'GND'})
C(s.add('Device:R','R147','10k',(190.5,116.84),angle=90), {1:'CVF1', 2:'CVF2'})
C(s.add('Device:C','C84','100n',(203.2,134.62)), {1:'CVF2', 2:'GND'})
C(s.add('Amplifier_Operational:TL074','U17','TL074',(228.6,116.84),unit=4), {12:'CVF2', 13:'CVO_FB', 14:'CVO_OUT'})
C(s.add('Device:R','R148','10k',(241.3,134.62)), {1:'CVO_OUT', 2:'CVO_FB'})
C(s.add('Device:R','R149','20k',(254.0,134.62)), {1:'CVO_FB', 2:'GND'})
C(s.add('Device:R','R150','1k',(269.24,116.84),angle=90), {1:'CVO_OUT', 2:'CVOUT_TIP'})

# ---- power -----------------------------------------------------------------
C(s.add('Amplifier_Operational:TL074','U17','TL074',(287.02,45.72),unit=5), {4:'VA', 11:'GND'})
C(s.add('Device:C','C85','100n',(299.72,45.72)), {1:'VA', 2:'GND'})

s.save('/home/claude/work/hardware/kicad/glitchwave567/env_cv.kicad_sch')
ok, rep = verify_netlist('/home/claude/work/hardware/kicad/glitchwave567/env_cv.kicad_sch', EXPECT)
print('VERIFY:', ok); print(rep)
