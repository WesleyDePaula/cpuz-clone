#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include "cpu/cpu_basic.h"
#include "cpu/cpu_cores.h"
#include "cpu/cpu_cache.h"

int main(void) {
    printf("| ------------------ CPU INFO ------------------\n");

    char vendor[13] = {0};
    char brand[49]  = {0};
    get_cpu_vendor(vendor);
    get_cpu_brand(brand);

    DWORD phys = count_physical_cores();
    DWORD logical = count_logical_processors();

    printf("| CPU Vendor             : %s\n", vendor[0] ? vendor : "(indisponível)");
    printf("| CPU Model (brand)      : %s\n", brand[0]  ? brand  : "(indisponível)");
    printf("| Logical Processors     : %lu\n", (unsigned long)logical);
    printf("| Physical Cores         : %lu\n", (unsigned long)phys);

    print_cache_rows_pretty();
    return 0;
}
