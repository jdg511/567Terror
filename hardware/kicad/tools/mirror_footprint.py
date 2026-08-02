"""Mirror a .kicad_mod about its local Y axis (negate every local x).

Why: a board-to-board stack needs the mating connector on the OTHER side of the
upper board. Placing a normal 2x08 header on B.Cu mirrors its pad grid - the grid
maps onto itself, but the PIN NUMBERS permute (col c <-> col 9-c, so 1<->15,
2<->16, 3<->13, ...). Flipping without accounting for that silently reverses the
pinout: GND and +5V swap ends. A pre-mirrored footprint cancels the flip, so pin N
lands on pin N of the header below, with no schematic change at all.

Usage:
  mirror_footprint.py <in.kicad_mod> <out.kicad_mod> <NewFootprintName> [--descr "..."]

Negates the first coordinate of at/start/end/mid/center/xy nodes, and for 3-value
(at x y rot) nodes also maps rot -> (180 - rot) mod 360, which is what mirroring
about x does to an angle. Text justification gets a `mirror` flag so silkscreen
still reads correctly from the back.

Verify the result by placing it and comparing pad coordinates against the mating
part - see tools/side.py. Do not trust this transform on faith.
"""
import re
import sys

COORD_NODES = {"at", "start", "end", "mid", "center", "xy"}
NUM = re.compile(r"^-?\d+(\.\d+)?(e-?\d+)?$", re.I)


def tokenize(s):
    out, i, n = [], 0, len(s)
    while i < n:
        c = s[i]
        if c == '"':
            j = i + 1
            while j < n:
                if s[j] == "\\":
                    j += 2
                    continue
                if s[j] == '"':
                    break
                j += 1
            out.append(s[i:j + 1])
            i = j + 1
        elif c in "()":
            out.append(c)
            i += 1
        elif c.isspace():
            i += 1
        else:
            j = i
            while j < n and not s[j].isspace() and s[j] not in "()":
                j += 1
            out.append(s[i:j])
            i = j
    return out


def parse(tokens, pos=0):
    assert tokens[pos] == "("
    node, pos = [], pos + 1
    while tokens[pos] != ")":
        if tokens[pos] == "(":
            sub, pos = parse(tokens, pos)
            node.append(sub)
        else:
            node.append(tokens[pos])
            pos += 1
    return node, pos + 1


def fmt(v):
    f = float(v)
    if f == int(f):
        return str(int(f))
    return ("%.6f" % f).rstrip("0").rstrip(".")


def transform(node):
    if not isinstance(node, list):
        return node
    if node and isinstance(node[0], str) and node[0] in COORD_NODES:
        vals = [i for i, t in enumerate(node[1:], 1)
                if isinstance(t, str) and NUM.match(t)]
        if vals:
            xi = vals[0]
            node[xi] = fmt(-float(node[xi]))
            if node[0] == "at" and len(vals) >= 3:
                ri = vals[2]
                node[ri] = fmt((180.0 - float(node[ri])) % 360.0)
    return [transform(c) for c in node]


def render(node, depth=0):
    pad = "\t" * depth
    if not isinstance(node, list):
        return node
    head = node[0] if isinstance(node[0], str) else None
    simple = all(not isinstance(c, list) for c in node)
    if simple:
        return pad + "(" + " ".join(node) + ")"
    parts = [pad + "(" + (head or "")]
    body = node[1:] if head else node
    inline = []
    for c in body:
        if not isinstance(c, list):
            inline.append(c)
        else:
            break
    if inline:
        parts[0] += " " + " ".join(inline)
    for c in body[len(inline):]:
        parts.append(render(c, depth + 1))
    parts.append(pad + ")")
    return "\n".join(parts)


def main():
    src, dst, name = sys.argv[1], sys.argv[2], sys.argv[3]
    descr = None
    if "--descr" in sys.argv:
        descr = sys.argv[sys.argv.index("--descr") + 1]

    text = open(src, encoding="utf-8").read()
    tree, _ = parse(tokenize(text))
    assert tree[0] == "footprint", tree[0]
    tree = transform(tree)
    tree[1] = '"%s"' % name

    npads = 0
    for c in tree:
        if isinstance(c, list) and c and c[0] == "pad":
            npads += 1
        if isinstance(c, list) and c and c[0] == "descr" and descr:
            c[1] = '"%s"' % descr
        if isinstance(c, list) and c and c[0] == "property" and len(c) > 2 \
                and c[1] == '"Value"':
            c[2] = '"%s"' % name

    open(dst, "w", encoding="utf-8", newline="\n").write(render(tree) + "\n")
    print("wrote %s  (%d pads)" % (dst, npads))


if __name__ == "__main__":
    main()
