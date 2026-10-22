/* MinGW: route every getenv in the runtime through boot_main.cpp's
 * process-wide cache (ps3_cached_getenv). The UCRT getenv takes the
 * environment lock and scans every variable on each call; the runtime polls
 * hundreds of diagnostic switches on hot paths, and with six SPU threads
 * doing so the lock became a convoy (minidump 2026-10-21: every SPU worker
 * queued on it). Under MSVC boot_main overrides the import slot instead;
 * MinGW's import library already defines that slot, hence the macro. */
#pragma once
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
char* __cdecl ps3_cached_getenv(const char* name);
#ifdef __cplusplus
}
#endif
#define getenv ps3_cached_getenv
