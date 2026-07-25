"""Acceptance-tested polish driver.

For each target net: run ripup_repair in a subprocess (fresh interpreter),
then measure TRUE unconnected count (zone fill + ratsnest). Keep the new
board only if the count decreased; otherwise roll back.
"""
import subprocess, shutil, sys, os

TOOLS = os.path.dirname(os.path.abspath(__file__))
PCB = sys.argv[1]
TARGETS = sys.argv[2].split(',')

MEASURE = '''
import pcbnew
b = pcbnew.LoadBoard(%r)
filler = pcbnew.ZONE_FILLER(b); filler.Fill(b.Zones())
b.BuildConnectivity()
print("COUNT=" + str(b.GetConnectivity().GetUnconnectedCount(True)))
'''


def measure(path):
    r = subprocess.run(['python3', '-c', MEASURE % path],
                       capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if line.startswith('COUNT='):
            return int(line.split('=')[1])
    print('measure failed:', r.stdout[-500:], r.stderr[-500:])
    return None


base = measure(PCB)
print(f'baseline unconnected: {base}')
for nm in TARGETS:
    shutil.copy(PCB, PCB + '.try_bak')
    r = subprocess.run(['python3', os.path.join(TOOLS, 'ripup_repair.py'), PCB, nm],
                       capture_output=True, text=True)
    tail = [l for l in r.stdout.splitlines() if 'ROUTED' in l or 'FAILED' in l or 'REMAINING' in l]
    now = measure(PCB)
    if now is not None and now < base:
        print(f'{nm}: ACCEPT {base} -> {now} | {tail[:1]}')
        base = now
    else:
        shutil.copy(PCB + '.try_bak', PCB)
        print(f'{nm}: REJECT (was {base}, got {now}) | {tail[:1]}')
print(f'final unconnected: {base}')
