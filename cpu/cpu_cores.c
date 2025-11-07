// cpu_cores.c
#include <windows.h>
#include <stdlib.h>
#include "cpu_cores.h"

DWORD count_physical_cores(void) {
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &len);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || len == 0) {
        GetLogicalProcessorInformationEx(RelationAll, NULL, &len);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || len == 0) return 0;
        BYTE* buf = (BYTE*)malloc(len);
        if (!buf) return 0;
        if (!GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf, &len)) {
            free(buf); return 0;
        }
        DWORD cores = 0;
        for (BYTE* p = buf; p < buf + len; ) {
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ex = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)p;
            if (ex->Relationship == RelationProcessorCore) cores++;
            p += ex->Size;
        }
        free(buf); return cores;
    }
    BYTE* buf = (BYTE*)malloc(len);
    if (!buf) return 0;
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf, &len)) {
        free(buf); return 0;
    }
    DWORD cores = 0;
    for (BYTE* p = buf; p < buf + len; ) {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ex = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)p;
        if (ex->Relationship == RelationProcessorCore) cores++;
        p += ex->Size;
    }
    free(buf); return cores;
}

DWORD count_logical_processors(void) {
    WORD groups = GetActiveProcessorGroupCount();
    DWORD total = 0;
    for (WORD g = 0; g < groups; ++g) total += GetActiveProcessorCount(g);
    if (total == 0) { SYSTEM_INFO si; GetSystemInfo(&si); total = si.dwNumberOfProcessors; }
    return total;
}