# Glitchwave 567 firmware (fw-0.1 bring-up skeleton)

Vendor: **Illicit Apothecary**. Target: Raspberry Pi Pico module (U20) on
the Glitchwave 567 main board.

## What fw-0.1 does

Bench bring-up only (FIRST_ARTICLE.md Stages 2–3): all 10 PWM CVs settable
over a USB serial console, full ADC scan of the 16-channel mux + CV jacks,
stomp readout, WS2812 chain test with boot rainbow. No tapers, gate logic,
LFOs, or presets yet — that's fw-0.3/0.4 per FIRMWARE_PLAN.md.

## Building (easiest path — no toolchain fiddling)

1. Install VS Code and the official **Raspberry Pi Pico** extension — it
   installs the SDK + compiler for you.
2. "Import Project" → pick this `hardware/firmware/` folder.
3. Click **Compile**. The extension drops `glitchwave567.uf2` in `build/`.
4. Hold BOOTSEL on the Pico, plug USB, drag the `.uf2` onto the RPI-RP2
   drive. Done — the pedal reboots into the firmware.

(Command-line alternative: install pico-sdk, copy its
`external/pico_sdk_import.cmake` here, then
`mkdir build && cd build && cmake .. && make`.)

## Console

Any serial terminal at any baud (USB CDC), or the VS Code serial monitor.
Type `help`. The bench commands used by FIRST_ARTICLE.md:

- `scan` — print all pots/trims/env/rail-sense/CV readings (grounded
  spare channels should read ~0).
- `cv defaults` — mix 50/50, gate open, effect on: the audio-probe state.
- `cv gate 1023`, `cv freq 512`, … — drive any CV directly.
- `fmode 0..7`, `freqrange 0..3` — SVF mode / 567 timing-cap select
  (the inverting level shifters are handled in code — logical values here).
- `led 3 255 0 0` — light one WS2812.

## Files

- `src/pins.h` — GPIO + mux-channel + LED-order map. Mirrors
  `hardware/FIRMWARE_PLAN.md` §1–2, which mirrors the schematic netlist.
  If the board ever changes, change the plan first, then this.
- `src/tunables.h` — every feel-related constant in one place.
- `src/main.c` — 1 kHz control loop, console, scan, WS2812.
- `src/ws2812.pio` — canonical Raspberry Pi PIO driver.
