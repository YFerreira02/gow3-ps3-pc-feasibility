// Runtime defaults for God of War III (BCES00510), set before main() runs.
//
// Starting point: the title-agnostic switches from Ascension
// (gow-ascension-pc, host/ascension_defaults.cpp). Ascension's address-bound
// ones (GCM_ROUND_GATE, GCM_ROUND_RING, SPU_REJOIN, SPURS_MAXCONT by workload
// id) belong to that executable and are left out until GoW III's own values
// are found. A value already set in the environment wins.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <stdlib.h>
#endif

namespace {

void set_default(const char* name, const char* value)
{
#ifdef _WIN32
    if (GetEnvironmentVariableA(name, nullptr, 0)) return;
    SetEnvironmentVariableA(name, value);
#else
    setenv(name, value, 0);
#endif
}

struct Gow3Defaults {
    Gow3Defaults()
    {
        set_default("PS3_EDGE_ZLIB_HLE", "1");     // Edge Zlib on the host
        set_default("RSX_LIVE_DRAW", "1");         // live NV4097 draw engine
        set_default("GCM_PARK_WAIT_MS", "2000");
        set_default("GCM_SEMA_ACQUIRE", "1");
        set_default("GCM_PARK_SPU_HOLD_MS", "1000");
        set_default("GCM_REPORT_SPU_HOLD_MS", "50");
        set_default("SPURS_READY_LEVEL", "1");
        set_default("SPURS_IDLE_BACKOFF_MAX", "0");
        set_default("LD_PERF_OVERLAY", "1");
        set_default("PS3_VERBOSE", "1");           // bring-up: keep the logs
    }
} s_defaults;

}  // namespace

