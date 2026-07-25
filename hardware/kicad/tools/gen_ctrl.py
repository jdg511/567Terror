"""CONTROL BOARD — 6 pots, 2 stomps, 6 WS2812B, keyed 2x8 header. Bolts
behind the enclosure face; only DC crosses the header."""
import sys; sys.path.insert(0, '/home/claude/work/hardware/kicad/tools')
from schgen import Sheet, connect, verify_netlist

G = {'GND','+5V','3V3'}
s = Sheet('Glitchwave 567 — Control Board', paper='A3', rev='0.1')
EXPECT = {}
def C(part, mapping):
    connect(s, part, mapping, globals_=G)
    for pin, net in mapping.items():
        if net is None: EXPECT.setdefault('_FLOATING', set()).add(f'{part.ref}.{pin}')
        else: EXPECT.setdefault(net, set()).add(f'{part.ref}.{pin}')

s.text('CONTROL BOARD: bolts behind the drilled face. 6x ALPS RK09K1130A5R (B10k lin), 2x Suntsu soft-touch stomps,', (25.4, 22.86), 1.8)
s.text('6x WS2812B (3 section lights + tempo + bypass + gate). Pot order: FREQ GAIN MIX / FIZZ Q VOL (2 rows of 3).', (25.4, 26.67), 1.8)
C(s.add('Connector_Generic:Conn_02x08_Odd_Even','J1','to MAIN 2x8',(45.72,63.5)),
  {1:'GND', 2:'+5V', 3:'3V3', 4:'GND',
   5:'POT1_W', 6:'POT2_W', 7:'POT3_W', 8:'POT4_W',
   9:'POT5_W', 10:'POT6_W', 11:'STOMP1', 12:'STOMP2',
   13:'WS_IN', 14:'GND', 15:None, 16:'GND'})
pots = [('RV1','FREQ','POT1_W'),('RV2','GAIN','POT2_W'),('RV3','MIX','POT3_W'),
        ('RV4','FIZZ','POT4_W'),('RV5','Q','POT5_W'),('RV6','VOL','POT6_W')]
for i,(ref,name,net) in enumerate(pots):
    x = 101.6 + (i%3)*38.1; y = 45.72 + (i//3)*38.1
    C(s.add('Device:R_Potentiometer',ref,f'B10k ({name})',(x,y),
            footprint='Glitchwave:ALPS_RK09K1130'), {1:'3V3', 2:net, 3:'GND'})
for i,(ref,net,x) in enumerate([('SW1','STOMP1',228.6),('SW2','STOMP2',266.7)]):
    C(s.add('Switch:SW_Push',ref,'Suntsu SSWFS-S01 (TAP/BYP)',(x,45.72),
            footprint='Glitchwave:SSWFS-S01'), {1:net, 2:'GND'})
s.text('WS2812 chain: WS_IN -> LED1(sect A) -> LED2(sect B) -> LED3(sect C) -> LED4(tempo) -> LED5(bypass) -> LED6(gate).', (25.4, 121.92), 1.8)
prev = 'WS_IN'
for i in range(6):
    ref = f'LED{i+1}'; x = 40.64 + i*45.72
    nxt = f'WSD{i+1}' if i < 5 else 'WS_END'
    C(s.add('LED:WS2812B',ref,'WS2812B',(x,146.05)), {4:prev, 2:(nxt if i<5 else None), 1:'+5V', 3:'GND'})
    C(s.add('Device:C',f'C{i+1}','100n',(x+15.24,163.83)), {1:'+5V', 2:'GND'})
    prev = nxt
C(s.add('Device:C_Polarized','C7','100u',(320.04,146.05)), {1:'+5V', 2:'GND'})
for i,(net,x) in enumerate([('+5V',280.67),('3V3',295.91),('GND',311.15)]):
    f = s.add('power:PWR_FLAG', f'#FLG30{i}', 'PWR_FLAG', (x, 63.5))
    connect(s, f, {1: net}, globals_=G)

s.save('/home/claude/work/hardware/kicad/glitchwave567_ctrl/glitchwave567_ctrl.kicad_sch')
ok, rep = verify_netlist('/home/claude/work/hardware/kicad/glitchwave567_ctrl/glitchwave567_ctrl.kicad_sch', EXPECT)
print('VERIFY:', ok); print(rep)
