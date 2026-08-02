"""Summarise a kicad-cli ERC json report. Usage: ercsum.py <report.json>"""
import collections
import json
import sys

rep = json.load(open(sys.argv[1], encoding="utf-8"))
sheets = rep.get("sheets", [])
rows = []
for sh in sheets:
    for v in sh.get("violations", []):
        rows.append((sh.get("path", "?"), v))

by = collections.Counter(v.get("type") for _, v in rows)
sev = collections.Counter(v.get("severity") for _, v in rows)
print("file:", sys.argv[1], " violations:", len(rows))
print("by severity:", dict(sev))
for t, n in by.most_common():
    print("   %-32s %d" % (t, n))
print()
for path, v in rows:
    items = v.get("items", [])
    where = ""
    if items and "pos" in items[0]:
        where = "(%.1f,%.1f)" % (items[0]["pos"]["x"], items[0]["pos"]["y"])
    print("   %-7s %-24s %-12s %s" % (
        v.get("severity"), path[:24], where, v.get("description", "")[:110]))
    for it in items:
        d = it.get("description", "")
        if d:
            print("             - %s" % d[:110])
