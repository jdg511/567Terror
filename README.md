# Glitchwave 567 — JUCE Circuit Simulation

A software simulation of the Glitchwave 567 guitar pedal (LM567 tone-decoder glitch
pedal, schematic in `glitchwave.png`). Builds as a **Standalone app**, **VST3**, and
(on macOS) **AU** — for Windows, Linux, and macOS. Vendor/manufacturer name in all
builds: **Illicit Apothecary**.

## Project plan status

* Step 1 (done): faithful sim of the stock schematic.
* Step 2 (done, current plugin v0.60): mods. LFO stack, Mu-Tron style envelope follower,
  stomp gestures, STARVE power model, and two CV inputs. See `docs/MODS.md`.
* Step 3 (next, separate session): redo the hardware from the plugin. Start with
  `docs/HARDWARE_HANDOFF_v0.60.md`. PCBWay files come after that.

## Where the plugin is now (v0.60) in one paragraph

Product name **Where The Fuzz Meets The Funk** (internal short name WTF). The LM567 rail is
modeled as a fixed LM2937-8.0 (hardware still has a 78L09 until the hardware session swaps it).
The envelope follower listens to the raw input as it enters the pedal. There is one audio file
player (Player 1, feeds the circuit like live input). CV 1 and CV 2 are true jack-detect inputs
fed from the host sidechain (left = CV 1, right = CV 2, mono feeds both, bus off by default):
CV 1 rides LFO 1 depth, CV 2 is the audio the envelope follower follows. Plugged means no
fallback, even when silent. The old "assign CV to any knob" dropdowns, strength and slew knobs,
and Players 2 and 3 were dropped because the hardware will not have them. Signal flow drawing:
`docs/WTF_signal_flow_v0.60.drawio`.

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
