"""OUTPUT sheet — original C8/R12 DC block, HP60 voicing, +3dB bell,
switchable +6dB / JFET Fetzer / -3-6 asym diode ladder, gate+VOL VCA and
buffered-bypass VCA (10ms crossfade in firmware), out buffer + R13 470R."""
import sys; sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
from schgen import Sheet, connect, verify_netlist

G = {'VA','VREF','GND','FILT_OUT','DRY_CLEAN','GATE_VOL_CV','BYPASS_CV','OUT_TIP'}
s = Sheet('Glitchwave 567 — Output Stage', paper='A3', rev='0.1')
EXPECT = {}
def C(part, mapping):
    connect(s, part, mapping, globals_=G)
    for pin, net in mapping.items():
        if net is None: EXPECT.setdefault('_FLOATING', set()).add(f'{part.ref}.{pin}')
        else: EXPECT.setdefault(net, set()).add(f'{part.ref}.{pin}')

# ---- original output DC block + HP60 voicing -------------------------------
s.text('Voicing (v0.24 order): C8/R12 DC block (7.2Hz, original) -> 12dB/oct HP@60 -> +3dB bell@800 Q0.5 -> +6dB switch.', (25.4, 25.4), 1.8)
C(s.add('Device:C','C8','220n',(33.02,45.72),angle=90), {1:'FILT_OUT', 2:'ODC'})
C(s.add('Device:R','R12','100k',(45.72,63.5)), {1:'ODC', 2:'VREF'})
# Sallen-Key HP 60Hz Q0.707: C=C=100n, R_fb 39k, R_q 18k
C(s.add('Device:C','C70','100n',(58.42,45.72),angle=90), {1:'ODC', 2:'H60_M'})
C(s.add('Device:C','C71','100n',(71.12,45.72),angle=90), {1:'H60_M', 2:'H60_P'})
C(s.add('Device:R','R100A','39k',(64.77,63.5)), {1:'H60_M', 2:'H60_OUT'})
C(s.add('Device:R','R101A','18k',(78.74,63.5)), {1:'H60_P', 2:'VREF'})
C(s.add('Amplifier_Operational:TL074','U14','TL074',(96.52,45.72),unit=1), {3:'H60_P', 2:'H60_OUT', 1:'H60_OUT'})
# +3dB bell @800Hz Q=0.5 exact: non-inv amp, Zf = 3.6k||56n, Zg = 4.3k + 47n to VREF
# (review-verified: +3.04dB @ 790Hz, Q=0.500; all-real-pole bell needs no gyrator)
s.text('Bell +3dB@800 Q0.5: G=1+Zf/Zg, Zf=3.6k||56n, Zg=4.3k+47n. Review-verified 790Hz/+3.04dB.', (121.92, 63.5), 1.5)
C(s.add('Amplifier_Operational:TL074','U14','TL074',(190.5,45.72),unit=2), {5:'H60_OUT', 6:'BELL_FB', 7:'BELL_OUT'})
C(s.add('Device:R','R102A','3.6k',(203.2,63.5)), {1:'BELL_OUT', 2:'BELL_FB'})
C(s.add('Device:C','C72A','56n',(215.9,63.5)), {1:'BELL_OUT', 2:'BELL_FB'})
C(s.add('Device:R','R103A','4.3k',(160.02,63.5)), {1:'BELL_FB', 2:'GYR_B'})
C(s.add('Device:C','C72','47n',(147.32,63.5)), {1:'GYR_B', 2:'VREF'})
# +6dB switchable: non-inv x2; SW1 shorts Rf -> unity
C(s.add('Amplifier_Operational:TL074','U14','TL074',(241.3,45.72),unit=3), {10:'BELL_OUT', 9:'B6_FB', 8:'B6_OUT'})
C(s.add('Device:R','R106A','10k',(254.0,63.5)), {1:'B6_OUT', 2:'B6_FB'})
C(s.add('Device:R','R107A','10k',(266.7,63.5)), {1:'B6_FB', 2:'VREF'})
C(s.add('Switch:SW_SPDT','SW1','+6dB (short=OFF)',(281.94,45.72)), {2:'B6_OUT', 1:'B6_FB', 3:None})
s.text('SW1 closed to B6_FB = unity (+6dB OFF, ship default per v0.23 sw ON? ships x2). Slide MSK-12C02.', (259.08, 78.74), 1.5)

# ---- JFET Fetzer stage (ships ON) ------------------------------------------
s.text('JFET J201 Fetzer-Valve stage (ships IN): big 2nd harmonic, asym squish. SW2 selects JFET path or bypass.', (25.4, 96.52), 1.8)
C(s.add('Device:C','C73','100n',(33.02,116.84),angle=90), {1:'B6_OUT', 2:'JF_G'})
C(s.add('Device:R','R108A','1M',(45.72,134.62)), {1:'JF_G', 2:'GND'})
C(s.add('Device:Q_NJFET_DGS','QJ1','MMBFJ201',(63.5,116.84)), {1:'JF_D', 2:'JF_G', 3:'JF_S'})
C(s.add('Device:R','R109A','847R (Fetzer Rs)',(76.2,134.62)), {1:'JF_S', 2:'GND'})
C(s.add('Device:R','R110A','15k',(76.2,99.06)), {1:'VA', 2:'JF_D'})
C(s.add('Device:C','C74','1u',(96.52,116.84),angle=90), {1:'JF_D', 2:'JF_AC'})
C(s.add('Device:R','R111A','220k',(109.22,134.62)), {1:'JF_AC', 2:'VREF'})
C(s.add('Switch:SW_SPDT','SW2','JFET in/out',(127.0,116.84)), {1:'JF_AC', 3:'B6_OUT', 2:'POST_JFET'})

# ---- -3/-6 asym diode ladder (ships OFF) -----------------------------------
s.text('-3/-6 asym ladder (v0.24, ships OUT): pos half soft 3-step, neg half 2-step. VALUES ARE STARTING POINTS - the sim curves are', (152.4, 96.52), 1.8)
s.text('ground truth; final R values tuned against plugin A/B at breadboard/review. SW3 selects ladder or bypass.', (152.4, 100.33), 1.8)
C(s.add('Amplifier_Operational:TL074','U14','TL074',(165.1,121.92),unit=4), {13:'LAD_IN', 12:'VREF', 14:'LAD_OUT'})
C(s.add('Device:R','R112A','10k',(152.4,116.84),angle=90), {1:'POST_JFET', 2:'LAD_IN'})
C(s.add('Device:R','R113A','10k',(177.8,139.7)), {1:'LAD_OUT', 2:'LAD_IN'})
# positive half (-3 ladder): 1/2/3 series diodes with graduated series R
C(s.add('Device:D','D70','1N4148W',(190.5,116.84),angle=90), {2:'LAD_IN', 1:'LP1'})
C(s.add('Device:R','R114A','3.3k',(190.5,139.7)), {1:'LP1', 2:'LAD_OUT'})
C(s.add('Device:D','D71','1N4148W',(203.2,116.84),angle=90), {2:'LAD_IN', 1:'LP2A'})
C(s.add('Device:D','D72','1N4148W',(203.2,134.62),angle=90), {2:'LP2A', 1:'LP2'})
C(s.add('Device:R','R115A','6.8k',(203.2,157.48)), {1:'LP2', 2:'LAD_OUT'})
# negative half (-6 ladder): opposite polarity
C(s.add('Device:D','D73','1N4148W',(215.9,116.84),angle=270), {1:'LAD_IN', 2:'LN1'})
C(s.add('Device:R','R116A','4.7k',(215.9,139.7)), {1:'LN1', 2:'LAD_OUT'})
C(s.add('Device:D','D74','1N4148W',(228.6,116.84),angle=270), {1:'LAD_IN', 2:'LN2A'})
C(s.add('Device:D','D75','1N4148W',(228.6,134.62),angle=270), {1:'LN2A', 2:'LN2'})
C(s.add('Device:R','R117A','10k',(228.6,157.48)), {1:'LN2', 2:'LAD_OUT'})
C(s.add('Switch:SW_SPDT','SW3','ladder in/out',(251.46,116.84)), {1:'LAD_OUT', 3:'POST_JFET', 2:'POST_LAD'})

# ---- gate/VOL VCA + buffered-bypass VCA + out buffer -----------------------
s.text('U15A = gate x VOL VCA (one VCA, product CV from Pico). U15B = clean-bypass VCA (DRY_CLEAN). Currents sum -> U16A -> buffer -> R13 470R -> OUT.', (25.4, 170.18), 1.8)
C(s.add('Device:R','R118A','220k',(33.02,190.5),angle=90), {1:'POST_LAD', 2:'GOTA_P'})
C(s.add('Device:R','R119A','680',(45.72,208.28)), {1:'GOTA_P', 2:'VREF'})
C(s.add('Device:R','R120A','680',(58.42,208.28)), {1:'GOTA_M', 2:'VREF'})
C(s.add('Amplifier_Operational:LM13700','U15','LM13700',(81.28,190.5),unit=3),
  {3:'GOTA_P', 4:'GOTA_M', 5:'OUT_SUM', 1:'GV_IABC', 2:None})
C(s.add('Device:R','R121A','10k',(58.42,228.6),angle=90), {1:'GATE_VOL_CV', 2:'GV_IABC'})
C(s.add('Device:R','R122A','220k',(114.3,190.5),angle=90), {1:'DRY_CLEAN', 2:'BOTA_P'})
C(s.add('Device:R','R123A','680',(127.0,208.28)), {1:'BOTA_P', 2:'VREF'})
C(s.add('Device:R','R124A','680',(139.7,208.28)), {1:'BOTA_M', 2:'VREF'})
C(s.add('Amplifier_Operational:LM13700','U15','LM13700',(162.56,190.5),unit=1),
  {14:'BOTA_P', 13:'BOTA_M', 12:'OUT_SUM', 16:'BYP_IABC', 15:None})
C(s.add('Device:R','R125A','10k',(139.7,228.6),angle=90), {1:'BYPASS_CV', 2:'BYP_IABC'})
C(s.add('Amplifier_Operational:TL074','U16','TL074',(203.2,190.5),unit=1), {2:'OUT_SUM', 3:'VREF', 1:'PRE_OUT'})
C(s.add('Device:R','R126A','220k',(215.9,208.28)), {1:'PRE_OUT', 2:'OUT_SUM'})
# final DC block + R13 470R to jack (original)
C(s.add('Amplifier_Operational:TL074','U16','TL074',(241.3,190.5),unit=2), {5:'PRE_OUT', 6:'OB_FB', 7:'OB_FB'})
C(s.add('Device:C_Polarized','C75','10u',(259.08,190.5),angle=90), {1:'OB_FB', 2:'OUT_AC'})
C(s.add('Device:R','R127A','100k',(271.78,208.28)), {1:'OUT_AC', 2:'GND'})
C(s.add('Device:R','R13','470R',(284.48,190.5),angle=90), {1:'OUT_AC', 2:'OUT_TIP'})

s.text('NOTE: U15A+U15B outputs tied = OTA CURRENT summing (intentional; ERC output-output waiver).', (25.4, 174.0), 1.5)
# ---- power + parked --------------------------------------------------------
C(s.add('Amplifier_Operational:TL074','U14','TL074',(307.34,45.72),unit=5), {4:'VA', 11:'GND'})
C(s.add('Device:C','C76','100n',(320.04,45.72)), {1:'VA', 2:'GND'})
C(s.add('Amplifier_Operational:LM13700','U15','LM13700',(307.34,83.82),unit=5), {11:'VA', 6:'GND'})
C(s.add('Device:C','C77','100n',(320.04,83.82)), {1:'VA', 2:'GND'})
C(s.add('Amplifier_Operational:TL074','U16','TL074',(307.34,116.84),unit=5), {4:'VA', 11:'GND'})
C(s.add('Device:C','C78','100n',(320.04,116.84)), {1:'VA', 2:'GND'})
C(s.add('Amplifier_Operational:LM13700','U15','LM13700',(307.34,146.05),unit=2), {10:'GND', 9:None})
C(s.add('Amplifier_Operational:LM13700','U15','LM13700',(307.34,165.1),unit=4), {7:'GND', 8:None})
C(s.add('Amplifier_Operational:TL074','U16','TL074',(307.34,190.5),unit=3), {10:'VREF', 9:'U16C_FB', 8:'U16C_FB'})
C(s.add('Amplifier_Operational:TL074','U16','TL074',(307.34,215.9),unit=4), {12:'VREF', 13:'U16D_FB', 14:'U16D_FB'})

s.save('/home/claude/work/hardware/kicad/glitchwave567/output.kicad_sch')
ok, rep = verify_netlist('/home/claude/work/hardware/kicad/glitchwave567/output.kicad_sch', EXPECT)
print('VERIFY:', ok); print(rep)
