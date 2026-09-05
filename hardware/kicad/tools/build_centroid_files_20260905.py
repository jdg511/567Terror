import csv
import os

# Converts KiCad's raw export_pos CSV (Ref,Val,Package,PosX,PosY,Rot,Side) into
# a PCBWay/JLC-style Centroid (CPL) file: Designator,Mid X,Mid Y,Layer,Rotation
# -- keeping Val/Package as trailing reference columns (harmless extra columns,
# ignored by most importers, useful for a human cross-check).

SRC_FILES = [
    ("pos_ctrl.csv", "Centroid_ctrl.csv"),
    ("pos_main.csv", "Centroid_main.csv"),
]

for src, dst in SRC_FILES:
    with open(src, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    out_rows = []
    for row in rows:
        side = row["Side"].strip().lower()
        layer = "Top" if side == "top" else "Bottom" if side == "bottom" else row["Side"]
        out_rows.append({
            "Designator": row["Ref"],
            "Mid X": row["PosX"],
            "Mid Y": row["PosY"],
            "Layer": layer,
            "Rotation": row["Rot"],
            "Comment": row["Val"],
            "Footprint": row["Package"],
        })

    with open(dst, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["Designator", "Mid X", "Mid Y", "Layer", "Rotation", "Comment", "Footprint"])
        writer.writeheader()
        writer.writerows(out_rows)

    print(f"Wrote {dst}: {len(out_rows)} placement rows")
