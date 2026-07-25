repo: jdg511/567Terror
branch: main

## Last sync

date: 2026-07-25T10:19:43Z

### Updated in this project

- v0.34 Terror: glitch-art restyle of the plugin face, driven by user reference imagery.
- Knobs, switches and the supply selector are live, using the real parameter curves and display strings from `createParameterLayout()`.
- Under-the-cover panel built out: gate trimmers, JFET / ladder switches, 9–18 V sim supply, hints toggle.
- `Starve` relabelled `?` and read out as a rail voltage (5 V → supply). Undocumented.
- `+6 dB` internal switch removed from the UI (still present in repo source).

## Screen map

| Project screen | Repo files |
|---|---|
| Glitchwave 567 - v0.32 Recreation.dc.html | src/PluginEditor.cpp, src/PluginEditor.h, src/PluginProcessor.cpp |
| Glitchwave 567 - v0.33 Polish.dc.html | src/PluginEditor.cpp, src/PluginEditor.h, src/PluginProcessor.cpp |
| Glitchwave 567 - v0.34 Terror.dc.html | src/PluginEditor.cpp, src/PluginEditor.h, src/PluginProcessor.cpp |

## Pending changes for the repo

Not yet applied upstream — these are source edits I cannot make from here:

- `CMakeLists.txt` — `COMPANY_NAME "JasonDIY"` → `"Illicit Apothecary"`; `VST3_CATEGORIES Fx Distortion` → `Fx Filter`; `project(... VERSION 0.32.0)` → `0.34.0`.
- `+6 dB` boost removal: drop `boost6` from `createParameterLayout()`, `raw.boost6`, `cp.boost6Gain`, `boostBtn`, and set `boost6Gain = 1.0f` in `src/dsp/Glitchwave567.h`.
- `setScaleFactor(2.0f)` → `2.5f` in `src/PluginEditor.cpp` to match the 25% type increase in the v0.34 design.

## Sync history

### 2026-07-25T04:37:19Z

- Recreated the v0.32 JUCE plugin editor pixel-for-pixel from `src/PluginEditor.cpp` / `.h`.
- Built v0.33 Polish: same layout, tightened LEDs / type / knobs, CV row removed, window 764 → 640.

## Resolution — 2026-07-25 (applied in v0.34, commit from the sim session)

- Applied: CMakeLists metadata (company "Illicit Apothecary", Fx Filter,
  VERSION 0.34.0) and setScaleFactor(2.5f).
- OVERRULED: the +6 dB boost removal. Jason: "have it as the actual pcb has
  it" — the PCB carries the +6 dB stage on an internal DIP switch (ships ON),
  so the sim keeps the param, the DSP path, AND a third PCB SWITCHES row
  ("+6 dB BOOST") under the cover.
- Starve "?" readout uses the real DSP direction (supply at 0 → sags to 5 V at
  1), not this project's inverted mock formula.
