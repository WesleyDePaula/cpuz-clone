// memory_timings.h - Parâmetros de temporização da memória
// Nota: O Windows não fornece API pública para valores SPD

#pragma once

#include <stdbool.h>
#include <stddef.h>

// Frequência efetiva da memória RAM em MHz (via WMI)
bool get_dram_frequency(char *buf, size_t buf_size);

// Nota: Timings (CAS, tRCD, tRAS) não estão disponíveis via API do Windows
// Para acessar dados SPD seria necessário um driver de kernel
