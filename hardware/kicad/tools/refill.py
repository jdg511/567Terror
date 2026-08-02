"""Refill all copper zones and save. Usage: refill.py <board.kicad_pcb>

Takes >60 s on the main board - run it detached and poll, the Desktop-Commander
bridge caps synchronous calls at about a minute.
"""
import sys
import pcbnew

path = sys.argv[1]
bd = pcbnew.LoadBoard(path)
print("zones:", len(list(bd.Zones())), flush=True)
filler = pcbnew.ZONE_FILLER(bd)
ok = filler.Fill(bd.Zones())
print("fill returned:", ok, flush=True)
bd.Save(path)
print("SAVED", path, flush=True)
