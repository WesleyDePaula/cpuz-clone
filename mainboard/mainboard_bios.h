// mainboard_bios.h - Funções para obter informações da BIOS
#ifndef MAINBOARD_BIOS_H
#define MAINBOARD_BIOS_H

#include <windows.h>

// Retorna a marca/fabricante da BIOS (ex: "American Megatrends Inc.")
// buffer deve ter pelo menos 128 bytes
BOOL get_bios_brand(char* buffer, size_t bufsize);

// Retorna a versão da BIOS (ex: "F14")
// buffer deve ter pelo menos 128 bytes
BOOL get_bios_version(char* buffer, size_t bufsize);

// Retorna a data da BIOS (ex: "07/19/2021")
// buffer deve ter pelo menos 64 bytes
BOOL get_bios_date(char* buffer, size_t bufsize);

#endif // MAINBOARD_BIOS_H
