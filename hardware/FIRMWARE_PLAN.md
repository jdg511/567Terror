# Glitchwave 567 — Pico Firmware Plan (STEP 4 prep, rev 0.1, 2026-07-25)

Firmware target: **Raspberry Pi Pico (RP2040) module U20**, C/C++ with the
official pico-sdk (chosen over MicroPython for PWM precision and PIO WS2812).
Jason never edits code — every tunable lives in one `tunables.h`, mirrored
from the plugin's `Tunables` struct, and the pedal updates over USB
drag-and-drop (UF2) through the open back plate.

Everything below is netlist-accurate, extracted from `gen_mcu.py`,
`gen_core567.py`, `gen_ctrl.py` at commit 1949fc3.

## 1. Pin map (authoritative)

| GPIO | Pico pin | Net | Function |
|---|---|---|---|
| GP0 | 1 | PWM_FREQ | 567 pitch CV → 2-pole RC (10k+100n ×2) → FREQ_CV → OTA Iabc |
| GP1 | 2 | PWM_DIRT | Dirt gain VCA CV |
| GP2 | 4 | PWM_MIXW | Mix crossfade WET VCA CV |
| GP3 | 5 | PWM_MIXD | Mix crossfade DRY VCA CV |
| GP4 | 6 | PWM_SVFF | SVF cutoff CV |
| GP5 | 7 | PWM_SVFQ | SVF resonance CV |
| GP6 | 9 | PWM_GATE | Gate×VOL VCA CV (one VCA does both — product computed here) |
| GP7 | 10 | PWM_BYP | Buffered-bypass crossfade VCA CV (10 ms fade) |
| GP8 | 11 | PWM_STARVE | Bazz Fuss rail-servo CV (5 V hardware floor) |
| GP9 | 12 | CVOUT_PWM | CV OUT jack (2-pole RC + buffer on env sheet) |
| GP10 | 14 | FREQ_A | CD4052 timing-cap select bit A — **INVERTED** (Q2 shifter) |
| GP11 | 15 | FREQ_B | CD4052 timing-cap select bit B — **INVERTED** (Q3 shifter) |
| GP12 | 16 | FMODE_A | SVF mode select A — **INVERTED** (Q4 shifter) |
| GP13 | 17 | FMODE_B | SVF mode select B — **INVERTED** (Q5 shifter) |
| GP14 | 19 | FMODE_C | SVF mode select C — **INVERTED** (Q6 shifter) |
| GP15 | 20 | WS_DATA_3V3 | WS2812 data → 74AHCT1G125 → 100R → header → 6-LED chain |
| GP16 | 21 | STOMP1 | Stomp 1, active-LOW (10k pullup to 3V3 + 100n) |
| GP17 | 22 | STOMP2 | Stomp 2, active-LOW |
| GP18 | 24 | MUX_S0 | 74HC4067 address 0 |
| GP19 | 25 | MUX_S1 | 74HC4067 address 1 |
| GP20 | 26 | MUX_S2 | 74HC4067 address 2 |
| GP21 | 27 | MUX_S3 | 74HC4067 address 3 |
| GP22 | 29 | — | spare |
| GP26/A0 | 31 | ADC_MUXED | 4067 common (through 1k, 1n to GND) |
| GP27/A1 | 32 | CV1_ADC | CV1 jack (analog rectify+slew front end) |
| GP28/A2 | 34 | CV2_ADC | CV2 jack (same) |

ADC_VREF (pin 35) is NC — onboard 201R/2.2u filter is authoritative.
VSYS fed from +5V buck through SS14 (D90): USB and pedal power coexist.

## 2. ADC mux channel map (74HC4067, U21)

| Ch | Net | Meaning |
|---|---|---|
| C0 | POT1_W | FREQ pot (B10k linear) |
| C1 | POT2_W | GAIN pot |
| C2 | POT3_W | MIX pot |
| C3 | POT4_W | FIZZ pot |
| C4 | POT5_W | Q pot |
| C5 | POT6_W | VOL pot |
| C6 | ENV_ADC | analog env follower (Mu-Tron ballistics, 4/150 ms) |
| C7 | VA_SENSE | VA rail divider — measures the 9–18 V supply |
| C8 | LOCK_SENSE | LM567 lock/output activity sense |
| C9 | TRIM_TH | gate THRESHOLD trim (RV1, inside pedal) |
| C10 | TRIM_HO | gate HOLD trim (RV2) |
| C11 | TRIM_FA | gate FADE trim (RV3) |
| C12–C15 | GND | grounded spares (use for self-test zero reading) |

Scan rules: set S0–S3, wait ≥20 µs (1k×1n plus mux Ron settle), take 4×
oversampled reads, median-of-4. Full 12-channel scan at 1 kHz control rate
is trivial (~1% of one core).

## 3. Control loop architecture (core 0)

1 kHz tick:
- Scan mux + CV1/CV2 direct ADCs.
- Pot conditioning: 16-sample moving average + 8-LSB hysteresis (kills
  zipper without feeling laggy — same reasoning as the plugin smoothing).
- Apply taper tables (below), gestures, gate logic, env/LFO routing.
- Write 9 PWM CV duty cycles + WS2812 frame (only on change).

PWM config: all CV outputs at **125 kHz / 10-bit** (125 MHz ÷ 1024) — far
above the 160 Hz RC poles; FREQ's second pole makes pitch CV extra clean.
CVOUT runs at the same rate into its 2-pole filter and op-amp buffer; the
env_cv sheet scales it to the 0–5 V jack range.

Core 1: WS2812 PIO feed + USB serial console (live tunable editing, like
A/B-ing the plugin's Tunables — settings saved to flash with wear-levelled
page rotation).

## 4. Taper & law tables (mirror the plugin exactly)

- **VOL (audio taper on linear B10k):** a = (81^x − 1)/80 — the plugin's
  exact law (10% at half rotation). Then GATE_CV = a × gateEnv before one
  shared VCA (VOL and gate multiply into PWM_GATE).
- **FREQ:** plugin f0 span 0.2 Hz–6 kHz. Firmware: pick cap range
  (47n/1u/22u/470u via FREQ_A/B — remember bits are INVERTED by the
  level shifters) so ranges overlap, then exponential law within a range
  driving the OTA Iabc CV. Hysteresis on range switching (switch only when
  crossing 10% past a boundary) to avoid cap-swap clicks; blank the loop
  filter... no LFIL cap exists — clicks ARE the voice; just don't oscillate
  between ranges.
- **GAIN (dirt):** log law ×1.1–×300 (VCA attenuator before fixed ×300).
- **SVF cutoff:** exponential 20 Hz–8.8 kHz; **Q:** 0.25–8 exponential.
- **MIX:** constant-power crossfade pair (PWM_MIXW/PWM_MIXD).
- **FIZZ (POT4):** on the pedal the CV-controlled SVF replaces the plugin's
  dual-gang Sallen-Key, so FIZZ drives SVF cutoff (PWM_SVFF) with the
  plugin's 8.1 kHz→1.34 kHz feel mapped onto the wider 20 Hz–8.8 kHz range;
  Q pot drives resonance; FMODE (stomp gesture) cycles LP/BP/HP/Notch
  via FMODE_A/B/C (inverted bits).

## 5. Gate / bypass / stomp logic

- **Gate:** envelope from ENV_ADC vs TRIM_TH threshold; hold TRIM_HO
  (0–500 ms), fade TRIM_FA (1–300 ms) — the sim's internal-panel trims.
  gateEnv multiplies VOL into PWM_GATE. Gate LED = WS2812 #6.
- **Bypass (STOMP1, short press):** 10 ms equal-power crossfade on PWM_BYP —
  matches the sim's silent switching. Bypass LED = WS2812 #5.
- **Tap tempo (STOMP2, taps):** sets LFO1 rate; tempo LED = WS2812 #4,
  blinks at rate.
- **STOMP2 hold:** cycle SVF mode (FMODE bits), section LEDs flash the mode.
- **Both stomps held (the sim's gesture):** STARVE ramp — PWM_STARVE sags
  the Bazz Fuss rail toward the 5 V hardware floor while held, recovers on
  release (attack/release ms in tunables).
- Debounce: 5 ms integrator per stomp; press <400 ms = tap, ≥400 ms = hold.

## 6. Modulation matrix (firmware)

Sources: LFO1, LFO2 (16 shapes incl. chaos — port the plugin tables), ENV,
CV1, CV2, VA_SENSE (rail-aware scaling), LOCK_SENSE (self-modulation!).
Targets: any PWM CV. Default routing mirrors plugin v0.32; matrix editable
over USB console, stored in flash. CVOUT source selectable (LFO1 default).

## 7. WS2812 chain (order matters — from gen_ctrl.py)

`WS_IN → LED1 (section A) → LED2 (section B) → LED3 (section C) → LED4
(tempo) → LED5 (bypass) → LED6 (gate)`. PIO driver, 800 kHz, gamma-corrected
low brightness default (pedalboard-friendly), full state model:
boot rainbow sweep → section activity (env-reactive) / tempo blink /
bypass solid / gate level.

## 8. Calibration & self-test (first boot + console command)

- Zero-read grounded mux channels C12–C15 (ADC offset).
- VA_SENSE → rail voltage; scale STARVE law and headroom-dependent laws.
- LM13700 Iabc spread: sweep PWM_GATE/PWM_MIXW etc. while watching
  LOCK_SENSE/ENV_ADC where observable; store per-unit trim constants.
- Pot range learn: full CCW/CW prompt over console (stores endpoints).

## 9. Milestones

1. `fw-0.1` bring-up: blink WS2812, read all ADC channels, print over USB.
2. `fw-0.2` CV plumbing: all 9 PWMs sweep, scope-verify RC'd CVs.
3. `fw-0.3` control feel: tapers + smoothing + gate logic.
4. `fw-0.4` gestures + LFOs + matrix + flash persistence.
5. `fw-1.0` A/B against plugin v0.32 with the real 567 core.

Nothing here requires board changes — GP22 spare and mux C12–C15 give
expansion room. Firmware work starts after the PCBWay order ships.
