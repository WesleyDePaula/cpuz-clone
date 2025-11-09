#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <powrprof.h>   // CallNtPowerInformation
#include <stdbool.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma comment(lib, "PowrProf.lib")
#endif

typedef struct _PROCESSOR_POWER_INFORMATION {
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;

bool get_cpu0_clock(DWORD *current_mhz, DWORD *max_mhz, DWORD *limit_mhz) {
    if (!current_mhz || !max_mhz || !limit_mhz) return false;

    // Nº total de processadores lógicos (considerando processor groups)
    DWORD nprocs = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (nprocs == 0) {
        // Fallback raro
        SYSTEM_INFO si; GetSystemInfo(&si);
        nprocs = si.dwNumberOfProcessors;
        if (nprocs == 0) return false;
    }

    PROCESSOR_POWER_INFORMATION* ppi = (PROCESSOR_POWER_INFORMATION*)malloc(sizeof(PROCESSOR_POWER_INFORMATION) * nprocs);
    if (!ppi) return false;

    NTSTATUS st = CallNtPowerInformation(
        ProcessorInformation,
        NULL, 0,
        ppi, sizeof(*ppi) * nprocs
    );

    if (st != 0) {  // STATUS_SUCCESS == 0
        free(ppi);
        return false;
    }

    // Núcleo lógico 0
    *current_mhz = ppi[0].CurrentMhz;
    *max_mhz     = ppi[0].MaxMhz;
    *limit_mhz   = ppi[0].MhzLimit;

    free(ppi);
    return true;
}
