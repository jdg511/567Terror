// ============================================================================
//  console.h — the USB serial console. This is Jason's interface to the pedal.
// ============================================================================
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void gw_console_init    (void);
void gw_console_service (void);      // call often from core 1
void gw_console_banner  (void);

#ifdef __cplusplus
}
#endif
