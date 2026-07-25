"""MCU sheet — Raspberry Pi Pico module (custom symbol), 9x PWM CV RC
filters, 74HC4067 analog mux -> ADC0, CV1/CV2 direct to ADC1/2, gate trim
pots, WS2812 5V level buffer, stomp inputs, control-board header."""
import sys; sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
from schgen import Sheet, make_symbol, register_custom, connect, verify_netlist

G = {'+5V','3V3','GND','VA_SENSE','LOCK_SENSE','ENV_ADC','CV1_ADC','CV2_ADC',
     'FREQ_CV','DIRT_CV','MIX_WET_CV','MIX_DRY_CV','SVF_F_CV','SVF_Q_CV',
     'GATE_VOL_CV','BYPASS_CV','STARVE_CV','CVOUT_PWM',
     'FREQ_A','FREQ_B','FMODE_A','FMODE_B','FMODE_C'}
s = Sheet('Glitchwave 567 — MCU (Pico)', paper='A3', rev='0.1')
EXPECT = {}
def C(part, mapping):
    connect(s, part, mapping, globals_=G)
    for pin, net in mapping.items():
        if net is None: EXPECT.setdefault('_FLOATING', set()).add(f'{part.ref}.{pin}')
        else: EXPECT.setdefault(net, set()).add(f'{part.ref}.{pin}')

# ---- custom Pico module symbol --------------------------------------------
lp = [('1','GP0','L',0),('2','GP1','L',1),('3','GND','L',2),('4','GP2','L',3),
      ('5','GP3','L',4),('6','GP4','L',5),('7','GP5','L',6),('8','GND','L',7),
      ('9','GP6','L',8),('10','GP7','L',9),('11','GP8','L',10),('12','GP9','L',11),
      ('13','GND','L',12),('14','GP10','L',13),('15','GP11','L',14),('16','GP12','L',15),
      ('17','GP13','L',16),('18','GND','L',17),('19','GP14','L',18),('20','GP15','L',19)]
rp = [('40','VBUS','R',0),('39','VSYS','R',1),('38','GND','R',2),('37','3V3_EN','R',3),
      ('36','3V3_OUT','R',4),('35','ADC_VREF','R',5),('34','GP28_A2','R',6),('33','AGND','R',7),
      ('32','GP27_A1','R',8),('31','GP26_A0','R',9),('30','RUN','R',10),('29','GP22','R',11),
      ('28','GND','R',12),('27','GP21','R',13),('26','GP20','R',14),('25','GP19','R',15),
      ('24','GP18','R',16),('23','GND','R',17),('22','GP17','R',18),('21','GP16','R',19)]
pins = [(n,nm,sd,i,'power_in' if nm in ('GND','AGND','VBUS','VSYS') else
         ('power_out' if nm=='3V3_OUT' else 'bidirectional')) for n,nm,sd,i in lp+rp]
pico = make_symbol('RPi_Pico', 'U', pins, body_w=30.48)
PICO = register_custom(s, pico)

s.text('Raspberry Pi Pico module. GP0-8 PWM CVs (RC filtered), GP9 CVOUT_PWM (2-pole on ENV sheet), GP10-14 mux addresses,', (25.4, 22.86), 1.8)
s.text('GP15 WS2812, GP16-17 stomps, GP18-21 4067 ADC-mux address, GP22 spare, ADC0=muxed 16ch, ADC1/2 = CV1/CV2 direct.', (25.4, 26.67), 1.8)
u20 = s.add(PICO, 'U20', 'Raspberry Pi Pico', (76.2, 88.9))
C(u20, {1:'PWM_FREQ', 2:'PWM_DIRT', 3:'GND', 4:'PWM_MIXW', 5:'PWM_MIXD',
        6:'PWM_SVFF', 7:'PWM_SVFQ', 8:'GND', 9:'PWM_GATE', 10:'PWM_BYP',
        11:'PWM_STARVE', 12:'CVOUT_PWM', 13:'GND', 14:'FREQ_A', 15:'FREQ_B',
        16:'FMODE_A', 17:'FMODE_B', 18:'GND', 19:'FMODE_C', 20:'WS_DATA_3V3',
        21:'STOMP1', 22:'STOMP2', 23:'GND', 24:'MUX_S0', 25:'MUX_S1',
        26:'MUX_S2', 27:'MUX_S3', 28:'GND', 29:None, 30:None,
        31:'ADC_MUXED', 32:'CV1_ADC', 33:'GND', 34:'CV2_ADC',
        35:'ADC_VREF_F', 36:'3V3', 37:None, 38:'GND', 39:'PICO_VSYS', 40:None})
C(s.add('Device:D_Schottky','D90','SS14 (+5V->VSYS)',(33.02,45.72),angle=90), {2:'+5V', 1:'PICO_VSYS'})
C(s.add('Device:C_Polarized','C90','10u',(33.02,63.5)), {1:'PICO_VSYS', 2:'GND'})
C(s.add('Device:C','C91','100n',(45.72,63.5)), {1:'3V3', 2:'GND'})
C(s.add('Device:R','R160','10R',(127.0,45.72),angle=90), {1:'3V3', 2:'ADC_VREF_F'})
C(s.add('Device:C','C92','1u',(139.7,63.5)), {1:'ADC_VREF_F', 2:'GND'})

# ---- PWM RC filters (1-pole here; the CV consumers add series R) -----------
s.text('PWM CV filters: 10k + 100n per CV (~160Hz pole; PWM at 100kHz+ in firmware).', (25.4, 152.4), 1.8)
pwm_map = [('PWM_FREQ','FREQ_CV'), ('PWM_DIRT','DIRT_CV'), ('PWM_MIXW','MIX_WET_CV'),
           ('PWM_MIXD','MIX_DRY_CV'), ('PWM_SVFF','SVF_F_CV'), ('PWM_SVFQ','SVF_Q_CV'),
           ('PWM_GATE','GATE_VOL_CV'), ('PWM_BYP','BYPASS_CV'), ('PWM_STARVE','STARVE_CV')]
for i,(src,dst) in enumerate(pwm_map):
    x = 30.48 + (i%5)*27.94; y = 165.1 + (i//5)*27.94
    C(s.add('Device:R', f'R16{i+1}', '10k', (x, y), angle=90), {1:src, 2:dst})
    C(s.add('Device:C', f'C9{i+3}', '100n', (x+8.89, y+12.7)), {1:dst, 2:'GND'})

# ---- ADC mux (74HC4067 on 3V3) --------------------------------------------
s.text('16ch ADC mux (3V3): C0-5 pot wipers, C6 env, C7 VA sense, C8 lock sense, C9-11 gate trims, C12-15 grounded.', (177.8, 22.86), 1.8)
u21 = s.add('74xx:CD74HC4067M','U21','74HC4067',(215.9,76.2))
C(u21, {1:'ADC_MUX_C', 9:'POT1_W', 8:'POT2_W', 7:'POT3_W', 6:'POT4_W', 5:'POT5_W',
        4:'POT6_W', 3:'ENV_ADC', 2:'VA_SENSE', 23:'LOCK_SENSE', 22:'TRIM_TH',
        21:'TRIM_HO', 20:'TRIM_FA', 19:'GND', 18:'GND', 17:'GND', 16:'GND',
        10:'MUX_S0', 11:'MUX_S1', 14:'MUX_S2', 13:'MUX_S3', 15:'GND',
        24:'3V3', 12:'GND'})
C(s.add('Device:R','R170','1k',(243.84,45.72),angle=90), {1:'ADC_MUX_C', 2:'ADC_MUXED'})
C(s.add('Device:C','C120','1n',(256.54,63.5)), {1:'ADC_MUXED', 2:'GND'})
C(s.add('Device:C','C121','100n',(256.54,83.82)), {1:'3V3', 2:'GND'})
# gate trims (main board, under the lid like the sim's internal panel)
for i,(tnet,ref,x) in enumerate([('TRIM_TH','RV1',256.54),('TRIM_HO','RV2',276.86),('TRIM_FA','RV3',297.18)]):
    C(s.add('Device:R_Potentiometer',ref,'10k trim (3364W)',(x,116.84)), {1:'3V3', 2:tnet, 3:'GND'})
s.text('RV1-3: gate THRESH / HOLD / FADE - the "inside the pedal" trims from the sim.', (243.84, 137.16), 1.5)

# ---- stomps, WS2812 buffer, control header --------------------------------
s.text('Stomps: momentary SPST to GND, 10k pullups + 100n. WS2812 data through 74AHCT1G125 on +5V (3.3V is marginal for 5V pixels).', (25.4, 208.28), 1.8)
for i,(st,x) in enumerate([('STOMP1',33.02),('STOMP2',58.42)]):
    C(s.add('Device:R', f'R17{i+1}', '10k', (x, 228.6)), {1:'3V3', 2:st})
    C(s.add('Device:C', f'C12{3+i}', '100n', (x+10.16, 245.11)), {1:st, 2:'GND'})
C(s.add('74xGxx:74AHCT1G125','U22','74AHCT1G125',(101.6,228.6)),
  {2:'WS_DATA_3V3', 4:'WS_DATA_5V', 1:'GND', 5:'+5V', 3:'GND'})
C(s.add('Device:C','C122','100n',(114.3,245.11)), {1:'+5V', 2:'GND'})
C(s.add('Device:R','R173','100R',(127.0,228.6),angle=90), {1:'WS_DATA_5V', 2:'WS_HDR'})

s.text('J10: keyed 2x8 header to the CONTROL BOARD. Only DC crosses: rails, 6 wipers, 2 stomps, WS2812 data.', (152.4, 208.28), 1.8)
C(s.add('Connector_Generic:Conn_02x08_Odd_Even','J10','CTRL 2x8',(190.5,233.68)),
  {1:'GND', 2:'+5V', 3:'3V3', 4:'GND',
   5:'POT1_W', 6:'POT2_W', 7:'POT3_W', 8:'POT4_W',
   9:'POT5_W', 10:'POT6_W', 11:'STOMP1', 12:'STOMP2',
   13:'WS_HDR', 14:'GND', 15:None, 16:'GND'})

f = s.add('power:PWR_FLAG', '#FLG200', 'PWR_FLAG', (287.02, 233.68))
connect(s, f, {1: 'PICO_VSYS'}, globals_=G)
EXPECT.setdefault('PICO_VSYS', set())

s.save('/home/claude/work/hardware/kicad/glitchwave567/mcu.kicad_sch')
ok, rep = verify_netlist('/home/claude/work/hardware/kicad/glitchwave567/mcu.kicad_sch', EXPECT)
print('VERIFY:', ok); print(rep)
