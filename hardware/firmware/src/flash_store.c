// ============================================================================
//  flash_store.c
// ============================================================================
#include "flash_store.h"

#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include <string.h>

#define STORE_SECTOR_SIZE  FLASH_SECTOR_SIZE          /* 4096 */
#define STORE_RECORD_SIZE  512
#define STORE_SLOTS        (STORE_SECTOR_SIZE / STORE_RECORD_SIZE)   /* 8 */

// Last sector of a 2 MB Pico. PICO_FLASH_SIZE_BYTES comes from the board file.
#define STORE_OFFSET       (PICO_FLASH_SIZE_BYTES - STORE_SECTOR_SIZE)
#define STORE_XIP_BASE     ((const uint8_t*) (XIP_BASE + STORE_OFFSET))

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t payload_bytes;
    uint32_t seq;            // higher wins; 0xFFFFFFFF means "blank"
    uint32_t crc;
} RecHeader;

_Static_assert (sizeof (RecHeader) + sizeof (GwTunables) <= STORE_RECORD_SIZE,
                "GwTunables outgrew the 256-byte flash record. Bump "
                "STORE_RECORD_SIZE to 1024 (and STORE_SLOTS drops to 4).");

static int s_used = 0;

// ---- CRC32 (the standard reflected polynomial), computed bytewise -----------
static uint32_t crc32 (const uint8_t* p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i)
    {
        c ^= p[i];
        for (int k = 0; k < 8; ++k)
            c = (c >> 1) ^ (0xEDB88320u & (uint32_t) (-(int32_t) (c & 1u)));
    }
    return ~c;
}

static const uint8_t* slot_ptr (int slot)
{
    return STORE_XIP_BASE + (size_t) slot * STORE_RECORD_SIZE;
}

static bool slot_valid (int slot, uint32_t* seq_out)
{
    RecHeader h;
    memcpy (&h, slot_ptr (slot), sizeof (h));
    if (h.magic != GW_STORE_MAGIC)      return false;
    if (h.version != GW_STORE_VERSION)  return false;
    if (h.payload_bytes != (uint16_t) sizeof (GwTunables)) return false;
    if (h.seq == 0xFFFFFFFFu)           return false;
    const uint32_t want = crc32 (slot_ptr (slot) + sizeof (RecHeader), h.payload_bytes);
    if (want != h.crc)                  return false;
    if (seq_out) *seq_out = h.seq;
    return true;
}

static bool slot_blank (int slot)
{
    const uint8_t* p = slot_ptr (slot);
    for (int i = 0; i < (int) STORE_RECORD_SIZE; ++i)
        if (p[i] != 0xFF) return false;
    return true;
}

static void recount (void)
{
    s_used = 0;
    for (int i = 0; i < (int) STORE_SLOTS; ++i)
        if (! slot_blank (i)) ++s_used;
}

void gw_store_init (void)
{
    // Lets flash_safe_execute park core 1 while we write.
    flash_safe_execute_core_init();
    recount();
}

bool gw_store_load (GwTunables* t)
{
    int best = -1;
    uint32_t best_seq = 0;
    for (int i = 0; i < (int) STORE_SLOTS; ++i)
    {
        uint32_t seq;
        if (slot_valid (i, &seq) && (best < 0 || seq >= best_seq))
        {
            best = i;
            best_seq = seq;
        }
    }
    if (best < 0) return false;
    memcpy (t, slot_ptr (best) + sizeof (RecHeader), sizeof (*t));

    // Push every field back through the guard rails. The CRC proves the bytes
    // are the ones we wrote; it does NOT prove they are sane -- an out-of-range
    // or NaN value that got in before a bounds check was tightened would
    // otherwise survive in flash forever and bypass the checks completely.
    for (int i = 0; i < gw_tunable_count; ++i)
    {
        const float v = gw_tunable_get (t, i);
        if (! gw_tunable_set (t, i, v))
            gw_tunable_set (t, i, gw_tunable_info[i].defval);
    }
    return true;
}

// ---- the two operations that must run with interrupts off ------------------
typedef struct { uint32_t offset; const uint8_t* data; size_t len; } WriteJob;

static void do_erase (void* param)
{
    (void) param;
    flash_range_erase (STORE_OFFSET, STORE_SECTOR_SIZE);
}

static void do_program (void* param)
{
    const WriteJob* j = (const WriteJob*) param;
    flash_range_program (j->offset, j->data, j->len);
}

bool gw_store_erase (void)
{
    const int rc = flash_safe_execute (do_erase, NULL, 1000);
    recount();
    return rc == PICO_OK;
}

bool gw_store_save (const GwTunables* t)
{
    // Find the highest sequence number currently on the chip.
    uint32_t max_seq = 0;
    for (int i = 0; i < (int) STORE_SLOTS; ++i)
    {
        uint32_t seq;
        if (slot_valid (i, &seq) && seq > max_seq) max_seq = seq;
    }

    // First blank slot. If none, wipe the sector and start over at slot 0.
    int slot = -1;
    for (int i = 0; i < (int) STORE_SLOTS; ++i)
        if (slot_blank (i)) { slot = i; break; }

    if (slot < 0)
    {
        if (! gw_store_erase()) return false;
        slot = 0;
    }

    // Flash programs in 256-byte pages; a 512-byte record is exactly two.
    static uint8_t page[STORE_RECORD_SIZE];
    memset (page, 0xFF, sizeof (page));

    RecHeader h;
    h.magic         = GW_STORE_MAGIC;
    h.version       = GW_STORE_VERSION;
    h.payload_bytes = (uint16_t) sizeof (GwTunables);
    h.seq           = max_seq + 1;
    h.crc           = crc32 ((const uint8_t*) t, sizeof (*t));

    memcpy (page, &h, sizeof (h));
    memcpy (page + sizeof (h), t, sizeof (*t));

    WriteJob job = { (uint32_t) (STORE_OFFSET + (size_t) slot * STORE_RECORD_SIZE),
                     page, STORE_RECORD_SIZE };

    if (flash_safe_execute (do_program, &job, 1000) != PICO_OK) return false;

    recount();

    // Read it back and check it really landed.
    uint32_t seq;
    return slot_valid (slot, &seq) && seq == h.seq;
}

int gw_store_slots_used  (void) { return s_used; }
int gw_store_slots_total (void) { return STORE_SLOTS; }
