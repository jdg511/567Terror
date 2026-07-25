"""POWER sheet — declarative style: every pin's net is stated once, then
machine-verified against the exported netlist."""
import sys; sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
from schgen import Sheet, make_symbol, register_custom, connect, verify_netlist

G = {'VA','V567','+5V','VREF','VDIRT','GND','STARVE_CV','VA_SENSE'}  # inter-sheet nets
s = Sheet('Glitchwave 567 — Power', paper='A3', rev='0.1')
EXPECT = {}
def C(part, mapping):
    connect(s, part, mapping, globals_=G)
    for pin, net in mapping.items():
        if net is None:
            EXPECT.setdefault('_FLOATING', set()).add(f'{part.ref}.{pin}')
            continue
        EXPECT.setdefault(net, set()).add(f'{part.ref}.{pin}')

mp1584 = make_symbol('MP1584EN', 'U',
    pins=[('2','VIN','L',0,'power_in'), ('7','EN','L',1,'input'),
          ('6','COMP','L',2,'input'), ('8','FREQ','L',3,'input'), ('5','FB','L',4,'input'),
          ('1','BST','R',0,'input'), ('3','SW','R',1,'output'),
          ('4','GND','B',3,'power_in')], body_w=20.32)
MP = register_custom(s, mp1584)

# ---- block: input + reverse protection + VA --------------------------------
s.text('9-18 V DC IN, 2.1mm CENTER-NEGATIVE (tip=GND, sleeve=+V) -> P-FET reverse protect -> ferrite -> VA (raw analog rail)', (25.4, 25.4), 1.8)
C(s.add('Connector:Barrel_Jack_Switch','J5','DC_2.1mm',(33.02,45.72)), {1:'GND', 2:'VIN_RAW', 3:None})
C(s.add('Transistor_FET:AO3401A','Q10','AO3401A',(71.12,45.72),angle=270), {3:'VIN_RAW', 2:'VPROT', 1:'QGATE'})
C(s.add('Device:R','R100','100k',(71.12,66.04)), {1:'QGATE', 2:'GND'})
C(s.add('Device:D_Zener','D100','BZT52C10',(83.82,66.04)), {1:'VPROT', 2:'QGATE'})   # K=1 to S, A=2 to gate
C(s.add('Device:FerriteBead','FB100','600R/3A',(96.52,45.72),angle=90), {1:'VPROT', 2:'VA'})
C(s.add('Device:C_Polarized','C100','220u/35V',(109.22,66.04)), {1:'VA', 2:'GND'})
C(s.add('Device:C','C101','100n',(121.92,66.04)), {1:'VA', 2:'GND'})

# ---- block: 78L09 -> V567 --------------------------------------------------
s.text('LM567 rail: 78L09 (chip abs max ~9-10V)', (152.4, 25.4), 1.8)
C(s.add('Regulator_Linear:L78L09_SOT89','U18','L78L09',(167.64,45.72)), {3:'VA', 1:'V567', 2:'GND'})
C(s.add('Device:C','C102','1u',(154.94,66.04)), {1:'VA', 2:'GND'})
C(s.add('Device:C','C103','1u',(180.34,66.04)), {1:'V567', 2:'GND'})

# ---- block: MP1584 buck -> +5V --------------------------------------------
s.text('5V buck (Pico VSYS + WS2812). VERIFY COMP/FREQ/FB values vs MP1584 datasheet at review pass', (33.02, 96.52), 1.8)
C(s.add(MP,'U19','MP1584EN',(63.5,127.0)), {2:'VA', 7:'BUCK_EN', 6:'BUCK_COMP', 8:'BUCK_FREQ', 5:'BUCK_FB', 1:'BUCK_BST', 3:'BUCK_SW', 4:'GND'})
C(s.add('Device:C','C104','10u/25V',(33.02,127.0)), {1:'VA', 2:'GND'})
C(s.add('Device:R','R101','100k',(33.02,144.78)), {1:'VA', 2:'BUCK_EN'})
C(s.add('Device:R','R102','51k',(43.18,157.48),angle=90), {1:'BUCK_COMP', 2:'BUCK_COMPC'})
C(s.add('Device:C','C105','3n3',(58.42,157.48),angle=90), {1:'BUCK_COMPC', 2:'GND'})
C(s.add('Device:R','R103','100k',(73.66,157.48),angle=90), {1:'BUCK_FREQ', 2:'GND'})
C(s.add('Device:C','C106','100n',(96.52,114.3),angle=90), {1:'BUCK_BST', 2:'BUCK_SW'})
C(s.add('Device:D_Schottky','D101','SS34',(96.52,144.78)), {1:'BUCK_SW', 2:'GND'})    # K=1 to SW, A=2 to GND
C(s.add('Device:L','L100','22uH/3A',(111.76,127.0),angle=270), {1:'BUCK_SW', 2:'+5V'})
C(s.add('Device:C','C107','22u',(127.0,144.78)), {1:'+5V', 2:'GND'})
C(s.add('Device:C','C108','22u',(139.7,144.78)), {1:'+5V', 2:'GND'})
C(s.add('Device:R','R104','40k2 1%',(152.4,127.0)), {1:'+5V', 2:'BUCK_FB'})
C(s.add('Device:R','R105','7k68 1%',(152.4,144.78)), {1:'BUCK_FB', 2:'GND'})

# ---- block: VREF = VA/2 buffer --------------------------------------------
s.text('VREF = VA/2 mid-rail, buffered (R14/R15/C10 of the original schematic)', (218.44, 25.4), 1.8)
C(s.add('Device:R','R106','47k',(228.6,45.72)), {1:'VA', 2:'VREF_RAW'})
C(s.add('Device:R','R107','47k',(228.6,63.5)), {1:'VREF_RAW', 2:'GND'})
C(s.add('Device:C_Polarized','C109','47u',(241.3,63.5)), {1:'VREF_RAW', 2:'GND'})
C(s.add('Amplifier_Operational:TL074','U6','TL074',(266.7,45.72),unit=1), {3:'VREF_RAW', 2:'VREF', 1:'VREF'})

# ---- block: starve servo -> VDIRT -----------------------------------------
s.text('STARVE servo: Pico PWM (STARVE_CV 0-3.3V) x5.45 -> commands VDIRT; 6V2 zener floor = 5.0V at emitter', (218.44, 96.52), 1.8)
s.text('VDIRT feeds ONLY the Bazz Fuss rail. VA_SENSE -> Pico ADC so firmware scales the law to the supply', (218.44, 100.33), 1.8)
C(s.add('Amplifier_Operational:TL074','U6','TL074',(238.76,127.0),unit=2), {5:'STARVE_CV', 6:'SRV_FB', 7:'SRV_OUT'})
C(s.add('Device:R','R108','33k',(228.6,144.78)), {1:'SRV_FB', 2:'GND'})
C(s.add('Device:R','R109','147k',(248.92,144.78)), {1:'SRV_FB', 2:'SRV_OUT'})
C(s.add('Device:D','D102','1N4148W',(264.16,127.0),angle=180), {2:'SRV_OUT', 1:'QBASE'})  # A=2 from servo, K=1 to base
C(s.add('Device:R','R110','10k',(279.4,109.22)), {1:'VA', 2:'FLOOR62'})
C(s.add('Device:D_Zener','D103','BZT52C6V2',(279.4,144.78)), {1:'FLOOR62', 2:'GND'})       # K=1 at ref node
C(s.add('Device:D','D104','1N4148W',(294.64,127.0),angle=180), {2:'FLOOR62', 1:'QBASE'})   # A=2 from ref, K=1 to base
C(s.add('Device:R','R111','10k',(309.88,144.78)), {1:'QBASE', 2:'GND'})
C(s.add('Transistor_BJT:BCP56','Q11','BCP56',(325.12,127.0)), {1:'QBASE', 2:'VA', 4:'VA', 3:'VDIRT'})
C(s.add('Device:C_Polarized','C110','47u',(340.36,144.78)), {1:'VDIRT', 2:'GND'})

# ---- block: VA sense, TL074 power, spare units ----------------------------
C(s.add('Device:R','R112','100k',(33.02,190.5)), {1:'VA', 2:'VA_SENSE'})
C(s.add('Device:R','R113','22k',(33.02,208.28)), {1:'VA_SENSE', 2:'GND'})
s.text('VA_SENSE: 18V -> 3.25V', (25.4, 223.52), 1.5)
C(s.add('Amplifier_Operational:TL074','U6','TL074',(71.12,198.12),unit=5), {4:'VA', 11:'GND'})
C(s.add('Device:C','C111','100n',(88.9,198.12)), {1:'VA', 2:'GND'})
# spare op-amp units parked as VREF followers
C(s.add('Amplifier_Operational:TL074','U6','TL074',(116.84,190.5),unit=3), {10:'VREF', 9:'U6C_FB', 8:'U6C_FB'})
C(s.add('Amplifier_Operational:TL074','U6','TL074',(116.84,215.9),unit=4), {12:'VREF', 13:'U6D_FB', 14:'U6D_FB'})

# ---- PWR_FLAGs -------------------------------------------------------------
for i,(net,x) in enumerate([('VIN_RAW',180.34),('VPROT',195.58),('VA',210.82),('+5V',226.06),('VDIRT',241.3),('GND',256.54)]):
    f = s.add('power:PWR_FLAG', f'#FLG10{i}', 'PWR_FLAG', (x, 205.74))
    connect(s, f, {1: net}, globals_=G)
    EXPECT.setdefault(net, set())  # flags excluded from verify

s.save('/home/claude/work/hardware/kicad/glitchwave567/power.kicad_sch')
ok, rep = verify_netlist('/home/claude/work/hardware/kicad/glitchwave567/power.kicad_sch', EXPECT)
print('VERIFY:', ok); print(rep)
