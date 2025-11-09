// cpu_cache.c
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    UINT8 level;
    PROCESSOR_CACHE_TYPE type;
    DWORD cacheSize;
    DWORD associativity;
    DWORD count;
} CacheAgg;

static const char* cache_label(UINT8 level, PROCESSOR_CACHE_TYPE type) {
    if (level == 1 && type == CacheData)        return "L1 Data";
    if (level == 1 && type == CacheInstruction) return "L1 Inst.";
    if (level == 1 && type == CacheUnified)     return "L1 Unified";
    if (level == 2) return "Level 2";
    if (level == 3) return "Level 3";
    return "Level ?";
}

static void human_kbytes_str(DWORD bytes, char out[32]) {
    double kb = bytes / 1024.0;
    if (kb >= 1024.0) {
        double mb = kb / 1024.0;
        if ((DWORD)(mb * 10) % 10 == 0) sprintf(out, "%u MBytes", (unsigned)(mb + 0.5));
        else                            sprintf(out, "%.1f MBytes", mb);
    } else {
        sprintf(out, "%u KBytes", (unsigned)(kb + 0.5));
    }
}

static int same_key(const CacheAgg* a, const CacheAgg* b) {
    return a->level == b->level &&
           a->type  == b->type  &&
           a->cacheSize == b->cacheSize &&
           a->associativity == b->associativity;
}

static int desired_row(const CacheAgg* a) {
    if (a->type == CacheTrace) return 0;
    if (a->level == 1) return (a->type == CacheData || a->type == CacheInstruction);
    return (a->level == 2 || a->level == 3);
}

static int order_key(const CacheAgg* a, const CacheAgg* b) {
    int wa = 100, wb = 100;
    if (a->level == 1 && a->type == CacheData) wa = 0;
    else if (a->level == 1 && a->type == CacheInstruction) wa = 1;
    else if (a->level == 2) wa = 2;
    else if (a->level == 3) wa = 3;

    if (b->level == 1 && b->type == CacheData) wb = 0;
    else if (b->level == 1 && b->type == CacheInstruction) wb = 1;
    else if (b->level == 2) wb = 2;
    else if (b->level == 3) wb = 3;

    if (wa != wb) return wa - wb;
    if (a->cacheSize != b->cacheSize) return (int)b->cacheSize - (int)a->cacheSize;
    return (int)b->associativity - (int)a->associativity;
}

void print_cache_rows_pretty(void) {
    DWORD len = 0;
    if (!GetLogicalProcessorInformationEx(RelationCache, NULL, &len) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        printf("| Cache: erro tamanho (%lu)\n", GetLastError()); return;
    }
    BYTE* buf = (BYTE*)malloc(len);
    if (!buf) { printf("| Cache: memoria insuficiente\n"); return; }
    if (!GetLogicalProcessorInformationEx(RelationCache, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf, &len)) {
        printf("| Cache: erro consulta (%lu)\n", GetLastError()); free(buf); return;
    }

    CacheAgg rows[64]; size_t nrows = 0;
    for (BYTE* p = buf; p < buf + len; ) {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ex = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)p;
        if (ex->Relationship == RelationCache) {
            const CACHE_RELATIONSHIP* c = &ex->Cache;
            CacheAgg cur = {0};
            cur.level = c->Level;
            cur.type  = c->Type;
            cur.cacheSize = c->CacheSize;
            cur.associativity = c->Associativity ? c->Associativity : 0;
            if (desired_row(&cur)) {
                size_t i;
                for (i = 0; i < nrows; ++i) if (same_key(&rows[i], &cur)) { rows[i].count++; break; }
                if (i == nrows && nrows < sizeof(rows)/sizeof(rows[0])) { cur.count = 1; rows[nrows++] = cur; }
            }
        }
        p += ex->Size;
    }
    free(buf);

    for (size_t i = 0; i + 1 < nrows; ++i)
        for (size_t j = i + 1; j < nrows; ++j)
            if (order_key(&rows[i], &rows[j]) > 0) { CacheAgg t = rows[i]; rows[i] = rows[j]; rows[j] = t; }

    if (nrows == 0) { printf("| Cache: nao encontrada\n"); return; }

    printf("| ----------------------------------------------\n");
    printf("| Cache\n");
    for (size_t i = 0; i < nrows; ++i) {
        char sizeStr[32]; human_kbytes_str(rows[i].cacheSize, sizeStr);
        const char* label = cache_label(rows[i].level, rows[i].type);
        printf("| %-22s : ", label);
        if (rows[i].count > 1) printf("%u x %s", rows[i].count, sizeStr);
        else                   printf("%s", sizeStr);
        if (rows[i].associativity) printf("%+5u-way", rows[i].associativity);
        else                       printf("%-10s", "unknown");
        printf("\n");
    }
}

void build_cache_string(wchar_t *out, size_t cchOut) {
    if (!out || cchOut == 0) return;
    out[0] = L'\0';

    DWORD len = 0;
    if (!GetLogicalProcessorInformationEx(RelationCache, NULL, &len) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        _snwprintf(out, cchOut, L"Cache: erro (%lu)\r\n", GetLastError());
        return;
    }
    BYTE* buf = (BYTE*)malloc(len);
    if (!buf) {
        _snwprintf(out, cchOut, L"Cache: memoria insuficiente\r\n");
        return;
    }
    if (!GetLogicalProcessorInformationEx(RelationCache,
        (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf, &len)) {
        _snwprintf(out, cchOut, L"Cache: erro consulta (%lu)\r\n", GetLastError());
        free(buf);
        return;
    }

    CacheAgg rows[64]; size_t nrows = 0;
    for (BYTE* p = buf; p < buf + len; ) {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ex = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)p;
        if (ex->Relationship == RelationCache) {
            const CACHE_RELATIONSHIP* c = &ex->Cache;
            CacheAgg cur = {0};
            cur.level = c->Level;
            cur.type  = c->Type;
            cur.cacheSize = c->CacheSize;
            cur.associativity = c->Associativity ? c->Associativity : 0;

            if (desired_row(&cur)) {
                size_t i;
                for (i = 0; i < nrows; ++i) if (same_key(&rows[i], &cur)) { rows[i].count++; break; }
                if (i == nrows && nrows < sizeof(rows)/sizeof(rows[0])) { cur.count = 1; rows[nrows++] = cur; }
            }
        }
        p += ex->Size;
    }
    free(buf);

    for (size_t i = 0; i + 1 < nrows; ++i)
        for (size_t j = i + 1; j < nrows; ++j)
            if (order_key(&rows[i], &rows[j]) > 0) { CacheAgg t = rows[i]; rows[i] = rows[j]; rows[j] = t; }

    if (nrows == 0) {
        _snwprintf(out, cchOut, L"Cache: nao encontrada\r\n");
        return;
    }

    wchar_t line[128];
    _snwprintf(out, cchOut, L"Cache\r\n");
    for (size_t i = 0; i < nrows; ++i) {
        char sizeStrA[32]; human_kbytes_str(rows[i].cacheSize, sizeStrA);
        wchar_t sizeStr[32]; mbstowcs(sizeStr, sizeStrA, 32);
        const char* labelA = cache_label(rows[i].level, rows[i].type);
        wchar_t label[32]; mbstowcs(label, labelA, 32);

        if (rows[i].associativity) {
            if (rows[i].count > 1)
                _snwprintf(line, 128, L"%-10ls  %u x %ls      %u-way\r\n",
                           label, rows[i].count, sizeStr, rows[i].associativity);
            else
                _snwprintf(line, 128, L"%-10ls  %ls      %u-way\r\n",
                           label, sizeStr, rows[i].associativity);
        } else {
            if (rows[i].count > 1)
                _snwprintf(line, 128, L"%-10ls  %u x %ls      unknown\r\n",
                           label, rows[i].count, sizeStr);
            else
                _snwprintf(line, 128, L"%-10ls  %ls      unknown\r\n",
                           label, sizeStr);
        }
        wcsncat(out, line, cchOut - wcslen(out) - 1);
    }
}

size_t build_cache_rows_kv2(
    wchar_t labels[][32],
    wchar_t sizes[][32],
    wchar_t assoc[][16],
    size_t  maxRows
) {
    if (!labels || !sizes || !assoc || maxRows == 0) return 0;

    // ----- monta rows[] como você já faz -----
    DWORD len = 0;
    if (!GetLogicalProcessorInformationEx(RelationCache, NULL, &len) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return 0;
    }
    BYTE* buf = (BYTE*)malloc(len);
    if (!buf) return 0;
    if (!GetLogicalProcessorInformationEx(RelationCache,
        (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf, &len)) {
        free(buf); return 0;
    }

    CacheAgg rows[64]; size_t nrows = 0;
    for (BYTE* p = buf; p < buf + len; ) {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ex =
            (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)p;
        if (ex->Relationship == RelationCache) {
            const CACHE_RELATIONSHIP* c = &ex->Cache;
            CacheAgg cur = {0};
            cur.level = c->Level;
            cur.type  = c->Type;
            cur.cacheSize = c->CacheSize;
            cur.associativity = c->Associativity ? c->Associativity : 0;
            if (desired_row(&cur)) {
                size_t i;
                for (i = 0; i < nrows; ++i) if (same_key(&rows[i], &cur)) { rows[i].count++; break; }
                if (i == nrows && nrows < sizeof(rows)/sizeof(rows[0])) { cur.count = 1; rows[nrows++] = cur; }
            }
        }
        p += ex->Size;
    }
    free(buf);

    for (size_t i = 0; i + 1 < nrows; ++i)
        for (size_t j = i + 1; j < nrows; ++j)
            if (order_key(&rows[i], &rows[j]) > 0) { CacheAgg t = rows[i]; rows[i] = rows[j]; rows[j] = t; }

    // ----- preenche label / size / assoc separadamente -----
    size_t outCount = 0;
    for (size_t i = 0; i < nrows && outCount < maxRows; ++i) {
        // label
        const char* la = cache_label(rows[i].level, rows[i].type);
        wchar_t lw[32]; mbstowcs(lw, la, 31); lw[31]=L'\0';
        wcsncpy(labels[outCount], lw, 31); labels[outCount][31]=L'\0';

        // size (ex.: "6 x 32 KBytes" ou "32 MBytes")
        char sizeA[32]; human_kbytes_str(rows[i].cacheSize, sizeA);
        wchar_t sizeW[32]; mbstowcs(sizeW, sizeA, 31); sizeW[31]=L'\0';

        if (rows[i].count > 1)
            _snwprintf(sizes[outCount], 32, L"%u x %ls", rows[i].count, sizeW);
        else
            _snwprintf(sizes[outCount], 32, L"%ls", sizeW);

        // associatividade (ex.: "8-way" ou "unknown")
        if (rows[i].associativity)
            _snwprintf(assoc[outCount], 16, L"%u-way", rows[i].associativity);
        else
            _snwprintf(assoc[outCount], 16, L"unknown");

        outCount++;
    }
    return outCount;
}

