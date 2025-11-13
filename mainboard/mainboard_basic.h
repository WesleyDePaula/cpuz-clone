// mainboard_basic.h - Funções para obter informações básicas da motherboard
#ifndef MAINBOARD_BASIC_H
#define MAINBOARD_BASIC_H

#include <windows.h>

// Retorna o fabricante da motherboard (ex: "Gigabyte Technology Co., Ltd.")
// buffer deve ter pelo menos 128 bytes
BOOL get_motherboard_manufacturer(char* buffer, size_t bufsize);

// Retorna o modelo da motherboard (ex: "B550M AORUS ELITE")
// buffer deve ter pelo menos 128 bytes
BOOL get_motherboard_model(char* buffer, size_t bufsize);

// Retorna as especificações de barramento (ex: "PCI-Express 4.0 (16.0 GT/s)")
// buffer deve ter pelo menos 128 bytes
BOOL get_motherboard_bus_specs(char* buffer, size_t bufsize);

#endif // MAINBOARD_BASIC_H
