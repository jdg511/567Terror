"""Turn a kicad-cli kicadxml netlist into review-ready JSON + digests.

The kicad-schematic-review skill ships an extract_kicad_sch.py that parses
.kicad_sch directly and warns it cannot follow hierarchical sheets. This project
IS hierarchical (8 sub-sheets on the main board), so instead we use KiCad's OWN
computed netlist as the factual basis - the same connectivity the PCB was built
from, rather than home-grown wire tracing.

Outputs, next to the input file:
  <stem>.components.json  ref -> value, footprint, libsource, sheet, all fields
  <stem>.pinmap.json      ref -> {pin: {name, type, net}}
  <stem>.nets.json        net -> [{ref, pin, pinname, pintype}]
  <stem>.digest.txt       human-readable summary for review

Usage: extract_netlist.py <netlist.xml>
"""
import json
import re
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict


def txt(el, path, default=""):
    n = el.find(path)
    return (n.text or default) if n is not None else default


def main():
    src = sys.argv[1]
    stem = re.sub(r"\.netlist\.xml$", "", src)
    root = ET.parse(src).getroot()

    comps = {}
    for c in root.findall("./components/comp"):
        ref = c.get("ref")
        fields = {}
        for f in c.findall("./fields/field"):
            fields[f.get("name")] = (f.text or "").strip()
        ls = c.find("./libsource")
        pr = c.find("./property[@name='Datasheet']")
        comps[ref] = {
            "ref": ref,
            "value": txt(c, "value"),
            "footprint": txt(c, "footprint"),
            "datasheet": txt(c, "datasheet") or (pr.get("value") if pr is not None else ""),
            "libsource": (ls.get("lib") + ":" + ls.get("part")) if ls is not None else "",
            "description": (ls.get("description") or "") if ls is not None else "",
            "sheet": (c.find("./sheetpath").get("names")
                      if c.find("./sheetpath") is not None else "/"),
            "fields": fields,
        }

    pinmeta = {}
    for lp in root.findall("./libparts/libpart"):
        key = lp.get("lib") + ":" + lp.get("part")
        pins = {}
        for p in lp.findall("./pins/pin"):
            pins[p.get("num")] = {"name": p.get("name"), "type": p.get("type")}
        pinmeta[key] = pins

    nets = {}
    pinmap = defaultdict(dict)
    for n in root.findall("./nets/net"):
        name = n.get("name")
        members = []
        for nd in n.findall("./node"):
            ref, pin = nd.get("ref"), nd.get("pin")
            meta = pinmeta.get(comps.get(ref, {}).get("libsource", ""), {}).get(pin, {})
            pname = nd.get("pinfunction") or meta.get("name", "")
            ptype = nd.get("pintype") or meta.get("type", "")
            members.append({"ref": ref, "pin": pin, "pinname": pname, "pintype": ptype})
            pinmap[ref][pin] = {"name": pname, "type": ptype, "net": name}
        nets[name] = members

    json.dump(comps, open(stem + ".components.json", "w"), indent=1)
    json.dump(pinmap, open(stem + ".pinmap.json", "w"), indent=1)
    json.dump(nets, open(stem + ".nets.json", "w"), indent=1)

    out = []
    out.append("SOURCE: %s" % src)
    out.append("components: %d   nets: %d" % (len(comps), len(nets)))
    byprefix = defaultdict(list)
    for r in comps:
        byprefix[re.match(r"^[A-Za-z]+", r).group(0)].append(r)
    out.append("by refdes prefix: " + ", ".join(
        "%s=%d" % (k, len(v)) for k, v in sorted(byprefix.items())))

    out.append("")
    out.append("=== ICs and multi-pin parts (U*, plus any part with >=5 pins) ===")
    for ref in sorted(comps, key=lambda r: (re.match(r"^[A-Za-z]+", r).group(0),
                                            int(re.sub(r"\D", "", r) or 0))):
        np_ = len(pinmap.get(ref, {}))
        if not (ref.startswith("U") or np_ >= 5):
            continue
        c = comps[ref]
        out.append("%-7s %-28s %-34s pins=%-3d sheet=%s" % (
            ref, c["value"][:28], c["libsource"][:34], np_, c["sheet"]))
        out.append("        footprint: %s" % c["footprint"])
        out.append("        datasheet: %s" % (c["datasheet"] or "(none)"))
        for pin in sorted(pinmap.get(ref, {}), key=lambda p: (len(p), p)):
            d = pinmap[ref][pin]
            out.append("          pin %-4s %-16s %-14s -> %s" % (
                pin, (d["name"] or "")[:16], d["type"], d["net"]))
        out.append("")

    out.append("=== nets with only ONE node (potential floating/unconnected) ===")
    singles = [(k, v) for k, v in nets.items() if len(v) == 1]
    for k, v in sorted(singles):
        out.append("  %-44s %s.%s (%s)" % (k, v[0]["ref"], v[0]["pin"], v[0]["pintype"]))
    out.append("  count: %d" % len(singles))

    out.append("")
    out.append("=== power/supply-ish nets (node counts) ===")
    for k in sorted(nets):
        if re.search(r"(GND|VCC|VDD|VSS|VA\b|V5|5V|3V3|3\.3|9V|12V|18V|VREF|VBUS|RAW|"
                     r"V567|POWER|SUPPLY)", k, re.I):
            out.append("  %-44s %d nodes" % (k, len(nets[k])))

    open(stem + ".digest.txt", "w", encoding="utf-8").write("\n".join(out))
    print("wrote %s.{components,pinmap,nets}.json and .digest.txt" % stem)
    print("components=%d nets=%d single-node-nets=%d" % (len(comps), len(nets), len(singles)))


if __name__ == "__main__":
    main()
