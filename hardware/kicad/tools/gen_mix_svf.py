"""MIX + SVF sheet — dual-VCA crossfade mixer (OTA current-summing) and the
LM13700 state-variable filter with VCA'd Q and CD4051 mode select
(Off/LP/BP/HP/Notch, exactly the sim's 5 modes)."""
import sys; sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
from schgen import Sheet, connect, verify_netlist

G = {'VA','VREF','GND','Q_OUT','DIRT_OUT','MIX_WET_CV','MIX_DRY_CV',
     'SVF_F_CV','SVF_Q_CV','FILT_OUT','FMODE_A','FMODE_B','FMODE_C'}
s = Sheet('Glitchwave 567 — Mix & SVF Filter', paper='A3', rev='0.1')
EXPECT = {}
def C(part, mapping):
    connect(s, part, mapping, globals_=G)
    for pin, net in mapping.items():
        if net is None: EXPECT.setdefault('_FLOATING', set()).add(f'{part.ref}.{pin}')
        else: EXPECT.setdefault(net, set()).add(f'{part.ref}.{pin}')

s.text('NOTE: U10A+U10B outputs tied = OTA CURRENT summing (intentional; ERC output-output waiver).', (25.4, 33.02), 1.5)
# ---- MIX: dual VCA crossfade, currents sum into one I-to-V -----------------
s.text('MIX crossfade: WET (raw 567 square) and DRY (Bazz Fuss) each through a VCA; output CURRENTS sum at U9A virtual ground (free mixing).', (25.4, 25.4), 1.8)
s.text('Original mixer ratios (R9 1M wet / R10 100k dry / R11 100k fb) are reproduced by the input attenuators + firmware CV law.', (25.4, 29.21), 1.8)
# WET conditioning: AC couple the lopsided square, then heavy attenuation (wet needs 1/10 of dry per original)
C(s.add('Device:C','C60','220n',(33.02,45.72),angle=90), {1:'Q_OUT', 2:'WET_AC'})
C(s.add('Device:R','R60','1M',(45.72,63.5)), {1:'WET_AC', 2:'VREF'})
C(s.add('Device:R','R61','470k',(58.42,45.72),angle=90), {1:'WET_AC', 2:'WOTA_P'})
C(s.add('Device:R','R62','680',(71.12,63.5)), {1:'WOTA_P', 2:'VREF'})
C(s.add('Device:R','R63','680',(83.82,63.5)), {1:'WOTA_M', 2:'VREF'})
C(s.add('Amplifier_Operational:LM13700','U10','LM13700',(106.68,45.72),unit=3),
  {3:'WOTA_P', 4:'WOTA_M', 5:'MIX_SUM', 1:'WET_IABC', 2:None})
C(s.add('Device:R','R64','10k',(83.82,83.82),angle=90), {1:'MIX_WET_CV', 2:'WET_IABC'})
# DRY path into OTA-B
C(s.add('Device:R','R65','220k',(154.94,45.72),angle=90), {1:'DIRT_OUT', 2:'DOTA_P'})
C(s.add('Device:R','R66','680',(167.64,63.5)), {1:'DOTA_P', 2:'VREF'})
C(s.add('Device:R','R67','680',(180.34,63.5)), {1:'DOTA_M', 2:'VREF'})
C(s.add('Amplifier_Operational:LM13700','U10','LM13700',(203.2,45.72),unit=1),
  {14:'DOTA_P', 13:'DOTA_M', 12:'MIX_SUM', 16:'DRY_IABC', 15:None})
C(s.add('Device:R','R68','10k',(180.34,83.82),angle=90), {1:'MIX_DRY_CV', 2:'DRY_IABC'})
# I-to-V (inverting like the original mixer - polarity preserved vs sim)
C(s.add('Amplifier_Operational:TL074','U9','TL074',(241.3,45.72),unit=1), {2:'MIX_SUM', 3:'VREF', 1:'MIX_OUT'})
C(s.add('Device:R','R69','220k',(254.0,63.5)), {1:'MIX_OUT', 2:'MIX_SUM'})

# ---- SVF: 2 OTA integrators + on-chip buffers + VCA'd damping --------------
s.text('SVF (LM13700 datasheet topology): U9B input sum -> U11A int1 (BP) -> U11B int2 (LP); HP at the sum node. Cutoff = SVF_F_CV -> both Iabc.', (25.4, 106.68), 1.8)
s.text('Q: damping BP feedback passes through U12A VCA -> SVF_Q_CV sets resonance 0.25..8 continuously (synth-style).', (25.4, 110.49), 1.8)
# input sum stage: HP node = amp of (in - LP - BP*damp)
C(s.add('Device:R','R70','100k',(33.02,127.0),angle=90), {1:'MIX_OUT', 2:'SVF_SUM'})
C(s.add('Device:R','R71','100k',(45.72,146.05)), {1:'SVF_LP', 2:'SVF_SUM'})
C(s.add('Device:R','R72','100k',(58.42,146.05)), {1:'QFB_OUT', 2:'SVF_SUM'})
C(s.add('Amplifier_Operational:TL074','U9','TL074',(83.82,127.0),unit=2), {6:'SVF_SUM', 5:'VREF', 7:'SVF_HP'})
C(s.add('Device:R','R73','100k',(96.52,146.05)), {1:'SVF_HP', 2:'SVF_SUM'})
# integrator 1 -> BP (OTA + on-chip darlington buffer)
C(s.add('Device:R','R74','220k',(121.92,127.0),angle=90), {1:'SVF_HP', 2:'I1_P'})
C(s.add('Device:R','R75','680',(134.62,146.05)), {1:'I1_P', 2:'VREF'})
C(s.add('Amplifier_Operational:LM13700','U11','LM13700',(157.48,127.0),unit=3),
  {3:'I1_P', 4:'SVF_BP', 5:'I1_OUT', 1:'F_IABC1', 2:None})
C(s.add('Device:C','C61','1n',(170.18,146.05)), {1:'I1_OUT', 2:'VREF'})
C(s.add('Amplifier_Operational:LM13700','U11','LM13700',(185.42,127.0),unit=4), {7:'I1_OUT', 8:'SVF_BP'})
C(s.add('Device:R','R76','10k',(134.62,165.1),angle=90), {1:'SVF_F_CV', 2:'F_IABC1'})
# integrator 2 -> LP
C(s.add('Device:R','R77','220k',(203.2,127.0),angle=90), {1:'SVF_BP', 2:'I2_P'})
C(s.add('Device:R','R78','680',(215.9,146.05)), {1:'I2_P', 2:'VREF'})
C(s.add('Amplifier_Operational:LM13700','U11','LM13700',(238.76,127.0),unit=1),
  {14:'I2_P', 13:'SVF_LP', 12:'I2_OUT', 16:'F_IABC2', 15:None})
C(s.add('Device:C','C62','1n',(251.46,146.05)), {1:'I2_OUT', 2:'VREF'})
C(s.add('Amplifier_Operational:LM13700','U11','LM13700',(266.7,127.0),unit=2), {10:'I2_OUT', 9:'SVF_LP'})
C(s.add('Device:R','R79','10k',(215.9,165.1),angle=90), {1:'SVF_F_CV', 2:'F_IABC2'})
# Q damping VCA (U12 OTA-A): BP -> attenuate -> VCA -> QFB_OUT (I-to-V by R72 into sum node? needs voltage) 
C(s.add('Device:R','R80','220k',(33.02,190.5),angle=90), {1:'SVF_BP', 2:'QOTA_P'})
C(s.add('Device:R','R81','680',(45.72,208.28)), {1:'QOTA_P', 2:'VREF'})
C(s.add('Device:R','R82','680',(58.42,208.28)), {1:'QOTA_M', 2:'VREF'})
C(s.add('Amplifier_Operational:LM13700','U12','LM13700',(81.28,190.5),unit=3),
  {3:'QOTA_P', 4:'QOTA_M', 5:'QFB_I', 1:'Q_IABC', 2:None})
C(s.add('Device:R','R83','10k',(58.42,228.6),angle=90), {1:'SVF_Q_CV', 2:'Q_IABC'})
C(s.add('Amplifier_Operational:TL074','U9','TL074',(106.68,190.5),unit=3), {9:'QFB_I', 10:'VREF', 8:'QFB_OUT'})
C(s.add('Device:R','R84','100k',(119.38,208.28)), {1:'QFB_OUT', 2:'QFB_I'})

# ---- mode select: CD4051 8:1 (Off/LP/BP/HP/Notch) --------------------------
s.text('Mode select: CD4051B on VA rail. ch0=Off(bypass) ch1=LP ch2=BP ch3=HP ch4=Notch. Address 3.3V->VA level-shifted (inverting).', (154.94, 178.0), 1.8)
# notch = LP + HP sum
C(s.add('Device:R','R85','100k',(154.94,190.5),angle=90), {1:'SVF_LP', 2:'NOTCH_SUM'})
C(s.add('Device:R','R86','100k',(154.94,203.2),angle=90), {1:'SVF_HP', 2:'NOTCH_SUM'})
C(s.add('Amplifier_Operational:TL074','U9','TL074',(180.34,196.85),unit=4), {13:'NOTCH_SUM', 12:'VREF', 14:'SVF_NOTCH'})
C(s.add('Device:R','R87','50k',(193.04,215.9)), {1:'SVF_NOTCH', 2:'NOTCH_SUM'})
u13 = s.add('Analog_Switch:CD4051B','U13','CD4051B',(228.6,203.2))
C(u13, {3:'FILT_OUT', 13:'MIX_OUT', 14:'SVF_LP', 15:'SVF_BP', 12:'SVF_HP', 1:'SVF_NOTCH',
        5:None, 2:None, 4:None,
        11:'FMODE_A_S', 10:'FMODE_B_S', 9:'FMODE_C_S', 6:'GND', 16:'VA', 7:'GND', 8:'GND'})
C(s.add('Device:C','C63','100n',(243.84,184.15)), {1:'VA', 2:'GND'})
for cvnet, outnet, qref, rb, rc, x in [
        ('FMODE_A','FMODE_A_S','Q4','R88','R89',266.7),
        ('FMODE_B','FMODE_B_S','Q5','R90','R91',287.02),
        ('FMODE_C','FMODE_C_S','Q6','R92','R93',307.34)]:
    C(s.add('Device:R', rb, '10k', (x-10.16, 215.9), angle=90), {1: cvnet, 2: f'{qref}_B'})
    C(s.add('Transistor_BJT:MMBT3904', qref,'MMBT3904',(x,215.9)), {1:f'{qref}_B', 2:outnet, 3:'GND'})
    C(s.add('Device:R', rc, '47k', (x, 196.85)), {1:'VA', 2:outnet})

# ---- power/decoupling ------------------------------------------------------
C(s.add('Amplifier_Operational:TL074','U9','TL074',(287.02,45.72),unit=5), {4:'VA', 11:'GND'})
C(s.add('Device:C','C64','100n',(299.72,45.72)), {1:'VA', 2:'GND'})
C(s.add('Amplifier_Operational:LM13700','U10','LM13700',(287.02,83.82),unit=5), {11:'VA', 6:'GND'})
C(s.add('Device:C','C65','100n',(299.72,83.82)), {1:'VA', 2:'GND'})
C(s.add('Amplifier_Operational:LM13700','U11','LM13700',(287.02,116.84),unit=5), {11:'VA', 6:'GND'})
C(s.add('Device:C','C66','100n',(299.72,116.84)), {1:'VA', 2:'GND'})
C(s.add('Amplifier_Operational:LM13700','U12','LM13700',(287.02,146.05),unit=5), {11:'VA', 6:'GND'})
C(s.add('Device:C','C67','100n',(299.72,146.05)), {1:'VA', 2:'GND'})
# parked units: U10 buffers, U12 OTA-B + buffers
C(s.add('Amplifier_Operational:LM13700','U10','LM13700',(325.12,45.72),unit=2), {10:None, 9:None})
C(s.add('Amplifier_Operational:LM13700','U10','LM13700',(325.12,63.5),unit=4), {7:None, 8:None})
C(s.add('Amplifier_Operational:LM13700','U12','LM13700',(325.12,83.82),unit=1),
  {14:'VREF', 13:'VREF', 12:None, 16:'GND', 15:None})
C(s.add('Amplifier_Operational:LM13700','U12','LM13700',(325.12,107.95),unit=2), {10:None, 9:None})
C(s.add('Amplifier_Operational:LM13700','U12','LM13700',(325.12,127.0),unit=4), {7:None, 8:None})

s.save('/home/claude/work/hardware/kicad/glitchwave567/mix_svf.kicad_sch')
ok, rep = verify_netlist('/home/claude/work/hardware/kicad/glitchwave567/mix_svf.kicad_sch', EXPECT)
print('VERIFY:', ok); print(rep)
