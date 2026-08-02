"""Summarise a kicad-cli DRC json report. Usage: drcsum.py <report.json> [--detail TYPE]"""
import collections
import json
import sys

rep = json.load(open(sys.argv[1], encoding="utf-8"))
viol = rep.get("violations", [])
unconn = rep.get("unconnected_items", [])
sch = rep.get("schematic_parity", [])

by = collections.Counter()
sev = collections.Counter()
for v in viol:
    by[v.get("type")] += 1
    sev[v.get("severity")] += 1

print("file:", sys.argv[1])
print("violations:", len(viol), " unconnected:", len(unconn), " parity:", len(sch))
print("by severity:", dict(sev))
print("by type:")
for t, n in by.most_common():
    errs = sum(1 for v in viol if v.get("type") == t and v.get("severity") == "error")
    print("   %-28s %4d   (errors: %d)" % (t, n, errs))

if "--detail" in sys.argv:
    want = sys.argv[sys.argv.index("--detail") + 1]
    print()
    print("---- detail for", want, "----")
    n = 0
    for v in viol:
        if v.get("type") != want:
            continue
        n += 1
        if n > 40:
            print("   ... (%d more)" % (by[want] - 40))
            break
        items = v.get("items", [])
        where = ""
        if items and "pos" in items[0]:
            where = "(%.2f,%.2f)" % (items[0]["pos"]["x"], items[0]["pos"]["y"])
        print("   %-6s %-13s %s" % (v.get("severity"), where, v.get("description", "")[:150]))
        for it in items:
            print("            - %s" % it.get("description", "")[:140])
