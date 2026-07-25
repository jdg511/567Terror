// ============================================================================
//  console.c — USB serial console.
//
//  Written for someone who does not want to read code. Every command explains
//  itself, every error says what to do instead, and `help` is the only thing
//  you have to remember.
// ============================================================================
#include "console.h"
#include "board.h"
#include "hw_io.h"
#include "control.h"
#include "stomps.h"
#include "selftest.h"
#include "flash_store.h"
#include "mod_system.h"
#include "tunables.h"
#include "tapers.h"

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define LINE_MAX 96
static char s_line[LINE_MAX];
static int  s_len = 0;
static bool s_echo = true;

// ---------------------------------------------------------------------------
void gw_console_banner (void)
{
    printf ("\n");
    printf ("  ##########################################################\n");
    printf ("  #   G L I T C H W A V E   5 6 7                          #\n");
    printf ("  #   Illicit Apothecary            pedal firmware " GW_FW_VERSION "   #\n");
    printf ("  ##########################################################\n");
    printf ("\n");
    printf ("  Type  help   for what you can do.\n");
    printf ("  Type  status for what the pedal is doing right now.\n");
    printf ("  Type  dump   to see every setting.\n\n");
}

static void cmd_help (void)
{
    printf (
    "\n"
    "  LOOKING AROUND\n"
    "    status              what every knob, switch and CV is doing right now\n"
    "    adc                 raw ADC counts, every channel\n"
    "    dump                every setting and its value\n"
    "    dump <text>         only settings whose name contains <text>\n"
    "    get <name>          one setting, with its range and what it does\n"
    "    shapes              the LFO waveform list\n"
    "    targets             the modulation destination list\n"
    "    stats               control-loop timing\n"
    "    stats reset          clear the worst-tick and overrun counters\n"
    "\n"
    "  CHANGING THINGS\n"
    "    set <name> <value>  change a setting, takes effect immediately\n"
    "    save                write the current settings into flash\n"
    "    load                go back to the last saved settings\n"
    "    defaults            go back to the shipping values (does not save)\n"
    "    wipe                erase saved settings so next boot is factory\n"
    "\n"
    "  PLAYING WITH IT\n"
    "    bypass on|off       force the bypass state\n"
    "    mode <0-3>          SVF mode: 0 LP, 1 BP, 2 HP, 3 Notch\n"
    "    tap <bpm>           set the LFO rate from a tempo in BPM\n"
    "\n"
    "  BRING-UP AND SERVICE\n"
    "    selftest            full pass/fail check of the board\n"
    "    leds                walk the 6-LED chain to verify its order\n"
    "    sweep <cv> [secs]   slow triangle on one CV, for the scope\n"
    "    cv                  list the CV names and their pins\n"
    "    cal adc             learn the ADC zero from the grounded channels\n"
    "    cal va <volts>      tell it the rail voltage you measured\n"
    "    cal pots            learn each pot's real end stops\n"
    "    pins                the pin map, including the inverted ones\n"
    "\n"
    "  HOUSEKEEPING\n"
    "    echo on|off         local echo, if your terminal double-prints\n"
    "    reboot              restart the firmware\n"
    "    bootsel             reboot into drag-and-drop mode for a new .uf2\n"
    "\n");
}

static void print_one (int i, bool with_help)
{
    const GwTunableInfo* in = &gw_tunable_info[i];
    const float v = gw_tunable_get (&gwt, i);
    switch (in->kind)
    {
        case GW_KIND_BOOL:
            printf ("  %-22s %-10s", in->name, v >= 0.5f ? "on" : "off");
            break;
        case GW_KIND_INT:
            printf ("  %-22s %-10d", in->name, (int) v);
            break;
        default:
            printf ("  %-22s %-10.4g", in->name, (double) v);
            break;
    }
    if (with_help) printf ("  %s", in->help);
    printf ("\n");
}

static void cmd_dump (const char* filter)
{
    int shown = 0;
    printf ("\n");
    for (int i = 0; i < gw_tunable_count; ++i)
    {
        if (filter && *filter && ! strstr (gw_tunable_info[i].name, filter)) continue;
        print_one (i, false);
        ++shown;
    }
    if (! shown) printf ("  nothing matches \"%s\". Try `dump` with no filter.\n", filter);
    else printf ("\n  %d setting%s. `get <name>` explains any of them.\n\n",
                 shown, shown == 1 ? "" : "s");
}

static void cmd_get (const char* name)
{
    const int i = gw_tunable_find (name);
    if (i < 0)
    {
        printf ("  no setting called \"%s\". Try `dump %s` to search.\n", name, name);
        return;
    }
    const GwTunableInfo* in = &gw_tunable_info[i];
    printf ("\n");
    print_one (i, false);
    printf ("    %s\n", in->help);
    printf ("    allowed %g .. %g, ships as %g\n\n",
            (double) in->minv, (double) in->maxv, (double) in->defval);
}

static void cmd_set (const char* name, const char* valstr)
{
    const int i = gw_tunable_find (name);
    if (i < 0)
    {
        printf ("  no setting called \"%s\". Try `dump %s` to search.\n", name, name);
        return;
    }
    const GwTunableInfo* in = &gw_tunable_info[i];

    float v;
    if      (strcmp (valstr, "on")  == 0 || strcmp (valstr, "true")  == 0) v = 1.0f;
    else if (strcmp (valstr, "off") == 0 || strcmp (valstr, "false") == 0) v = 0.0f;
    else
    {
        char* end = NULL;
        v = strtof (valstr, &end);
        if (end == valstr)
        {
            printf ("  \"%s\" isn't a number. %s takes %g .. %g\n",
                    valstr, name, (double) in->minv, (double) in->maxv);
            return;
        }
    }

    // The three *_target settings index a list with reserved holes in it. Let
    // someone pick a hole and the setting would take, then do nothing at all.
    if (strcmp (name, "lfo1_target") == 0 || strcmp (name, "lfo2_target") == 0
        || strcmp (name, "env_target") == 0)
    {
        if (! gw_target_implemented ((int) (v + 0.5f)))
        {
            printf ("  %d isn't a usable destination. `targets` lists the real ones.\n",
                    (int) (v + 0.5f));
            return;
        }
    }

    if (! gw_tunable_set (&gwt, i, v))
    {
        printf ("  %g is outside %s's range (%g .. %g), so nothing changed.\n",
                (double) v, name, (double) in->minv, (double) in->maxv);
        return;
    }
    print_one (i, false);
    printf ("  (live now. `save` to keep it through a power cycle.)\n");
}

static void cmd_status (void)
{
    printf ("\n  KNOBS            raw pos   after mod   what it means\n");
    printf ("    FREQ            %.3f     %.3f       %.2f Hz, cap range %d\n",
            (double) gws.pot[POT_FREQ], (double) gws.knobs.freq,
            (double) gws.freq_hz, gws.freq_range);
    printf ("    GAIN            %.3f     %.3f       x%.1f\n",
            (double) gws.pot[POT_GAIN], (double) gws.knobs.gain, (double) gws.gain_x);
    printf ("    MIX             %.3f     %.3f       wet %.2f / dry %.2f\n",
            (double) gws.pot[POT_MIX], (double) gws.knobs.mix,
            (double) gws.cv[CV_MIXW], (double) gws.cv[CV_MIXD]);
    printf ("    FIZZ            %.3f     %.3f       %.0f Hz\n",
            (double) gws.pot[POT_FIZZ], (double) gws.knobs.fizz, (double) gws.svf_fc_hz);
    printf ("    Q               %.3f     %.3f       Q %.2f\n",
            (double) gws.pot[POT_Q], (double) gws.knobs.q, (double) gws.svf_q);
    printf ("    VOL             %.3f     %.3f       taper %.3f\n",
            (double) gws.pot[POT_VOL], (double) gws.knobs.vol,
            (double) gw_law_vol (gws.knobs.vol));

    printf ("\n  STATE\n");
    printf ("    bypass          %s (crossfade %.2f)\n",
            gws.engaged ? "EFFECT IN" : "bypassed", (double) gws.bypass);
    printf ("    SVF mode        %s\n", GW_SVF_NAME[gws.svf_mode]);
    printf ("    gate            %s, VCA %.3f\n",
            gws.gate_open ? "open" : "closed", (double) gws.gate_env);
    printf ("    starve          %.3f %s\n",
            (double) gws.starve, gws.starving ? "(both stomps held)" : "");
    printf ("    stomps          1:%s  2:%s\n",
            gw_stomp_raw (1) ? "DOWN" : "up", gw_stomp_raw (2) ? "DOWN" : "up");
    printf ("    rail            %.2f V (VA_SENSE %.3f)\n",
            (double) gws.va_volts, (double) gws.va_sense);

    printf ("\n  TRIMS (inside the pedal)\n");
    printf ("    RV1 threshold   %.3f -> %.3f\n", (double) gws.trim_th, (double) gw_trim_threshold (gws.trim_th));
    printf ("    RV2 hold        %.3f -> %.0f ms\n", (double) gws.trim_ho, (double) gw_trim_hold_ms (gws.trim_ho));
    printf ("    RV3 fade        %.3f -> %.0f ms\n", (double) gws.trim_fa, (double) gw_trim_fade_ms (gws.trim_fa));

    printf ("\n  MODULATION\n");
    printf ("    LFO1  %-11s %6.3f Hz  depth %.2f -> %-10s  now %.3f\n",
            gw_shape_names[(int) gwt.lfo1_shape], (double) gwt.lfo1_rate_hz,
            (double) gwt.lfo1_depth, gw_target_names[(int) gwt.lfo1_target],
            (double) gw_mod_lfo1());
    printf ("    LFO2  %-11s %6.3f Hz  depth %.2f -> %-10s  now %+.3f\n",
            gw_shape_names[(int) gwt.lfo2_shape], (double) gwt.lfo2_rate_hz,
            (double) gwt.lfo2_depth, gw_target_names[(int) gwt.lfo2_target],
            (double) gw_mod_lfo2());
    printf ("    ENV   gain x%-5.2f %s -> %-10s  analog %.3f, after gain %.3f\n",
            (double) gwt.env_gain, gwt.env_drive_up ? "up  " : "down",
            gw_target_names[(int) gwt.env_target],
            (double) gws.env_analog, (double) gw_mod_env());
    printf ("    CV1   %-9s %.3f      CV2  %-9s %.3f\n",
            gw_mod_cv1_plugged() ? "plugged" : "empty", (double) gw_mod_cv1(),
            gw_mod_cv2_plugged() ? "plugged" : "empty", (double) gw_mod_cv2());
    printf ("    LOCK_SENSE      %.3f\n", (double) gws.lock_sense);

    printf ("\n  CV OUTPUTS (duty cycle into each RC filter)\n   ");
    for (int i = 0; i < GW_NUM_CV; ++i)
    {
        printf (" %s %.3f", GW_CV_NAME[i], (double) gws.cv[i]);
        if (i == 4) printf ("\n   ");
    }
    printf ("\n\n");
}

static void cmd_adc (void)
{
    printf ("\n  4067 channels\n");
    for (int i = 0; i < GW_NUM_MUX; ++i)
        printf ("    C%-2d %-11s %4u\n", i, GW_MUX_NAME[i], gws.raw[i]);
    printf ("  direct\n");
    printf ("    A1  CV1         %4u\n", gws.raw_cv1);
    printf ("    A2  CV2         %4u\n\n", gws.raw_cv2);
}

static void cmd_pins (void)
{
    printf ("\n  PWM CV outputs\n");
    for (int i = 0; i < GW_NUM_CV; ++i)
        printf ("    GP%-2d  %-7s  duty %.3f\n", GW_CV_PIN[i], GW_CV_NAME[i], (double) gws.cv[i]);
    printf ("\n  Level-shifted selects -- ALL FIVE ARE INVERTED IN HARDWARE\n");
    printf ("    GP%-2d  FREQ_A   (Q2)\n", PIN_FREQ_A);
    printf ("    GP%-2d  FREQ_B   (Q3)\n", PIN_FREQ_B);
    printf ("    GP%-2d  FMODE_A  (Q4)\n", PIN_FMODE_A);
    printf ("    GP%-2d  FMODE_B  (Q5)\n", PIN_FMODE_B);
    printf ("    GP%-2d  FMODE_C  (Q6)\n", PIN_FMODE_C);
    printf ("\n  Other\n");
    printf ("    GP%-2d  WS2812 data (-> 74AHCT1G125 -> 100R -> 6 LEDs)\n", PIN_WS_DATA);
    printf ("    GP%-2d  STOMP1, active low\n", PIN_STOMP1);
    printf ("    GP%-2d  STOMP2, active low\n", PIN_STOMP2);
    printf ("    GP%d-%d MUX_S0..S3\n", PIN_MUX_S0, PIN_MUX_S3);
    printf ("    GP%-2d  spare\n", PIN_SPARE_GP22);
    printf ("    GP%-2d  ADC0 4067 common\n", PIN_ADC_MUXED);
    printf ("    GP%-2d  ADC1 CV1 jack\n", PIN_CV1_ADC);
    printf ("    GP%-2d  ADC2 CV2 jack\n\n", PIN_CV2_ADC);
}

static void cmd_cv_list (void)
{
    printf ("\n  n  name    pin\n");
    for (int i = 0; i < GW_NUM_CV; ++i)
        printf ("  %d  %-7s GP%d\n", i, GW_CV_NAME[i], GW_CV_PIN[i]);
    printf ("\n  `sweep <n> [seconds]` ramps one of them for the scope.\n\n");
}

static void cmd_shapes (void)
{
    printf ("\n  LFO shapes -- same numbers as the plugin\n");
    for (int i = 0; i < GW_NUM_SHAPES; ++i)
    {
        printf ("   %2d %-12s", i, gw_shape_names[i]);
        if ((i % 4) == 3) printf ("\n");
    }
    printf ("\n  For 4 and 5, rate means the noise low-pass cutoff.\n");
    printf ("  For 18..23, rate means how fast the generator wanders.\n\n");
}

static void cmd_targets (void)
{
    printf ("\n  Modulation destinations\n");
    for (int i = 0; i < GW_NUM_TARGETS; ++i)
    {
        if (gw_target_names[i][0] == '-') continue;
        printf ("   %2d %-12s", i, gw_target_names[i]);
        if ((i % 4) == 3) printf ("\n");
    }
    printf ("\n\n");
}

static void cmd_stats (void)
{
    const uint32_t period = (uint32_t) (1000000.0f / (float) gwt.ctrl_rate_hz);
    printf ("\n  control rate     %d Hz  (%lu us per tick)\n",
            (int) gwt.ctrl_rate_hz, (unsigned long) period);
    printf ("  last tick        %lu us\n", (unsigned long) gws.loop_us_last);
    printf ("  worst tick       %lu us  (%.0f%% of budget)\n",
            (unsigned long) gws.loop_us_max,
            100.0 * (double) gws.loop_us_max / (double) period);
    printf ("  overruns         %lu\n", (unsigned long) gws.overruns);
    printf ("  ticks since boot %lu\n", (unsigned long) gws.tick);
    printf ("  flash slots used %d of %d\n",
            gw_store_slots_used(), gw_store_slots_total());
    if (gws.overruns > 0)
        printf ("\n  Overruns mean the loop can't keep up. Lower adc_oversample\n"
                "  or ctrl_rate_hz, or raise mux_settle_us only if you must.\n");
    printf ("\n");
}

// ---------------------------------------------------------------------------
static void dispatch (char* line)
{
    // split into up to 4 words
    char* w[4] = { NULL, NULL, NULL, NULL };
    int   n = 0;
    char* p = line;
    while (*p && n < 4)
    {
        while (*p == ' ' || *p == '\t') ++p;
        if (! *p) break;
        w[n++] = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        if (*p) *p++ = '\0';
    }
    if (n == 0) return;

    const char* c = w[0];

    if (!strcmp (c, "help") || !strcmp (c, "?") || !strcmp (c, "h")) { cmd_help(); return; }
    if (!strcmp (c, "status") || !strcmp (c, "s"))                   { cmd_status(); return; }
    if (!strcmp (c, "adc"))                                          { cmd_adc(); return; }
    if (!strcmp (c, "pins"))                                         { cmd_pins(); return; }
    if (!strcmp (c, "cv"))                                           { cmd_cv_list(); return; }
    if (!strcmp (c, "shapes"))                                       { cmd_shapes(); return; }
    if (!strcmp (c, "targets"))                                      { cmd_targets(); return; }
    if (!strcmp (c, "stats"))
    {
        if (n > 1 && !strcmp (w[1], "reset"))
        {
            gw_control_reset_stats();
            printf ("  worst-tick and overrun counters cleared.\n");
            return;
        }
        cmd_stats();
        return;
    }
    if (!strcmp (c, "dump"))                                         { cmd_dump (n > 1 ? w[1] : NULL); return; }

    if (!strcmp (c, "get"))
    {
        if (n < 2) { printf ("  usage: get <name>\n"); return; }
        cmd_get (w[1]); return;
    }

    if (!strcmp (c, "set"))
    {
        if (n < 3) { printf ("  usage: set <name> <value>     e.g. set led_brightness 0.4\n"); return; }
        cmd_set (w[1], w[2]); return;
    }

    if (!strcmp (c, "save"))
    {
        printf (gw_store_save (&gwt) ? "  saved.\n"
                                     : "  SAVE FAILED -- settings are still live but won't survive a power cycle.\n");
        return;
    }

    // `load` and `defaults` replace the WHOLE settings struct. Build it in a
    // scratch copy and let core 0 swap it in between ticks -- writing it here
    // would let the control loop read a half-updated struct for one tick.
    if (!strcmp (c, "load"))
    {
        GwTunables tmp = gwt;
        if (gw_store_load (&tmp))
        {
            gw_control_request_settings (&tmp);
            printf ("  loaded the saved settings.\n");
        }
        else printf ("  nothing saved yet, so nothing to load.\n");
        return;
    }

    if (!strcmp (c, "defaults"))
    {
        GwTunables tmp;
        gw_tunables_defaults (&tmp);
        gw_control_request_settings (&tmp);
        printf ("  back to the shipping values. Nothing written to flash --\n"
                "  `save` to make it stick, or `load` to change your mind.\n");
        return;
    }

    if (!strcmp (c, "wipe"))
    {
        printf (gw_store_erase() ? "  saved settings erased. Next boot comes up on defaults.\n"
                                 : "  erase failed.\n");
        return;
    }

    if (!strcmp (c, "bypass"))
    {
        if (n < 2) { printf ("  usage: bypass on|off\n"); return; }
        const bool on = !strcmp (w[1], "on") || !strcmp (w[1], "1");
        gw_control_set_engaged (on);
        printf ("  %s\n", on ? "effect in" : "bypassed");
        return;
    }

    if (!strcmp (c, "mode"))
    {
        if (n < 2) { printf ("  usage: mode 0|1|2|3   (LP BP HP Notch)\n"); return; }
        const int m = atoi (w[1]);
        if (m < 0 || m >= GW_NUM_SVF_MODE) { printf ("  0..3 only.\n"); return; }
        gw_control_set_svf_mode (m);
        printf ("  SVF mode %s\n", GW_SVF_NAME[m]);
        return;
    }

    if (!strcmp (c, "tap"))
    {
        if (n < 2) { printf ("  usage: tap <bpm>\n"); return; }
        const float bpm = strtof (w[1], NULL);
        if (bpm < 10.0f || bpm > 3000.0f) { printf ("  10..3000 BPM.\n"); return; }
        const float hz = bpm / 60.0f;
        const int idx = (gwt.tap_target_lfo == 2) ? gw_tunable_find ("lfo2_rate_hz")
                                                  : gw_tunable_find ("lfo1_rate_hz");
        gw_tunable_set (&gwt, idx, hz);
        printf ("  LFO%d = %.3f Hz (%.0f BPM)\n", (int) gwt.tap_target_lfo, (double) hz, (double) bpm);
        return;
    }

    if (!strcmp (c, "selftest")) { gw_selftest_run(); return; }
    if (!strcmp (c, "leds"))     { gw_selftest_leds(); return; }

    if (!strcmp (c, "sweep"))
    {
        if (n < 2) { printf ("  usage: sweep <cv> [seconds]   -- `cv` lists them\n"); return; }
        gw_selftest_sweep_cv (atoi (w[1]), n > 2 ? atoi (w[2]) : 5);
        return;
    }

    if (!strcmp (c, "cal"))
    {
        if (n < 2) { printf ("  usage: cal adc | cal va <volts> | cal pots\n"); return; }
        if (!strcmp (w[1], "adc"))  { gw_cal_adc_zero(); return; }
        if (!strcmp (w[1], "pots")) { gw_cal_pot_endpoints(); return; }
        if (!strcmp (w[1], "va"))
        {
            if (n < 3) { printf ("  usage: cal va <the voltage you measured at VA>\n"); return; }
            gw_cal_va (strtof (w[2], NULL));
            return;
        }
        printf ("  cal adc | cal va <volts> | cal pots\n");
        return;
    }

    if (!strcmp (c, "echo"))
    {
        if (n < 2) { printf ("  usage: echo on|off\n"); return; }
        s_echo = !strcmp (w[1], "on");
        printf ("  echo %s\n", s_echo ? "on" : "off");
        return;
    }

    if (!strcmp (c, "reboot"))
    {
        printf ("  restarting...\n");
        sleep_ms (100);
        watchdog_reboot (0, 0, 10);
        return;
    }

    if (!strcmp (c, "bootsel"))
    {
        printf ("  going into drag-and-drop mode. A drive called RPI-RP2 will appear;\n"
                "  copy the new .uf2 onto it.\n");
        sleep_ms (200);
        reset_usb_boot (0, 0);
        return;
    }

    printf ("  don't know \"%s\". Type `help`.\n", c);
}

// ---------------------------------------------------------------------------
void gw_console_init (void)
{
    s_len = 0;
    s_line[0] = '\0';
}

void gw_console_service (void)
{
    int ch;
    while ((ch = getchar_timeout_us (0)) != PICO_ERROR_TIMEOUT)
    {
        if (ch == '\r' || ch == '\n')
        {
            if (s_echo) printf ("\n");
            s_line[s_len] = '\0';
            if (s_len > 0) dispatch (s_line);
            s_len = 0;
            printf ("gw> ");
            fflush (stdout);
        }
        else if (ch == 8 || ch == 127)               // backspace
        {
            if (s_len > 0) { --s_len; if (s_echo) printf ("\b \b"); }
        }
        else if (ch >= 32 && ch < 127)
        {
            if (s_len < LINE_MAX - 1)
            {
                s_line[s_len++] = (char) ch;
                if (s_echo) putchar (ch);
            }
        }
        fflush (stdout);
    }

    // optional periodic stats
    if (gwt.stats_period_ms > 0)
    {
        static absolute_time_t next = { 0 };
        if (time_reached (next))
        {
            next = make_timeout_time_ms ((uint32_t) gwt.stats_period_ms);
            printf ("[%lu] tick %lu  loop %lu/%lu us  overruns %lu\n",
                    (unsigned long) (time_us_32() / 1000u),
                    (unsigned long) gws.tick,
                    (unsigned long) gws.loop_us_last,
                    (unsigned long) gws.loop_us_max,
                    (unsigned long) gws.overruns);
        }
    }
}
