#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include "cpu/cpu_basic.h"
#include "cpu/cpu_cores.h"
#include "cpu/cpu_cache.h"
#include "cpu/cpu_clock.h"

int main(void) {
    printf("| ------------------ CPU INFO ------------------\n");

    char vendor[13] = {0};
    char brand[49]  = {0};
    get_cpu_vendor(vendor);
    get_cpu_brand(brand);

    DWORD phys = count_physical_cores();
    DWORD logical = count_logical_processors();

    /*
    * Fornecedor
    * Modelo
    * Nº de núcleos físicos
    * Nº de processadores lógicos
    */
    printf("| CPU Vendor             : %s\n", vendor[0] ? vendor : "(indisponível)");
    printf("| CPU Model (brand)      : %s\n", brand[0]  ? brand  : "(indisponível)");
    printf("| Logical Processors     : %lu\n", (unsigned long)logical);
    printf("| Physical Cores         : %lu\n", (unsigned long)phys);

    // Clock do primeiro núcleo (CPU lógico 0)
    DWORD cur=0, max=0, lim=0;
    if (get_cpu0_clock(&cur, &max, &lim)) {
        printf("| CPU0 Current Clock     : %lu MHz\n", (unsigned long)cur);
        printf("| CPU0 Max Clock         : %lu MHz\n", (unsigned long)max);
        printf("| CPU0 Limit (throttle)  : %lu MHz\n", (unsigned long)lim);
    } else {
        printf("| CPU0 Clock             : (nao disponivel)\n");
    }

    // Cache
    print_cache_rows_pretty();
    return 0;
}
