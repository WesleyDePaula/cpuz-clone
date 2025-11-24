#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include <intrin.h>
#include "cpu_basic.h"

// Obtém o fabricante do processador via instrução CPUID
void get_cpu_vendor(char vendor[13]) {
    int r[4] = {0};
    vendor[0] = '\0';
    __cpuid(r, 0);
    *(int*)&vendor[0] = r[1]; // EBX
    *(int*)&vendor[4] = r[3]; // EDX
    *(int*)&vendor[8] = r[2]; // ECX
    vendor[12] = '\0';
}

// Obtém o nome comercial do processador via CPUID (ex: Intel Core i7-9700K)
void get_cpu_brand(char brand[49]) {
    int r[4]; brand[0] = '\0';
    __cpuid(r, 0x80000000);
    unsigned maxExt = (unsigned)r[0];
    if (maxExt >= 0x80000004) {
        int* out = (int*)brand;
        __cpuid(out + 0, 0x80000002);
        __cpuid(out + 4, 0x80000003);
        __cpuid(out + 8, 0x80000004);
        brand[48] = '\0';
        char* p = brand; while (*p == ' ') ++p;
        if (p != brand) memmove(brand, p, strlen(p) + 1);
    }
}
