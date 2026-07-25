"""INPUT + DIRT sheet — faithful front end (R1/R2/C1/R3 network, buffer,
40Hz 24dB/oct voicing HP, +15dB trim, x214.6 gain w/ 15.4Hz shelf) plus the
always-on Bazz Fuss dirt on the starvable VDIRT rail, gain by LM13700 VCA."""
import sys; sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
from schgen import Sheet, connect, verify_netlist

G = {'VA','VREF','VDIRT','GND','IN_TIP','DRY_CLEAN','V567_DRIVE','DIRT_OUT','DIRT_CV'}
s = Sheet('Glitchwave 567 — Input & Dirt', paper='A3', rev='0.1')
EXPECT = {}
def C(part, mapping):
    connect(s, part, mapping, globals_=G)
    for pin, net in mapping.items():
        if net is None:
            EXPECT.setdefault('_FLOATING', set()).add(f'{part.ref}.{pin}')
        else:
            EXPECT.setdefault(net, set()).add(f'{part.ref}.{pin}')

# ---- input network + buffer U1.1 (original R1/R2/C1/R3) --------------------
s.text('INPUT (faithful to original): R1 1k series, R2 2.2M load, C1 220n block, R3 2.2M bias to VREF -> U1A buffer', (25.4, 25.4), 1.8)
C(s.add('Device:R','R1','1k',(33.02,45.72),angle=90), {1:'IN_TIP', 2:'IN_A'})
C(s.add('Device:R','R2','2.2M',(45.72,63.5)), {1:'IN_A', 2:'GND'})
C(s.add('Device:C','C1','220n',(58.42,45.72),angle=90), {1:'IN_A', 2:'IN_B'})
C(s.add('Device:R','R3','2.2M',(71.12,63.5)), {1:'IN_B', 2:'VREF'})
C(s.add('Amplifier_Operational:TL074','U1','TL074',(88.9,45.72),unit=1), {3:'IN_B', 2:'BUF_OUT', 1:'BUF_OUT'})

# ---- v0.10 voicing: 24 dB/oct Butterworth HP @ 40 Hz (2x Sallen-Key) -------
s.text('Voicing low-cut 24dB/oct @40Hz (v0.10): two Sallen-Key HP, Q=0.54 then Q=1.31. DRY tap AFTER voicing (as in sim).', (127.0, 25.4), 1.8)
# SK-HP 1: C=C=100n, R to VREF at +in = 36k (Q leg), R feedback = 43k
C(s.add('Device:C','C20','100n',(127.0,45.72),angle=90), {1:'BUF_OUT', 2:'HP1_M'})
C(s.add('Device:C','C21','100n',(139.7,45.72),angle=90), {1:'HP1_M', 2:'HP1_P'})
C(s.add('Device:R','R20','43k',(133.35,63.5)), {1:'HP1_M', 2:'HP1_OUT'})
C(s.add('Device:R','R21','36k',(147.32,63.5)), {1:'HP1_P', 2:'VREF'})
C(s.add('Amplifier_Operational:TL074','U1','TL074',(165.1,45.72),unit=2), {5:'HP1_P', 6:'HP1_OUT', 7:'HP1_OUT'})
# SK-HP 2: C=C=100n, R_q = 15k, R_fb = 100k
C(s.add('Device:C','C22','100n',(190.5,45.72),angle=90), {1:'HP1_OUT', 2:'HP2_M'})
C(s.add('Device:C','C23','100n',(203.2,45.72),angle=90), {1:'HP2_M', 2:'HP2_P'})
C(s.add('Device:R','R22','100k',(196.85,63.5)), {1:'HP2_M', 2:'DRY_CLEAN'})
C(s.add('Device:R','R23','15k',(210.82,63.5)), {1:'HP2_P', 2:'VREF'})
C(s.add('Amplifier_Operational:TL074','U1','TL074',(228.6,45.72),unit=3), {10:'HP2_P', 9:'DRY_CLEAN', 8:'DRY_CLEAN'})
s.text('DRY_CLEAN = clean tap -> dirt VCA + bypass path', (223.52, 63.5), 1.5)

# ---- +15 dB fixed trim (v0.8, was the DRIVE pot) ---------------------------
s.text('+15dB fixed trim (v0.8) then x214.6 gain stage (1+470k/2.2k, C2 4.7u shelf -> unity below 15.4Hz). Clips rails -> square into 567.', (25.4, 96.52), 1.8)
C(s.add('Amplifier_Operational:TL074','U2','TL074',(38.1,116.84),unit=1), {3:'DRY_CLEAN', 2:'TRIM_FB', 1:'TRIM_OUT'})
C(s.add('Device:R','R24','47k',(50.8,134.62)), {1:'TRIM_OUT', 2:'TRIM_FB'})
C(s.add('Device:R','R25','10k',(63.5,134.62)), {1:'TRIM_FB', 2:'VREF'})
# ---- x214.6 gain stage U1.4 (original R4/R5/C2) ----------------------------
C(s.add('Amplifier_Operational:TL074','U1','TL074',(101.6,116.84),unit=4), {12:'TRIM_OUT', 13:'GAIN_FB', 14:'V567_DRIVE'})
C(s.add('Device:R','R4','470k',(114.3,134.62)), {1:'V567_DRIVE', 2:'GAIN_FB'})
C(s.add('Device:R','R5','2.2k',(127.0,134.62)), {1:'GAIN_FB', 2:'GAIN_SH'})
C(s.add('Device:C_Polarized','C2','4.7u',(140.97,134.62)), {1:'GAIN_SH', 2:'VREF'})

# ---- dirt gain VCA (U3 = LM13700 OTA-A) ------------------------------------
s.text('DIRT GAIN VCA (LM13700, replaces digipot - 5V digipot cannot pass 9V-referenced audio at 18V supply).', (165.1, 96.52), 1.8)
s.text('DIRT_CV (Pico PWM, filtered) -> Iabc. x1..x300 sweep = VCA attenuator + fixed x300 stage.', (165.1, 100.33), 1.8)
# attenuate DRY_CLEAN ~1:100 into OTA linear range, re-biased at VREF
C(s.add('Device:R','R30','220k',(165.1,116.84),angle=90), {1:'DRY_CLEAN', 2:'OTA_INP'})
C(s.add('Device:R','R31','680',(177.8,134.62)), {1:'OTA_INP', 2:'VREF'})
C(s.add('Device:R','R32','680',(190.5,134.62)), {1:'OTA_INM', 2:'VREF'})
C(s.add('Amplifier_Operational:LM13700','U3','LM13700',(215.9,116.84),unit=3),
  {3:'OTA_INP', 4:'OTA_INM', 5:'OTA_OUT', 1:'DIRT_IABC', 2:None})
C(s.add('Device:R','R33','10k',(190.5,152.4),angle=90), {1:'DIRT_CV', 2:'DIRT_IABC'})
# I-to-V + fixed gain: OTA out into inverting stage around VREF
C(s.add('Amplifier_Operational:TL074','U2','TL074',(254.0,116.84),unit=2), {6:'OTA_OUT', 5:'VREF', 7:'DIRT_PRE'})
C(s.add('Device:R','R34','330k',(266.7,134.62)), {1:'DIRT_PRE', 2:'OTA_OUT'})

# ---- Bazz Fuss on VDIRT rail (Q1 darlington + D1, original topology) -------
s.text('BAZZ FUSS (always-on, dry path): darlington + diode, collector rail = VDIRT (starve target). 9kHz voicing LP, DC-blocked out.', (25.4, 170.18), 1.8)
C(s.add('Device:C','C30','100n',(38.1,187.96),angle=90), {1:'DIRT_PRE', 2:'BF_BASE'})
C(s.add('Device:Q_NPN_Darlington_BCE','Q1','MMBTA13',(63.5,187.96)), {1:'BF_BASE', 2:'BF_COL', 3:'GND'})
C(s.add('Device:D','D1','1N4148W',(50.8,205.74),angle=90), {1:'BF_BASE', 2:'BF_COL'})   # K=1 base, A=2 collector
C(s.add('Device:R','R35','10k',(76.2,170.18)), {1:'VDIRT', 2:'BF_COL'})
C(s.add('Device:C','C31','100n',(88.9,187.96),angle=90), {1:'BF_COL', 2:'BF_AC'})
# 9 kHz voicing LP + re-bias to VREF
C(s.add('Device:R','R36','4.7k',(101.6,187.96),angle=90), {1:'BF_AC', 2:'DIRT_OUT'})
C(s.add('Device:C','C32','3.9n',(114.3,205.74)), {1:'DIRT_OUT', 2:'GND'})
C(s.add('Device:R','R37','1M',(127.0,205.74)), {1:'DIRT_OUT', 2:'VREF'})

# ---- chip power + decoupling ----------------------------------------------
C(s.add('Amplifier_Operational:TL074','U1','TL074',(203.2,187.96),unit=5), {4:'VA', 11:'GND'})
C(s.add('Device:C','C33','100n',(215.9,187.96)), {1:'VA', 2:'GND'})
C(s.add('Amplifier_Operational:TL074','U2','TL074',(234.95,187.96),unit=5), {4:'VA', 11:'GND'})
C(s.add('Device:C','C34','100n',(247.65,187.96)), {1:'VA', 2:'GND'})
C(s.add('Amplifier_Operational:LM13700','U3','LM13700',(266.7,187.96),unit=5), {11:'VA', 6:'GND'})
C(s.add('Device:C','C35','100n',(281.94,187.96)), {1:'VA', 2:'GND'})
# unused LM13700 half (OTA-B) + both darlington buffers: parked
C(s.add('Amplifier_Operational:LM13700','U3','LM13700',(302.26,116.84),unit=1),
  {14:'VREF', 13:'VREF', 12:None, 16:'GND', 15:None})
C(s.add('Amplifier_Operational:LM13700','U3','LM13700',(302.26,146.05),unit=2), {10:None, 9:None})
C(s.add('Amplifier_Operational:LM13700','U3','LM13700',(302.26,165.1),unit=4), {7:None, 8:None})
# spare U2 units parked as followers
C(s.add('Amplifier_Operational:TL074','U2','TL074',(302.26,187.96),unit=3), {10:'VREF', 9:'U2C_FB', 8:'U2C_FB'})
C(s.add('Amplifier_Operational:TL074','U2','TL074',(302.26,213.36),unit=4), {12:'VREF', 13:'U2D_FB', 14:'U2D_FB'})

s.save('/home/claude/work/hardware/kicad/glitchwave567/input_dirt.kicad_sch')
ok, rep = verify_netlist('/home/claude/work/hardware/kicad/glitchwave567/input_dirt.kicad_sch', EXPECT)
print('VERIFY:', ok); print(rep)
