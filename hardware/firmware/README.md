# Glitchwave 567 — pedal firmware

**Illicit Apothecary** · `fw-1.0` · Raspberry Pi Pico (RP2040), module U20

This is the brain that sits inside the pedal. It reads the six knobs, the two CV
jacks, the envelope follower and the two footswitches, and it drives ten
control-voltage outputs, five digital select lines and the six-LED chain. All of
the analog signal path is still analog — the Pico never touches your guitar
signal. It just turns knobs, very precisely, a thousand times a second.

Written against `hardware/FIRMWARE_PLAN.md`, netlist-accurate to commit 1949fc3.

---

## The one file you'd ever edit

`tunables.h`. Nothing else. Every number that decides how the pedal *feels* is
in there with a plain-English description, a shipping default, and guard rails
that refuse a value that would break something.

And you mostly won't even edit that, because **you can change any of it live
over USB while the pedal is playing.**

## Talking to the pedal

Plug a USB cable into the Pico through the open back plate. The pedal shows up
as a serial port. Open it with PuTTY, the Arduino IDE's serial monitor, or on
Windows just `mode` + any terminal — baud rate doesn't matter, it's USB.

You get a `gw>` prompt. Type `help`.

```
gw> status                     what every knob, switch and CV is doing right now
gw> dump                       all 86 settings and their values
gw> dump led                   just the ones with "led" in the name
gw> get vol_taper_base         what one setting does, its range, its default
gw> set led_brightness 0.45    changes it instantly, while you're playing
gw> save                       makes your changes survive a power cycle
gw> defaults                   back to the shipping values
```

`save` writes to flash. It rotates through eight slots before it has to erase,
so you can sit there tweaking and saving without wearing the chip out.

To load new firmware: type `bootsel`, and a drive called **RPI-RP2** appears.
Drag `glitchwave567.uf2` onto it. That's the whole update process. (Holding the
BOOTSEL button while plugging in USB does the same thing.)

## Before the boards arrive — bench mode

You can flash this onto a bare Pico on your desk **today** and watch it work:

```
gw> set bench_mode 1
gw> save
gw> reboot
```

It then fakes the pots and the envelope follower, so you get the boot rainbow,
the six LEDs reacting, and all ten CV pins putting out real PWM you can scope.
Wire a WS2812 strip to GP15 and you can confirm the LED chain before you've
soldered a single pad. Set `bench_mode 0` again once the pedal is built.

## Bringing up the real board

In this order:

```
gw> selftest                   pass/fail on the whole board, in plain English
gw> leds                       walks the 6 LEDs so you can check the chain order
gw> cal adc                    learns the ADC zero from the grounded 4067 channels
gw> cal va 9.12                tell it the voltage you actually measured at VA
gw> cal pots                   walks you through both knob end stops
gw> save
```

`selftest` checks the ADC zero, that the rail is present and plausible, that
every mux channel answers (a channel stuck at a rail is the signature of an open
4067 pin or a cold joint), that both footswitches read released, that the
inverted select bits toggle, that all ten CVs ramp, and that the control loop is
comfortably inside its time budget.

For scoping individual CV lines during bring-up:

```
gw> cv                         lists the ten CVs and their pins
gw> sweep 0 10                 slow triangle on PWM_FREQ for 10 seconds
```

The control loop stands down while a bring-up tool runs, so it can't fight you
for the ADC or overwrite the CV you're watching.

## The four knobs that matter on first power-up

The exact CV-voltage-to-parameter curve of each OTA and VCA isn't known until
the board is on the bench. Four settings bend each one until the knob *feels*
like the plugin — adjust them live, no rebuild:

| setting | bends |
|---|---|
| `dirt_cv_curve` | the GAIN VCA |
| `svff_cv_curve` | the SVF cutoff |
| `svfq_cv_curve` | the SVF resonance |
| `gate_cv_curve` | the GATE×VOL VCA |
| `freq_cv_curve` | the 567 pitch CV |

`1.0` is straight through. Above 1 gives finer control low down, below 1 gives
finer control up top. This is milestone `fw-0.2` work — scope in one hand,
console in the other.

---

## What matches the plugin exactly

Verified numerically against `src/dsp/Glitchwave567.h` and `ModSystem.h` at
v0.32 by `test/verify_laws.c` — 94 checks, all passing:

- **VOL** — `a = (81^x − 1)/80`, the plugin's audio taper on the linear B10k pot.
  10% at half rotation, to five decimal places.
- **FREQ** — 0.2 Hz to 6 kHz, exponential, matching `0.2 × 30000^x`.
- **GAIN** — ×1.1 to ×300 log, matching `1.1 × 272.727^x`.
- **SVF Q** — 0.25 to 8 exponential, matching `0.25 × 32^x`.
- **MIX** — constant-power crossfade; power flat to 7×10⁻⁸ across the sweep.
- **All 24 LFO shapes**, including Lorenz, Rössler, drunk walk, Perlin drift,
  wobble and glitch, ported from `ModSystem.h` with the same coefficients.
- **The normalled CV jacks** — no signal for 3 seconds and the depth VCA opens
  fully, so the LFO just runs at its knob. Same as the sim.
- **Envelope follower** ballistics, gain range ×0.125 to ×40, up/down drive.

**FIZZ is the one deliberate difference.** On the PCB a CV-controlled SVF
replaced the plugin's dual-gang Sallen-Key, so the cutoff range is wider:
20 Hz–8.8 kHz instead of the plugin's 44 Hz–8.8 kHz Hi range. The verifier
prints the plugin's curve alongside for comparison.

## How FREQ actually gets four decades out of one 567

The LM567's oscillator is linear in the OTA's control current, so one timing cap
can't span 0.2 Hz to 6 kHz. FREQ_A/B pick one of four caps (47n / 1µ / 22µ /
470µ) and the OTA sweeps inside that cap's window.

Firmware turns the pot into *the frequency you want* (exponential, in software,
where it belongs), picks the cap whose window contains it, and sends that
frequency's linear position in the window as the CV. Neighbouring windows
overlap by design, and **that overlap is the hysteresis** — inside it either cap
can make the note, so the firmware stays on the one it's already using instead
of chattering between them. Swapping caps clicks, and on this pedal clicks are
part of the voice; oscillating between two caps is not.

## The footswitches

| gesture | does |
|---|---|
| STOMP1 tap | bypass toggle, 10 ms equal-power crossfade |
| STOMP2 taps | tap tempo → the LFO rate |
| STOMP2 hold | cycle the SVF mode: LP → BP → HP → Notch |
| **both held** | **STARVE** — sags the Bazz Fuss rail toward its 5 V floor while held, recovers on release |

"Tap" is a press shorter than `stomp_hold_ms` (400 ms). Debounce is a 5 ms
integrator rather than a timer, so a worn 3PDT still feels decisive.

While STARVE is running, all six LEDs dim with the rail — you can watch the
supply collapse.

## The LEDs

Chain order is fixed by `gen_ctrl.py`: section A, section B, section C, tempo,
bypass, gate. `leds` on the console walks them in that order so you can confirm
the header is wired right.

Boot gives a rainbow sweep. Then: A follows the 567 core (LOCK_SENSE) in
red/violet, B follows the dirt in amber, C shows the filter section coloured by
SVF mode, tempo blinks in cyan at the tap rate, bypass is magenta when the
effect is in, gate is green and follows the VCA. Changing the SVF mode flashes
all three section LEDs in that mode's colour.

Default brightness is deliberately low (`led_brightness 0.28`) — pedalboard
friendly, not a photo shoot. Turn it up if you want.

---

## Building it yourself

You don't need to — a compiled `glitchwave567.uf2` ships alongside this. But if
you want to change a default in `tunables.h` and rebuild:

```
export PICO_SDK_PATH=/path/to/pico-sdk      # tested against SDK 2.1.1
cmake -B build -G Ninja
cmake --build build
```

Result: `build/glitchwave567.uf2`. On Windows, the official Raspberry Pi Pico
extension for VS Code does all of this with one click.

To run the law checks on a PC, no hardware needed:

```
cc -O2 -I. -Isrc -o verify test/verify_laws.c src/tunables.c -lm && ./verify
```

### ⚠ Never add `-ffast-math`

It looks harmless on a 1 kHz control loop and it is not. `-ffast-math` implies
`-ffinite-math-only`, which lets the compiler assume NaN can't happen — it then
**deletes** the NaN guards in `hw_io.c` and `mod_system.c` and **inverts**
`gw_clampf`, so clamping a NaN returns the *upper* bound. The failure mode is a
CV pinned at 100%: full volume with the gate forced open. This was measured in
the disassembly of a real build, not theorised. `CMakeLists.txt` sets
`-fno-finite-math-only` on purpose, and `test/verify_laws.c` has regression
tests that fail if the guard ever stops working.

## Where things live

| file | what it is |
|---|---|
| `tunables.h` | **every setting.** The only file you'd edit. |
| `src/board.h` | the pin map. Describes the copper — don't edit to change behaviour. |
| `src/tapers.h` | the knob laws, mirrored from the plugin |
| `src/mod_system.c` | the LFOs and envelope follower, ported from `ModSystem.h` |
| `src/control.c` | the 1 kHz control loop and the LED state model |
| `src/stomps.c` | debounce, gestures, tap tempo |
| `src/hw_io.c` | PWM CV out, muxed ADC in, WS2812 |
| `src/console.c` | everything you type at `gw>` |
| `src/selftest.c` | `selftest`, `leds`, `sweep`, `cal` |
| `src/flash_store.c` | settings that survive a power cycle |
| `src/main.c` | core 0 = control loop, core 1 = console + LEDs |
| `test/verify_laws.c` | the 94 checks, runnable on a PC |

### The trap in `board.h`

FREQ_A/B and FMODE_A/B/C reach their CMOS chips through NPN level shifters
(Q2–Q6), **and those stages invert.** Every write to those five pins goes
through `gw_write_shifted()`, never `gpio_put()` directly. If you ever add a
sixth select line, use that function.

## Resource budget

Compiled size is 102 KB of the 2 MB flash and 11 KB of the 264 KB RAM, so
there's room for whatever comes next. The control loop's own timing is measured
on the pedal and reported by `stats` — full 12-channel mux scan with 4×
oversampling plus all the modulation maths, against a 1000 µs budget. If it ever
overruns, `stats` says so and tells you which setting to back off.

GP22 is spare, and mux channels C12–C15 are grounded spares the self-test uses
for its ADC zero — expansion room without touching the board.

## Milestone status

| | |
|---|---|
| `fw-0.1` bring-up: LEDs, ADC, USB | ✅ `selftest`, `leds`, `adc` |
| `fw-0.2` CV plumbing, scope-verified | ✅ code done — needs the real board + a scope |
| `fw-0.3` tapers, smoothing, gate | ✅ verified numerically against the plugin |
| `fw-0.4` gestures, LFOs, matrix, flash | ✅ all in |
| `fw-1.0` A/B against plugin v0.32 | ⏳ waiting on hardware |

Everything that can be verified without the physical pedal has been. What's left
is the part only the real board can tell us: the actual CV curves.
