// mainboard_chipset.h - Informações de chipset e southbridge
// Usa CPUID, registro do Windows e enumeração de dispositivos PCI
#ifndef MAINBOARD_CHIPSET_H
#define MAINBOARD_CHIPSET_H

#include <windows.h>
#include <wchar.h>

// Estrutura com informações de chipset/southbridge
typedef struct {
    wchar_t vendor[64];    // Fabricante (AMD, Intel, etc)
    wchar_t model[64];     // Modelo (Ryzen SOC, B550, Comet Lake, etc)
    wchar_t revision[16];  // Revisão do hardware
} ChipsetInfo;

// Get chipset info (SoC / primary PCH)
// Source: CPUID vendor + WMI Win32_BaseBoard.Product pattern matching
// Returns: number of entries filled (0 or 1)
size_t get_chipset_info(ChipsetInfo* info, size_t max_entries);

// Get southbridge info (PCH/FCH)
// Source: PCI device enumeration for ISA/LPC bridges, vendor ID mapping
// Returns: number of entries filled (0 or 1)
size_t get_southbridge_info(ChipsetInfo* info, size_t max_entries);

// Build display rows for GUI from chipset/southbridge data
// Returns: number of rows filled
size_t build_chipset_rows(wchar_t labels[][32], wchar_t vendors[][64],
                          wchar_t models[][64], wchar_t revisions[][16],
                          size_t max_rows);

#endif // MAINBOARD_CHIPSET_H
