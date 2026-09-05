import sys, shutil, os

HERE = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(HERE, "..", "glitchwave567_ctrl", "glitchwave567_ctrl.kicad_sch")
backup = os.path.join(HERE, "..", "glitchwave567_ctrl", "glitchwave567_ctrl.kicad_sch.bak_20260905c")
shutil.copy(path, backup)

text = open(path, encoding="utf-8").read()
orig_len = len(text)


def must_replace(text, old, new, label):
    n = text.count(old)
    if n != 1:
        print(f"FAIL {label}: found {n} occurrences (expected 1)")
        sys.exit(1)
    return text.replace(old, new)


text = must_replace(
    text,
    '(property "Value" "Alpha SF12011F-0102-20R-M-050 (TAP/BYP)"',
    '(property "Value" "Alpha SF12011F-0102-20R-M-011 (TAP/BYP)"',
    "SW1 Value -050 -> -011",
)
text = must_replace(
    text,
    '(property "Value" "Alpha SF12011F-0102-20R-M-050 (TAP/TEMPO)"',
    '(property "Value" "Alpha SF12011F-0102-20R-M-011 (TAP/TEMPO)"',
    "SW2 Value -050 -> -011",
)

open(path, "w", encoding="utf-8").write(text)
print("OK, wrote", len(text), "bytes (was", orig_len, "), backup at", backup)
