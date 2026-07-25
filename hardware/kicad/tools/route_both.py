import sys, time, shutil, subprocess, importlib
sys.path.insert(0,'/home/claude/work/hardware/kicad/tools')

def run(board_gen, pcb, name):
    import router
    configs = [('asc', ()), ('desc', ()), ('asc', None), ('asc', None), ('asc', None), ('desc', None)]
    best = (10**9, None)
    prio_pool = set()
    for i, (mode, prio) in enumerate(configs):
        subprocess.run(['python3', board_gen], capture_output=True, cwd='/home/claude/work/hardware/kicad/tools')
        importlib.reload(router)
        p = tuple(prio_pool) if prio is None else prio
        t0 = time.time()
        fails, total = router.route_board(pcb, 138, 114, priority=p, order_mode=mode)
        print(f'{name} [{mode}{" prio" if p else ""}]: {len(fails)}/{total} failed ({time.time()-t0:.0f}s)', fails[:6])
        prio_pool |= set(fails)
        if len(fails) < best[0]:
            best = (len(fails), (mode, p))
            shutil.copy(pcb, pcb + '.best')
        if not fails: break
    shutil.copy(pcb + '.best', pcb)
    print(f'{name} BEST: {best[0]} unrouted (kept)')
    return best[0]

f1 = run('gen_pcb_ctrl.py', '/home/claude/work/hardware/kicad/glitchwave567_ctrl/glitchwave567_ctrl.kicad_pcb', 'CTRL')
f2 = run('gen_pcb_main.py', '/home/claude/work/hardware/kicad/glitchwave567/glitchwave567.kicad_pcb', 'MAIN')
print('FINAL unrouted - ctrl:', f1, 'main:', f2)
