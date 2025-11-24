// mainboard_bios.h - Informações da BIOS/UEFI
#ifndef MAINBOARD_BIOS_H
#define MAINBOARD_BIOS_H

#include <windows.h>

// Fabricante da BIOS via WMI
BOOL get_bios_brand(char* buffer, size_t bufsize);

// Versão da BIOS via WMI
BOOL get_bios_version(char* buffer, size_t bufsize);

// Data de lançamento da BIOS via WMI
BOOL get_bios_date(char* buffer, size_t bufsize);

#endif // MAINBOARD_BIOS_H
