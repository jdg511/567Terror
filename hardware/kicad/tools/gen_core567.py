"""LM567 CORE sheet — the heart. No LFIL/OFIL caps (DNP pads only) = the
voice. Timing: OTA emulates RT (continuous FREQ via Iabc) + CD4052 selects
one of 4 timing caps for the 0.2Hz..6kHz range. Q node: original R16 100k
pull-up, stray C made explicit."""
import sys; sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
from schgen import Sheet, make_symbol, register_custom, connect, verify_netlist

G = {'VA','V567','VREF','GND','V567_DRIVE','Q_OUT','FREQ_CV','FREQ_A','FREQ_B','LOCK_SENSE'}
s = Sheet('Glitchwave 567 — LM567 Core', paper='A3', rev='0.1')
EXPECT = {}
def C(part, mapping):
    connect(s, part, mapping, globals_=G)
    for pin, net in mapping.items():
        if net is None:
            EXPECT.setdefault('_FLOATING', set()).add(f'{part.ref}.{pin}')
        else:
            EXPECT.setdefault(net, set()).add(f'{part.ref}.{pin}')

lm567 = make_symbol('LM567', 'U',
    pins=[('3','IN','L',0,'input'), ('4','VCC','T',3,'power_in'),
          ('5','RT','L',2,'passive'), ('6','CT','L',3,'passive'),
          ('2','LFIL','R',2,'passive'), ('1','OFIL','R',3,'passive'),
          ('8','OUT','R',0,'open_collector'), ('7','GND','B',3,'power_in')],
    body_w=17.78)
LM = register_custom(s, lm567)

# ---- the chip --------------------------------------------------------------
s.text('LM567 TONE DECODER — THE HEART. NO CAPS on LFIL/OFIL (DNP pads only): the loop chatters at audio rate. THIS IS THE VOICE. DO NOT FIT.', (25.4, 25.4), 1.8)
u5 = s.add(LM, 'U5', 'LM567 (SO-8)', (78.74, 55.88))
C(u5, {3:'IN567', 4:'V567', 5:'RT_SQ', 6:'CT_NODE', 2:'LFIL_PAD', 1:'OFIL_PAD', 8:'Q_OC', 7:'GND'})
# input coupling (original C3 220n into ~20k internal impedance = 36Hz HP)
C(s.add('Device:C','C3','220n',(40.64,55.88),angle=90), {1:'V567_DRIVE', 2:'IN567'})
C(s.add('Device:C','C40','100n',(96.52,30.48)), {1:'V567', 2:'GND'})
# DNP experiment pads (not fitted, not in BOM)
C(s.add('Device:C','C41','DNP-LFIL',(120.65,55.88), dnp=True), {1:'LFIL_PAD', 2:'GND'})
C(s.add('Device:C','C42','DNP-OFIL',(133.35,55.88), dnp=True), {1:'OFIL_PAD', 2:'GND'})
s.text('C41/C42: DO NOT FIT. Pads exist only for taming experiments.', (115.57, 78.74), 1.5)

# ---- Q output node (original R16 pull-up + explicit stray C) ---------------
s.text('Q open-collector node: R16 100k pull-up to V567 (weak = lopsided square, original behavior). C43 makes the sim stray-C explicit.', (154.94, 25.4), 1.8)
C(s.add('Device:R','R16','100k',(168.91,45.72)), {1:'V567', 2:'Q_OUT'})
C(s.add('Device:C','C43','220p',(181.61,63.5)), {1:'Q_OUT', 2:'GND'})
s.wire((168.91+0,0),(168.91,0))  # no-op cosmetic
C(s.add('Device:R','R40','0R',(196.85,45.72),angle=90), {1:'Q_OC', 2:'Q_OUT'})
# lock sense divider -> Pico ADC (Q swings to ~9V; 100k/47k -> 3.1V max)
C(s.add('Device:R','R41','100k',(214.63,45.72)), {1:'Q_OUT', 2:'LOCK_SENSE'})
C(s.add('Device:R','R42','47k',(214.63,63.5)), {1:'LOCK_SENSE', 2:'GND'})

# ---- timing: OTA as variable RT --------------------------------------------
s.text('FREQ: U7 OTA emulates the timing resistor. Pin5 square -> attenuator -> OTA -> current into CT node; Iabc (FREQ_CV) sweeps f0 over decades.', (25.4, 96.52), 1.8)
s.text('R38 100k parallel floor keeps the VCO alive at Iabc=0. Original was R6 3.6k + B10k pot with C4 220n (304-1148Hz).', (25.4, 100.33), 1.8)
# divider reference for OTA inputs: V567/2 (its own midpoint, not VA VREF)
C(s.add('Device:R','R43','47k',(33.02,116.84)), {1:'V567', 2:'V567_MID'})
C(s.add('Device:R','R44','47k',(33.02,134.62)), {1:'V567_MID', 2:'GND'})
C(s.add('Device:C','C44','1u',(45.72,134.62)), {1:'V567_MID', 2:'GND'})
# attenuate pin5 square into OTA linear range
C(s.add('Device:R','R45','220k',(58.42,116.84),angle=90), {1:'RT_SQ', 2:'OTA2_INP'})
C(s.add('Device:R','R46','680',(71.12,134.62)), {1:'OTA2_INP', 2:'V567_MID'})
C(s.add('Device:R','R47','680',(83.82,134.62)), {1:'OTA2_INM', 2:'V567_MID'})
C(s.add('Amplifier_Operational:LM13700','U7','LM13700',(106.68,116.84),unit=3),
  {3:'OTA2_INP', 4:'OTA2_INM', 5:'CT_NODE', 1:'FREQ_IABC', 2:None})
C(s.add('Device:R','R48','10k',(83.82,152.4),angle=90), {1:'FREQ_CV', 2:'FREQ_IABC'})
C(s.add('Device:R','R38','100k',(129.54,116.84),angle=90), {1:'RT_SQ', 2:'CT_NODE'})

# ---- CD4052 switched timing caps ------------------------------------------
s.text('CT range select: CD4052B (on V567 rail) picks 1 of 4 caps. 47n/1u/22u/470u x OTA sweep covers 0.2Hz-6kHz+ with overlap.', (154.94, 96.52), 1.8)
s.text('Address lines level-shifted 3.3V->V567 by Q2/Q3 (inverting - firmware flips bits).', (154.94, 100.33), 1.8)
u8 = s.add('Analog_Switch:CD4052B','U8','CD4052B',(190.5,127.0))
C(u8, {13:'CT_NODE', 12:'TC_A', 14:'TC_B', 15:'TC_C', 11:'TC_D',
       10:'FREQ_A_S', 9:'FREQ_B_S', 6:'GND', 16:'V567', 7:'GND', 8:'GND',
       3:None, 1:None, 5:None, 2:None, 4:None})
C(s.add('Device:C','C45','47n',(220.98,146.05)), {1:'TC_A', 2:'GND'})
C(s.add('Device:C','C46','1u',(233.68,146.05)), {1:'TC_B', 2:'GND'})
C(s.add('Device:C_Polarized','C47','22u',(246.38,146.05)), {1:'TC_C', 2:'GND'})
C(s.add('Device:C_Polarized','C48','470u',(259.08,146.05)), {1:'TC_D', 2:'GND'})
C(s.add('Device:C','C49','100n',(220.98,109.22)), {1:'V567', 2:'GND'})
# level shifters (inverting, open-collector style)
for i,(cvnet, outnet, qref, rb, rc, x) in enumerate([
        ('FREQ_A','FREQ_A_S','Q2','R49','R50',269.24),
        ('FREQ_B','FREQ_B_S','Q3','R51','R52',290.83)]):
    C(s.add('Device:R', rb, '10k', (x-10.16, 190.5), angle=90), {1: cvnet, 2: f'{qref}_B'})
    C(s.add('Transistor_BJT:MMBT3904','%s' % qref,'MMBT3904',(x,190.5)), {1:f'{qref}_B', 2:outnet, 3:'GND'})
    C(s.add('Device:R', rc, '47k', (x, 170.18)), {1:'V567', 2:outnet})

# ---- U7 power + parked units ----------------------------------------------
C(s.add('Amplifier_Operational:LM13700','U7','LM13700',(33.02,190.5),unit=5), {11:'VA', 6:'GND'})
C(s.add('Device:C','C50','100n',(48.26,190.5)), {1:'VA', 2:'GND'})
C(s.add('Amplifier_Operational:LM13700','U7','LM13700',(71.12,190.5),unit=1),
  {14:'VREF', 13:'VREF', 12:None, 16:'GND', 15:None})
C(s.add('Amplifier_Operational:LM13700','U7','LM13700',(96.52,190.5),unit=2), {10:None, 9:None})
C(s.add('Amplifier_Operational:LM13700','U7','LM13700',(114.3,190.5),unit=4), {7:None, 8:None})

s.save('/home/claude/work/hardware/kicad/glitchwave567/core567.kicad_sch')
ok, rep = verify_netlist('/home/claude/work/hardware/kicad/glitchwave567/core567.kicad_sch', EXPECT)
print('VERIFY:', ok); print(rep)
