# Glitchwave 567 — JUCE Circuit Simulation

A software simulation of the Glitchwave 567 guitar pedal (LM567 tone-decoder glitch
pedal, schematic in `glitchwave.png`). Builds as a **Standalone app**, **VST3**, and
(on macOS) **AU** — for Windows, Linux, and macOS. Vendor/manufacturer name in all
builds: **Illicit Apothecary**.

## Project plan status

* Step 1 (done): faithful sim of the stock schematic.
* **Step 2 (this, v0.2): mods — LFO stack + envelope follower, 3 audio file players,
  sidechain/CV control of every knob. See `docs/MODS.md` for the full manual.**
* Step 3 (later): PCB files for PCBWay.

## v0.2 mods in one paragraph

LFO 1 (rate/depth/shape/target dropdown) wobbles any knob — its rate is bent by LFO 2
("LFO modulated by another LFO"), and an envelope follower pushes FREQ with your playing
dynamics (Env→Freq knob, bipolar). Three file players: PLAY toggles play/stop, Player 1
routes into the circuit like live input (with a Level knob), Players 2 and 3 are CV
sources. CV 1 = Sidechain Left + Player 2; CV 2 = Sidechain Right + Player 3. Each CV bus
has a target dropdown (any knob or the mod controls) plus mini Strength and Slew knobs —
and CV 2 can also target CV 1's Strength and Slew. Knobs stay live; CV adds on top.

## Downloads (Windows / Linux / macOS)

Every push to `main` builds all three platforms automatically via GitHub Actions
(see `.github/workflows/build.yml`): VST3 + Standalone for Windows and Linux, and
VST3 + Standalone + AU for macOS. Grab the zipped builds from the **Actions** tab
on the repo, under the latest workflow run's Artifacts section. Pushing a version
tag (e.g. `v0.10.0`) also publishes a **GitHub Release** with all three zips
attached. macOS/Linux builds from CI are unsigned — on macOS you may need to
right-click > Open (or clear the quarantine flag) the first time.

## Building on Windows

1. Install **Visual Studio 2022 Community** (free) with the
   **"Desktop development with C++"** workload. That includes CMake — nothing else needed.
2. Double-click **`build.bat`**. First run downloads JUCE and takes a few minutes.
3. Outputs:
   * Standalone: `build\Glitchwave567_artefacts\Release\Standalone\Glitchwave 567.exe`
   * VST3: `build\Glitchwave567_artefacts\Release\VST3\Glitchwave 567.vst3`
     (copy the whole `.vst3` folder into `C:\Program Files\Common Files\VST3`)

In the Standalone app, click **Options → Audio/MIDI Settings** to pick your interface
and enable the input (that's your guitar going "into the pedal").

## The controls (same as the real pedal)

| Knob | Circuit part | Range (v0.3) |
|------|--------------|--------------|
| FREQ | 567 lock frequency | 0.1 Hz … 18 kHz (stock pedal was 304–1148 Hz) |
| LPF  | Sallen-Key low-pass (was "FIZZ") | 200 Hz … 20 kHz |
| RES  | LPF resonance | Q 0.25 … 8 |
| DRY  | DRY1 (A100k) | clean level into the output mixer |
| VOL  | VOL1 (A100k) | glitch (wet) level into the output mixer |
| Dry>LPF | *(mod)* | how much the dry path also goes through the LPF |
| Input Trim | *(sim only)* | ±24 dB to match your interface level to "guitar level" |

Set Input Trim so that normal playing feels like it drives the pedal the way your real
rig would — the 567's tracking is very level-dependent, just like the real chip.

## Files

* `src/dsp/Glitchwave567.h` — the circuit model itself (portable C++, no JUCE)
* `src/PluginProcessor.*`, `src/PluginEditor.*` — JUCE plugin wrapper + UI
* `docs/SIM_NOTES.md` — every schematic part → code mapping, and the assumptions made
* `tools/offline_render.cpp` — offline test harness (used to verify the sim in the cloud)

## Evaluating the sim (suggestions)

Play the same riffs through the real pedal and the sim and compare:

1. Where on the FREQ knob each riff locks / glitches
2. Chatter texture when slightly out of lock
3. Idle squeal with strings muted (and how VOL kills it)
4. Thumps when lock engages/disengages
5. FIZZ sweep brightness range

`docs/SIM_NOTES.md` lists the tunable constants we can adjust based on what you hear.
