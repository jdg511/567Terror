import zipfile
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))  # hardware/
KICAD = os.path.join(ROOT, "kicad")
FAB = os.path.join(ROOT, "fab")

OUT_ZIP = os.path.join(FAB, "Glitchwave567_PCBWay_Release_20260911.zip")

def add_file(zf, src, arcname):
    zf.write(src, arcname)

def add_dir(zf, src_dir, arc_prefix, exclude_prefixes=()):
    for dirpath, dirnames, filenames in os.walk(src_dir):
        # prune excluded subdirs in-place
        dirnames[:] = [d for d in dirnames if not any(
            os.path.join(dirpath, d).replace("\\", "/").endswith(p) or ("/" + d + "/") in (dirpath.replace("\\", "/") + "/")
            for p in exclude_prefixes
        )]
        for fn in filenames:
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, src_dir)
            skip = False
            for p in exclude_prefixes:
                if p in full.replace("\\", "/"):
                    skip = True
                    break
            if skip:
                continue
            arc = os.path.join(arc_prefix, rel)
            zf.write(full, arc)

with zipfile.ZipFile(OUT_ZIP, "w", zipfile.ZIP_DEFLATED) as zf:
    # --- Gerbers ---
    add_dir(zf, os.path.join(FAB, "gerbers_main"), "Gerbers/MAIN_board")
    add_dir(zf, os.path.join(FAB, "gerbers_ctrl"), "Gerbers/CONTROL_board")

    # --- BOM ---
    add_file(zf, os.path.join(FAB, "PCBWay_BOM_main.csv"), "BOM/PCBWay_BOM_main.csv")
    add_file(zf, os.path.join(FAB, "PCBWay_BOM_ctrl.csv"), "BOM/PCBWay_BOM_ctrl.csv")
    add_file(zf, os.path.join(ROOT, "BOM.xlsx"), "BOM/BOM.xlsx")

    # --- Centroid / CPL ---
    add_file(zf, os.path.join(FAB, "Centroid_main.csv"), "Centroid/Centroid_main.csv")
    add_file(zf, os.path.join(FAB, "Centroid_ctrl.csv"), "Centroid/Centroid_ctrl.csv")

    # --- README ---
    add_file(zf, os.path.join(FAB, "README_PCBWAY.md"), "README_PCBWAY.md")

    # --- KiCad source: MAIN board ---
    main_dir = os.path.join(KICAD, "glitchwave567")
    exclude = (".history", ".mcp-backups", ".bak_", "checkpoint_", ".zip", "drc_violations",
               "_missing3Dmodels", ".attempt_", ".cad", "~1", "pcbway_production", "/test/", "wfandfmeet")
    for fn in sorted(os.listdir(main_dir)):
        full = os.path.join(main_dir, fn)
        if os.path.isfile(full):
            if any(x in fn for x in exclude):
                continue
            if fn.endswith((".kicad_sch", ".kicad_pcb", ".kicad_pro", ".kicad_prl", "fp-lib-table", "sym-lib-table")):
                add_file(zf, full, os.path.join("KiCad_Source/glitchwave567", fn))

    # --- KiCad source: CONTROL board ---
    ctrl_dir = os.path.join(KICAD, "glitchwave567_ctrl")
    for fn in sorted(os.listdir(ctrl_dir)):
        full = os.path.join(ctrl_dir, fn)
        if os.path.isfile(full):
            if any(x in fn for x in exclude):
                continue
            if fn.endswith((".kicad_sch", ".kicad_pcb", ".kicad_pro", ".kicad_prl", "fp-lib-table", "sym-lib-table")):
                add_file(zf, full, os.path.join("KiCad_Source/glitchwave567_ctrl", fn))

    # --- Footprint library ---
    pretty_dir = os.path.join(KICAD, "Glitchwave.pretty")
    for fn in sorted(os.listdir(pretty_dir)):
        full = os.path.join(pretty_dir, fn)
        if os.path.isfile(full) and fn.endswith(".kicad_mod"):
            add_file(zf, full, os.path.join("KiCad_Source/Glitchwave.pretty", fn))

print("Wrote", OUT_ZIP)
with zipfile.ZipFile(OUT_ZIP) as zf:
    names = zf.namelist()
    print(f"{len(names)} files in archive")
    for n in names:
        print(" ", n)
