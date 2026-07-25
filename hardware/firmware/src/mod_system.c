// ============================================================================
//  mod_system.c — faithful C port of the plugin's ModSystem.
// ============================================================================
#include "mod_system.h"
#include "tunables.h"
#include "tapers.h"

#include <math.h>
#include <string.h>
#include <stdint.h>

// See hw_io.c: a fold-proof non-finite test. The Lorenz and Rossler integrators
// are explicit Euler, so at high rates they CAN escape to Inf and then to NaN
// inside a single call. isfinite() alone was not enough -- a NaN state would
// stick forever and pin the LFO at its positive extreme until power-cycle.
static inline bool gw_bad (float f)
{
    uint32_t u;
    memcpy (&u, &f, sizeof (u));
    return (u & 0x7F800000u) == 0x7F800000u;
}

const char* const gw_shape_names[GW_NUM_SHAPES] = {
    "Sine", "Triangle", "Square", "S&H", "WhiteNoise", "PinkNoise",
    "RampUp", "RampDown", "Sweep", "Lumps", "RampOct", "QuadRamp",
    "QuadPulse", "TriStep", "SineOct", "Sine3rd", "Sine4th", "RandSlopes",
    "Lorenz", "Rossler", "DrunkWalk", "PerlinDrift", "Wobble", "Glitch"
};

// A "-" name means the slot exists only to keep the plugin's numbering stable;
// it is NOT offered on the console and cannot be selected. LFO2's rate and
// depth are among these: LFO2 is generated before LFO1 and the envelope, so
// nothing downstream can reach back and modulate it without a second pass.
const char* const gw_target_names[GW_NUM_TARGETS] = {
    "Off", "FREQ", "FIZZ", "MIX", "VOL", "-",
    "LFO1rate", "LFO1depth", "-", "-", "EnvAmount",
    "-", "-", "SVF_Q", "-", "GAIN", "EnvLevel"
};

bool gw_target_implemented (int t)
{
    if (t < 0 || t >= GW_NUM_TARGETS) return false;
    return gw_target_names[t][0] != '-';
}

// ---------------------------------------------------------------- state ----
typedef struct { float lp, p0, p1, p2; } NoiseState;

typedef struct {
    float ax, ay, az;          // Lorenz / Rossler coordinates
    bool  seeded;
    float dv, dpos;            // drunk walk
    float pph[3], pa[3], pb[3];// Perlin-style drift, 3 octaves
    float wEnv, wTgt;          // wobble swell
    float gCalm, gVal, gBurst; // glitch
} LfoState;

static float      s_inEnv, s_cv1Env, s_cv2Env;
static float      s_cv1Silent, s_cv2Silent;
static float      s_lfo1Phase, s_lfo2Phase;
static float      s_sh1, s_sh2, s_shp1, s_shp2;
static NoiseState s_noise1, s_noise2;
static LfoState   s_st1, s_st2;
static uint32_t   s_rng;
static float      s_visLfo1, s_visLfo2, s_visEnv;

void gw_mod_reset (void)
{
    s_inEnv = s_cv1Env = s_cv2Env = 0.0f;
    s_cv1Silent = s_cv2Silent = 1000.0f;      // start "unplugged" -> VCAs open
    s_lfo1Phase = s_lfo2Phase = 0.0f;
    s_sh1 = s_sh2 = s_shp1 = s_shp2 = 0.0f;
    memset (&s_noise1, 0, sizeof (s_noise1));
    memset (&s_noise2, 0, sizeof (s_noise2));
    memset (&s_st1, 0, sizeof (s_st1));
    memset (&s_st2, 0, sizeof (s_st2));
    s_st1.wEnv = s_st1.wTgt = 0.6f;
    s_st2.wEnv = s_st2.wTgt = 0.6f;
    s_rng = 0x1234ABCDu;
    s_visLfo1 = s_visLfo2 = s_visEnv = 0.0f;
}

void gw_mod_init (void) { gw_mod_reset(); }

void gw_mod_retrigger_lfo1 (void)
{
    memset (&s_st1, 0, sizeof (s_st1));  s_st1.wEnv = s_st1.wTgt = 0.6f;
    memset (&s_noise1, 0, sizeof (s_noise1));
}

void gw_mod_retrigger_lfo2 (void)
{
    memset (&s_st2, 0, sizeof (s_st2));  s_st2.wEnv = s_st2.wTgt = 0.6f;
    memset (&s_noise2, 0, sizeof (s_noise2));
}

// ------------------------------------------------------------- primitives --
static inline float whiteNoise (void)
{
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return (float) (int32_t) s_rng * (1.0f / 2147483648.0f);
}

static inline float rand01 (void)      { return 0.5f * (whiteNoise() + 1.0f); }
static inline float fracf  (float x)   { return x - floorf (x); }
static inline float wrapPhase (float p){ return p - floorf (p); }
static inline float sineOf (float ph)  { return sinf (ph * 6.283185307f); }

// ---- the periodic shapes, traced from the TAPLFO 3D datasheet diagram -----
static float lfoValue (int shape, float phase, float sh, float shPrev)
{
    switch (shape)
    {
        case GW_SHAPE_SINE:       return sineOf (phase);
        case GW_SHAPE_TRIANGLE:   return phase < 0.5f ? 4.0f * phase - 1.0f
                                                      : 3.0f - 4.0f * phase;
        case GW_SHAPE_SQUARE:     return phase < 0.5f ? 1.0f : -1.0f;
        case GW_SHAPE_SAMPLEHOLD: return sh;
        case GW_SHAPE_RAMPUP:     return 2.0f * phase - 1.0f;
        case GW_SHAPE_RAMPDOWN:   return 1.0f - 2.0f * phase;

        case GW_SHAPE_SWEEP:      return cosf (phase * 6.283185307f);

        case GW_SHAPE_LUMPS:      return 2.0f * fabsf (sinf (phase * 3.141592653f)) - 1.0f;

        case GW_SHAPE_RAMPOCT:    return ((2.0f * phase - 1.0f)
                                        + 0.5f * (2.0f * fracf (phase * 2.0f) - 1.0f)) / 1.5f;

        case GW_SHAPE_QUADRAMP:   return phase < 0.5f ? (1.0f - 2.0f * fracf (phase * 8.0f))
                                                      : -1.0f;

        case GW_SHAPE_QUADPULSE:  return phase < 0.5f ? (fracf (phase * 8.0f) < 0.5f ? 1.0f : -1.0f)
                                                      : -1.0f;

        case GW_SHAPE_TRISTEP: {
            const float t = phase < 0.5f ? 2.0f * phase : 2.0f - 2.0f * phase;
            int q = (int) (t * 4.0f);
            if (q > 3) q = 3;
            return (float) q * (2.0f / 3.0f) - 1.0f;
        }

        case GW_SHAPE_SINEOCT:    return (sinf (phase * 6.283185307f)
                                        + 0.5f * sinf (phase * 12.56637061f)) / 1.2990381f;

        case GW_SHAPE_SINE3RD:    return (sinf (phase * 6.283185307f)
                                        + (1.0f / 3.0f) * sinf (phase * 18.84955592f)) / 0.9428090f;

        case GW_SHAPE_SINE4TH:    return (sinf (phase * 6.283185307f)
                                        + 0.25f * sinf (phase * 25.13274123f)) / 1.1888206f;

        case GW_SHAPE_RANDSLOPES: return shPrev + (sh - shPrev) * phase;

        default:                  return sh;
    }
}

static float noiseSample (NoiseState* ns, bool pink, float cutoffHz, float dt)
{
    const float w = whiteNoise();
    float x = w;
    if (pink)
    {
        ns->p0 = 0.99765f * ns->p0 + w * 0.0990460f;
        ns->p1 = 0.96300f * ns->p1 + w * 0.2965164f;
        ns->p2 = 0.57000f * ns->p2 + w * 1.0526913f;
        x = (ns->p0 + ns->p1 + ns->p2 + w * 0.1848f) * 0.25f;
    }
    float fc = cutoffHz;
    const float nyq = 0.45f / dt;
    if (fc > nyq) fc = nyq;
    const float c = 1.0f - expf (-6.2831853f * fc * dt);
    ns->lp += c * (x - ns->lp);
    float ratio = (2.0f - c) / (c > 1e-9f ? c : 1e-9f);
    if (ratio < 1.0f) ratio = 1.0f;
    return gw_clampf (ns->lp * sqrtf (ratio) * 0.7f, -1.0f, 1.0f);
}

// One LFO output sample. Periodic shapes ride `phase`; 18..23 are stateful,
// where "rate" means how fast the generator wanders rather than a frequency.
static float generate (int shape, float rateHz, float dt,
                       float* phase, float* sh, float* shp,
                       NoiseState* ns, LfoState* ls)
{
    switch (shape)
    {
        case GW_SHAPE_WHITENOISE:
        case GW_SHAPE_PINKNOISE:
            return noiseSample (ns, shape == GW_SHAPE_PINKNOISE, rateHz, dt);

        case GW_SHAPE_LORENZ: {
            if (! ls->seeded)
            {
                ls->ax = 0.1f + 0.05f * whiteNoise(); ls->ay = 0.0f; ls->az = 25.0f;
                ls->seeded = true;
            }
            float h = 0.75f * rateHz * dt;             // ~1 orbit per 1/rate sec
            int n = 1 + (int) (h / 0.01f);
            if (n > 64) n = 64;                        // keep the tick bounded
            h /= (float) n;
            for (int i = 0; i < n; ++i)
            {
                const float dx = 10.0f * (ls->ay - ls->ax);
                const float dy = ls->ax * (28.0f - ls->az) - ls->ay;
                const float dz = ls->ax * ls->ay - (8.0f / 3.0f) * ls->az;
                ls->ax += h * dx; ls->ay += h * dy; ls->az += h * dz;
            }
            if (gw_bad (ls->ax) || gw_bad (ls->ay) || gw_bad (ls->az)
                || fabsf (ls->ax) > 1000.0f)
            {
                ls->seeded = false;                // re-seed on the next sample
                return 0.0f;                       // and don't hand out the NaN
            }
            return gw_clampf (ls->ax / 20.0f, -1.0f, 1.0f);
        }

        case GW_SHAPE_ROSSLER: {
            if (! ls->seeded)
            {
                ls->ax = 1.0f + 0.1f * whiteNoise(); ls->ay = 0.0f; ls->az = 0.0f;
                ls->seeded = true;
            }
            float h = 5.9f * rateHz * dt;              // ~1 spiral per 1/rate sec
            int n = 1 + (int) (h / 0.05f);
            if (n > 64) n = 64;
            h /= (float) n;
            for (int i = 0; i < n; ++i)
            {
                const float dx = -ls->ay - ls->az;
                const float dy = ls->ax + 0.2f * ls->ay;
                const float dz = 0.2f + ls->az * (ls->ax - 5.7f);
                ls->ax += h * dx; ls->ay += h * dy; ls->az += h * dz;
            }
            if (gw_bad (ls->ax) || gw_bad (ls->ay) || gw_bad (ls->az)
                || fabsf (ls->ax) > 1000.0f)
            {
                ls->seeded = false;
                return 0.0f;
            }
            return gw_clampf (ls->ax / 11.0f, -1.0f, 1.0f);
        }

        case GW_SHAPE_DRUNKWALK: {
            const float a = gw_clampf (rateHz * dt * 4.0f, 0.0f, 0.5f);
            ls->dv += whiteNoise() * a;
            ls->dv *= 1.0f - 0.5f * a;
            ls->dpos += ls->dv * gw_clampf (rateHz * dt * 6.0f, 0.0f, 0.5f);
            if (ls->dpos >  1.0f) { ls->dpos =  2.0f - ls->dpos; ls->dv = -fabsf (ls->dv); }
            if (ls->dpos < -1.0f) { ls->dpos = -2.0f - ls->dpos; ls->dv =  fabsf (ls->dv); }
            return ls->dpos;
        }

        case GW_SHAPE_PERLINDRIFT: {
            float sum = 0.0f, amp = 1.0f, tot = 0.0f, f = rateHz;
            for (int k = 0; k < 3; ++k)
            {
                ls->pph[k] += f * dt;
                if (ls->pph[k] >= 1.0f)
                {
                    ls->pph[k] -= floorf (ls->pph[k]);
                    ls->pa[k] = ls->pb[k];
                    ls->pb[k] = whiteNoise();
                }
                const float t = ls->pph[k];
                const float s = 0.5f - 0.5f * cosf (3.1415927f * t);
                sum += (ls->pa[k] + (ls->pb[k] - ls->pa[k]) * s) * amp;
                tot += amp;
                f *= 2.1f; amp *= 0.5f;
            }
            return gw_clampf (sum / (tot * 0.75f), -1.0f, 1.0f);
        }

        case GW_SHAPE_GLITCH: {
            if (ls->gBurst > 0.0f)
            {
                ls->gBurst -= dt;
                if (rand01() < dt * 250.0f)          // a hard jump every ~4 ms
                    ls->gVal = whiteNoise();
            }
            else
            {
                if (rand01() < rateHz * 0.4f * dt)   // flurries ~0.4 per cycle
                    ls->gBurst = 0.08f + 0.17f * rand01();
                ls->gCalm += (whiteNoise() - ls->gCalm)
                           * gw_clampf (dt * rateHz * 0.25f, 0.0f, 0.1f);
                ls->gVal  += (0.1f * ls->gCalm - ls->gVal)
                           * gw_clampf (dt * rateHz * 2.0f, 0.0f, 0.5f);
            }
            return gw_clampf (ls->gVal, -1.0f, 1.0f);
        }

        default: break;                              // periodic shapes fall through
    }

    const float prev = *phase;
    *phase = wrapPhase (*phase + rateHz * dt);
    if (*phase < prev)
    {
        *shp = *sh;                                  // RandSlopes needs the previous
        *sh  = whiteNoise();
        if (shape == GW_SHAPE_WOBBLE)
            ls->wTgt = 0.15f + 0.85f * rand01();     // a new swell each cycle
    }

    if (shape == GW_SHAPE_WOBBLE)
    {
        ls->wEnv += (ls->wTgt - ls->wEnv) * gw_clampf (rateHz * dt * 2.0f, 0.0f, 0.5f);
        return sineOf (*phase) * ls->wEnv;
    }

    return lfoValue (shape, *phase, *sh, *shp);
}

// --------------------------------------------------------------- knob apply --
static void applyToKnob (GwKnobs* k, int t, float off)
{
    switch (t)
    {
        case GW_TGT_FREQ: k->freq = gw_clampf (k->freq + off, 0.0f, 1.0f); break;
        case GW_TGT_FIZZ: k->fizz = gw_clampf (k->fizz + off, 0.0f, 1.0f); break;
        case GW_TGT_MIX:  k->mix  = gw_clampf (k->mix  + off, 0.0f, 1.0f); break;
        case GW_TGT_VOL:  k->vol  = gw_clampf (k->vol  + off, 0.0f, 1.0f); break;
        case GW_TGT_SVFQ: k->q    = gw_clampf (k->q    + off, 0.0f, 1.0f); break;
        case GW_TGT_GAIN: k->gain = gw_clampf (k->gain + off, 0.0f, 1.0f); break;
        default: break;
    }
}

// ------------------------------------------------------------------- tick ---
void gw_mod_tick (float env_analog, float cv1, float cv2, float rate_hz)
{
    // The board's env follower already has the Mu-Tron 4/150 ms ballistics and
    // the CV jacks already have analog rectify+slew, so firmware only adds the
    // little bit of extra smoothing the tunables ask for.
    const float cvC = gw_onepole_coeff (gwt.cv_smooth_ms, rate_hz);
    s_cv1Env += cvC * (gw_clampf (cv1, 0.0f, 1.0f) - s_cv1Env);
    s_cv2Env += cvC * (gw_clampf (cv2, 0.0f, 1.0f) - s_cv2Env);

    if (gwt.env_extra_smooth_ms > 0.01f)
    {
        const float ec = gw_onepole_coeff (gwt.env_extra_smooth_ms, rate_hz);
        s_inEnv += ec * (gw_clampf (env_analog, 0.0f, 1.0f) - s_inEnv);
    }
    else
    {
        s_inEnv = gw_clampf (env_analog, 0.0f, 1.0f);
    }
}

// ---------------------------------------------------------------- compute ---
GwKnobs gw_mod_compute (const GwKnobs* base, float dt)
{
    GwKnobs out = *base;
    if (dt <= 0.0f) return out;

    // ---- normalled-jack detect + the two depth VCAs ------------------------
    if (s_cv1Env > gwt.cv_detect_level) s_cv1Silent = 0.0f; else s_cv1Silent += dt;
    if (s_cv2Env > gwt.cv_detect_level) s_cv2Silent = 0.0f; else s_cv2Silent += dt;

    const float cv1Vca = (s_cv1Silent < gwt.cv_unplug_sec)
                       ? gw_clampf (s_cv1Env * gwt.cv_gain, 0.0f, 1.0f) : 1.0f;
    const float cv2Vca = (s_cv2Silent < gwt.cv_unplug_sec)
                       ? gw_clampf (s_cv2Env * gwt.cv_gain, 0.0f, 1.0f) : 1.0f;

    float lfo1DepthEff = gwt.lfo1_depth;
    float envGainEff   = gwt.env_gain;

    // ---- LFO 2 (bipolar) ---------------------------------------------------
    // Clamped to Nyquist for the control rate, same as LFO1 below -- otherwise
    // dropping ctrl_rate_hz to 200 lets a 200 Hz LFO2 alias into a slow wobble.
    const float lfo2Rate = gw_clampf (gwt.lfo2_rate_hz, 0.01f, 0.45f / dt);
    const float lfo2Raw = generate ((int) gwt.lfo2_shape, lfo2Rate, dt,
                                    &s_lfo2Phase, &s_sh2, &s_shp2, &s_noise2, &s_st2);
    s_visLfo2 = lfo2Raw;
    const float lfo2Sig = lfo2Raw * gwt.lfo2_depth * cv2Vca;

    float lfo1RateFactor = 1.0f;
    switch ((int) gwt.lfo2_target)
    {
        case GW_TGT_LFO1RATE:
            lfo1RateFactor = exp2f (lfo2Sig * 2.0f); break;
        case GW_TGT_LFO1DEPTH:
            lfo1DepthEff = gw_clampf (lfo1DepthEff + lfo2Sig, 0.0f, 1.0f); break;
        case GW_TGT_ENVAMOUNT:
            envGainEff = gw_clampf (envGainEff * exp2f (lfo2Sig * 2.0f), 0.125f, 40.0f); break;
        default:
            applyToKnob (&out, (int) gwt.lfo2_target, lfo2Sig * 0.5f); break;
    }

    // ---- envelope pre-pass (env can steer LFO1 before LFO1 runs) -----------
    const float envPre      = gw_clampf (s_inEnv * envGainEff, 0.0f, 1.0f);
    const float envApplyPre = gwt.env_drive_up ? envPre : -envPre;
    switch ((int) gwt.env_target)
    {
        case GW_TGT_LFO1RATE:
            lfo1RateFactor *= exp2f (envApplyPre * 2.0f); break;
        case GW_TGT_LFO1DEPTH:
            lfo1DepthEff = gw_clampf (lfo1DepthEff + envApplyPre, 0.0f, 1.0f); break;
        default: break;
    }

    // ---- LFO 1 (unipolar-up) ----------------------------------------------
    const float lfo1RateBent = gw_clampf (gwt.lfo1_rate_hz * lfo1RateFactor,
                                          0.01f, 0.45f / dt);
    const float lfo1Raw = generate ((int) gwt.lfo1_shape, lfo1RateBent, dt,
                                    &s_lfo1Phase, &s_sh1, &s_shp1, &s_noise1, &s_st1);
    const float lfo1Out = 0.5f * (lfo1Raw + 1.0f);
    s_visLfo1 = lfo1Out;

    float envLevelMul = 1.0f;
    const float lfo1Sig = lfo1Out * lfo1DepthEff * cv1Vca;
    switch ((int) gwt.lfo1_target)
    {
        case GW_TGT_ENVAMOUNT:
            envGainEff = gw_clampf (envGainEff * exp2f (lfo1Sig * 2.0f), 0.125f, 40.0f); break;
        case GW_TGT_ENVLEVEL:
            envLevelMul = 1.0f + 2.0f * lfo1Sig; break;      // up to x3
        default:
            applyToKnob (&out, (int) gwt.lfo1_target, lfo1Sig * 0.5f); break;
    }

    // ---- envelope follower -> its own target ------------------------------
    const float envSig = gw_clampf (s_inEnv * envGainEff * envLevelMul, 0.0f, 1.0f);
    s_visEnv = envSig;
    const int et = (int) gwt.env_target;
    if (et != GW_TGT_LFO1RATE && et != GW_TGT_LFO1DEPTH)
        applyToKnob (&out, et, gwt.env_drive_up ? envSig : -envSig);

    return out;
}

float gw_mod_lfo1 (void) { return s_visLfo1; }
float gw_mod_lfo2 (void) { return s_visLfo2; }
float gw_mod_env  (void) { return s_visEnv; }
float gw_mod_cv1  (void) { return s_cv1Env; }
float gw_mod_cv2  (void) { return s_cv2Env; }
bool  gw_mod_cv1_plugged (void) { return s_cv1Silent < gwt.cv_unplug_sec; }
bool  gw_mod_cv2_plugged (void) { return s_cv2Silent < gwt.cv_unplug_sec; }
