// ============================================================================
//  flash_store.h — settings that survive a power cycle.
//
//  One 4 kB flash sector at the very top of the 2 MB chip, carved into 8
//  records of 512 bytes. A save writes the next blank record; only when all
//  eight are used does the sector get erased. That is 8x fewer erases, which
//  matters if Jason sits there tweaking a value and hitting `save`.
//
//  Flash writes have to run with the XIP cache off and the other core parked,
//  so this goes through the SDK's flash_safe_execute().
// ============================================================================
#pragma once

#include <stdbool.h>
#include "tunables.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GW_STORE_MAGIC   0x47573536u    /* "GW56" */
#define GW_STORE_VERSION 1

void gw_store_init (void);

// Loads the newest valid record into *t. Returns false if there is nothing
// saved yet (in which case *t is left alone — call defaults first).
bool gw_store_load (GwTunables* t);

// Returns false if the write could not be verified.
bool gw_store_save (const GwTunables* t);

// Erase the whole sector, so the next boot comes up on defaults.
bool gw_store_erase (void);

int  gw_store_slots_used (void);
int  gw_store_slots_total (void);

#ifdef __cplusplus
}
#endif
