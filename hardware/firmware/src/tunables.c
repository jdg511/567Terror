// ============================================================================
//  tunables.c — generates the settings table from the list in tunables.h.
//  There is nothing to configure here. Edit tunables.h instead.
// ============================================================================
#include "tunables.h"
#include <stddef.h>
#include <string.h>

GwTunables gwt;

#define GW_I_F(n, d, lo, hi, h) \
    { #n, GW_KIND_FLOAT, (uint16_t) offsetof (GwTunables, n), (float)(d), (float)(lo), (float)(hi), h },
#define GW_I_I(n, d, lo, hi, h) \
    { #n, GW_KIND_INT,   (uint16_t) offsetof (GwTunables, n), (float)(d), (float)(lo), (float)(hi), h },
#define GW_I_B(n, d, h) \
    { #n, GW_KIND_BOOL,  (uint16_t) offsetof (GwTunables, n), (float)(d), 0.0f, 1.0f, h },

const GwTunableInfo gw_tunable_info[] = {
    GW_TUNABLE_LIST (GW_I_F, GW_I_I, GW_I_B)
};

#undef GW_I_F
#undef GW_I_I
#undef GW_I_B

const int gw_tunable_count = (int) (sizeof (gw_tunable_info) / sizeof (gw_tunable_info[0]));

static void* field_ptr (GwTunables* t, int idx)
{
    return (void*) ((char*) t + gw_tunable_info[idx].offset);
}

static const void* field_cptr (const GwTunables* t, int idx)
{
    return (const void*) ((const char*) t + gw_tunable_info[idx].offset);
}

void gw_tunables_defaults (GwTunables* t)
{
    memset (t, 0, sizeof (*t));
    for (int i = 0; i < gw_tunable_count; ++i)
        gw_tunable_set (t, i, gw_tunable_info[i].defval);
}

int gw_tunable_find (const char* name)
{
    for (int i = 0; i < gw_tunable_count; ++i)
        if (strcmp (gw_tunable_info[i].name, name) == 0)
            return i;
    return -1;
}

float gw_tunable_get (const GwTunables* t, int idx)
{
    if (idx < 0 || idx >= gw_tunable_count) return 0.0f;
    switch (gw_tunable_info[idx].kind)
    {
        case GW_KIND_FLOAT: return *(const float*)   field_cptr (t, idx);
        case GW_KIND_INT:   return (float) *(const int32_t*) field_cptr (t, idx);
        case GW_KIND_BOOL:  return (float) *(const uint8_t*) field_cptr (t, idx);
    }
    return 0.0f;
}

int gw_tunable_set (GwTunables* t, int idx, float v)
{
    if (idx < 0 || idx >= gw_tunable_count) return 0;
    const GwTunableInfo* in = &gw_tunable_info[idx];
    // Written this way round on purpose: `v < min || v > max` is FALSE for NaN,
    // so NaN would sail through the guard rails. `set vol_taper_base nan` is a
    // thing a person can type, and strtof happily parses it.
    if (! (v >= in->minv && v <= in->maxv)) return 0;
    switch (in->kind)
    {
        case GW_KIND_FLOAT: *(float*)   field_ptr (t, idx) = v; break;
        case GW_KIND_INT:   *(int32_t*) field_ptr (t, idx) = (int32_t) (v < 0.0f ? v - 0.5f : v + 0.5f); break;
        case GW_KIND_BOOL:  *(uint8_t*) field_ptr (t, idx) = (v >= 0.5f) ? 1u : 0u; break;
    }
    return 1;
}
