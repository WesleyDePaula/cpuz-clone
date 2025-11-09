#pragma once
#include <windows.h>
#include <stdbool.h>

// Retorna true em sucesso e preenche MHz do CPU lógico 0
bool get_cpu0_clock(DWORD *current_mhz, DWORD *max_mhz, DWORD *limit_mhz);
