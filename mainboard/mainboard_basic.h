// mainboard_basic.h - Informações básicas da placa-mãe
#ifndef MAINBOARD_BASIC_H
#define MAINBOARD_BASIC_H

#include <windows.h>

// Fabricante da placa-mãe via WMI
BOOL get_motherboard_manufacturer(char* buffer, size_t bufsize);

// Modelo da placa-mãe via WMI
BOOL get_motherboard_model(char* buffer, size_t bufsize);

// Especificações do barramento PCI-Express via registro do Windows
BOOL get_motherboard_bus_specs(char* buffer, size_t bufsize);

#endif // MAINBOARD_BASIC_H
