// cpu_cache.h
#pragma once
void print_cache_rows_pretty(void);

void build_cache_string(wchar_t *out, size_t cchOut);

size_t build_cache_rows_kv2(
    wchar_t labels[][32],
    wchar_t sizes[][32],
    wchar_t assoc[][16],
    size_t  maxRows
);