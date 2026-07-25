# Glitchwave 567 — STEP 3: Hardware / PCB (PCBWay)

Status: **spec draft v1** (2026-07-25). Owner: hardware session. Nothing here
changes the plugin; the plugin (v0.32) is the reference behavior this board
must reproduce.

## Locked decisions (Jason, 2026-07-25)

| Decision | Choice |
|---|---|
| Assembly | **PCBWay full turnkey** — PCBWay sources and solders EVERY part including pots, stomp switches, jacks, and the Pico module. Consequence: all mechanical parts must be LCSC/PCBWay-sourceable, and enclosure drilling must match the board exactly (1:1 drill template is a hard deliverable). Jason's only build steps: drill the 1590XX, drop the board in, tighten the nuts. |
| Board count | **Main board + stomp daughterboard** (decided 2026-07-25, supersedes single-board): pots, LEDs, jacks and all circuitry on the main board behind the face; the two stomp switches on a small second PCB, joined by a short ribbon/header. Both boards in the same PCBWay order/panel. |
| Enclosure | **Hammond 1590XX** (dimensions verified against Hammond drawing during layout) |
| CV jacks | **2 CV inputs + 1 CV output**, on the side walls |
| CAD | KiCad — authored in the cloud (v7 file format), opens cleanly in Jason's KiCad 10.0 |

## The one big architectural fact

The v0.32 control scheme gives every panel knob **four meanings** (X plain /
Y TAP-held / Z BYPASS-held / A both-held), with soft-takeover, tap tempo,
16 LFO shapes including Lorenz/Rossler chaos, selectable modulation routing,
and re-seed on tap. **A passive pot cannot be four analog pots at once.**

Therefore the pedal is *digitally-controlled analog*:

- The **audio path is 100% analog** end to end (buffer → dirt → LM567 →
  SVF → voicing → JFET/ladder → gate → out). No audio ever touches the Pico.
- The **Raspberry Pi Pico is the CV brain**: it reads the 6 panel pots, the
  2 stomps, the 2 CV input jacks and the envelope level through its ADC/mux,
  runs the LFOs + modulation routing + tap tempo + layer logic in firmware,
  and steers the analog blocks through control elements (below). It also
  drives the WS2812 LEDs and the CV OUT jack.
- The **internal trim pots and DIP switches are real analog components** in
  the audio path (gate thresh/hold/fade pots, JFET switch, −3/−6 ladder
  switch, +6 dB switch) — exactly like the sim's "under the cover" panel.

## Block diagram (audio path, left→right)

```
IN jack ─ buffer ─ HP40 (24dB/oct Butterworth, 2×Sallen-Key) ─┬─ DRY: Bazz Fuss (starved rail) ──┐
                                                              │                                  ├─ MIX (VCA xfade)
                                                              └─ +15dB trim ─ LM567 demod ───────┘      │
                                                                                              SVF filter (LP/BP/HP/Notch)
                                                                                                        │
                                                              DC block ─ HP60 ─ bell +3dB@800 ─ +6dB [DIP] ─ JFET J201 [DIP]
                                                                                                        │
                                                                                     −3/−6 asym diode ladder [DIP]
                                                                                                        │
                                                                                        gate (VCA) ─ VOL ─ buffer ─ OUT jack
```

## Control elements — how the Pico steers analog blocks

Recommended element per controlled parameter (SMD, PCBWay-sourceable):

| Parameter | Range (from sim) | Element | Notes |
|---|---|---|---|
| 567 FREQ | 0.2 Hz – 6 kHz | DAC/PWM current source into RT node + **4 switched timing caps** (analog mux 74HC4052) | stock RT/CT only covers ~300–1150 Hz; 4 cap decades × current span covers the full range |
| Dirt GAIN | ×1.1 – ×300 | digipot (MCP42100 half) as input attenuator into fixed-gain Bazz Fuss drive | log law done in firmware |
| SVF cutoff | 20 Hz – 8.8 kHz (Lo/Hi) | **LM13700 OTA state-variable filter**, cutoff = Iabc from PWM CV | classic CV-controlled SVF; Lo/Hi range = firmware scaling |
| SVF Q | 0.25 – 8 | digipot in the damping leg | |
| MIX crossfade | D100/FX0 … D0/FX100 | LM13700 dual-VCA crossfade | |
| VOL | 0–100% | LM13700 VCA (or digipot — decided at schematic time) | |
| Output gate | −96 dB fade | same VOL VCA, driven by firmware gate logic (thresh/hold/fade **trim pots** read by Pico ADC) | gate LED topside driven by Pico |
| STARVE | rail sag → 5 V floor | op-amp rail servo: PWM CV sets the Bazz Fuss local rail, hard 5 V floor in hardware | both-stomps-held gesture, per sim |
| Env follower | Mu-Tron ballistics | **analog** full-wave rectifier + 4 ms/150 ms ballistics → Pico ADC; routing to targets done in firmware | keeps the Mu-Tron feel analog, routing flexible |
| LFO1/LFO2 | 16 shapes, chaos | pure firmware → PWM+RC filtered CVs | |
| CV in 1/2 | audio-rate sidechain | rectify+slew analog front end → Pico ADC | matches plugin CV bus behavior |
| CV out | 0–5 V | PWM → 2-pole RC → op-amp buffer → side jack | firmware-selectable source (LFO1 default) |

## Power tree

```
9–18 V center-negative DC jack (back wall)
 └─ P-FET reverse protection (AO3401A) ─ ferrite + RC filter
     ├─ VA raw analog rail (9–18 V): TL074 stages, JFET, ladder, SVF — headroom rides the supply (the 18 V = +6 dB feature)
     │   └─ starve servo (Bazz Fuss local rail only, 5 V floor)
     ├─ 9 V LDO (78L09, SOT-89): LM567 only (abs max ~9 V)
     └─ 5 V buck (MP1584 or TPS562201): Pico VSYS + WS2812 LEDs
VA/2 mid-rail reference (buffered) for all single-supply op-amp stages
```

All electrolytics ≥25 V; op-amps 36 V-rated (TL074 family).

## Semiconductor / key part candidates (all SMD unless noted)

| Block | Part | Package |
|---|---|---|
| Op-amps (buffers, filters, voicing, gate, servos) | TL074 ×4–5 | SOIC-14 |
| Tone demodulator | LM567 (LM567CMX) | SOIC-8 |
| Dirt | MMBTA18 (or BC847C) + 1N4148W | SOT-23 / SOD-123 |
| Output JFET stage | MMBFJ201 | SOT-23 |
| Ladder diodes | 1N4148W array (asym −3/−6 network per v0.24 curves) | SOD-123 |
| OTA (SVF + VCAs) | LM13700 ×2 | SOIC-16 |
| Digipots | MCP42100 (dual 100k, SPI) ×2 | SOIC-14 |
| Analog mux (timing caps) | 74HC4052 | TSSOP-16 |
| Reverse protection | AO3401A P-FET + zener gate clamp | SOT-23 |
| 9 V reg | 78L09 | SOT-89 |
| 5 V | MP1584EN buck (or 78M05 DPAK if EMI testing says so) | SOIC-8 |
| MCU | **Raspberry Pi Pico module** (castellated, hand- or machine-soldered) | module |
| LEDs | WS2812B ×3 (sections) + tempo LED + bypass LED + gate LED | 5050 / 3 mm |

Pico as a *module* (not bare RP2040) is deliberate: no crystal/flash/USB
layout risk, drag-and-drop firmware updates over USB by just opening the
back plate, ~$4.

## Panel & mechanical plan (single board behind the face)

```
        BACK WALL:   [ OUT jack ]   [ 9V DC ]   [ IN jack ]
LEFT WALL:                                            RIGHT WALL:
 CV1 IN ○                                              ○ CV OUT
 CV2 IN ○
        ┌─────────────────────────────────────────┐
        │   (FREQ)     (LPF)      (MIX)           │   ← knob row 1
        │   (LFO1 RT)  (LFO2 RT)  (ENV GAIN)      │   ← knob row 2
        │    ●tempo  ●gate  ◎◎◎ section LEDs      │
        │   [ TAP stomp ]        [ BYPASS stomp ] │   ← closest to player
        └─────────────────────────────────────────┘
```

- 6 pots: 9 mm Alpha board-mount (RK09K style), knobs in a symmetric 3+3 grid.
- 2 stomps: soft-touch momentary SPST, **on their own daughterboard** (the
  layer system needs momentary action; latching done in firmware per the
  v0.27/v0.31 rules; true-bypass relay or buffered bypass decided at
  schematic time). Daughterboard → main board via a keyed 4-pin header
  (GND, 3V3, STOMP1, STOMP2) + short ribbon.
- Jacks: Amphenol ACJM / Neutrik NMJ box-style board-mount 1/4″ on the back
  wall + side walls; DC = board-mount barrel (2.1 mm center-negative).
- Internal: 3 gate trim pots, 3-pos DIP or slide switches (JFET / ladder /
  +6 dB) — board-top, reachable with the back plate off, per the sim's
  "under the cover" panel.
- Gate LED, tempo LED, bypass LED, 3 WS2812 — topside, panel light-pipes.

## Deliverables of STEP 3

1. `hardware/kicad/glitchwave567/` — full KiCad project (schematic + main
   board + stomp daughterboard).
2. `hardware/fab/` — gerber+drill ZIP for PCBWay, assembly BOM (with LCSC/
   distributor part numbers) + centroid CPL for their SMT service.
3. `hardware/drill_template.pdf` — 1:1 face/side/back drilling template.
4. `hardware/ORDERING.md` — click-by-click PCBWay ordering instructions.
5. Pico firmware — **separate follow-on task** after the board is ordered
   (the board is designed so firmware can be updated over USB forever).

## Open items (flagged, not blocking)

- Bypass style: relay true-bypass vs buffered bypass (sim is buffered with
  10 ms crossfade — buffered matches the sim exactly and is simpler).
- VOL: VCA vs digipot (VCA shares the gate stage — likely free).
- Momentary stomp part choice (soft-touch SPST) — confirm feel with Jason.
- Exact 1590XX drawing + pot/stomp heights verified via find-missing-3d-models
  before layout.
