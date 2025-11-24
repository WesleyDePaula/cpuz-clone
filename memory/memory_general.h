// memory_general.h - Informações gerais sobre a memória RAM
// Funções retornam false se não conseguirem obter os dados

#pragma once

#include <stdbool.h>
#include <stddef.h>

// Tipo de memória (DDR3, DDR4, DDR5, etc) via WMI
bool get_memory_type(char *buf, size_t buf_size);

// Quantidade total de memória RAM instalada via API do Windows
bool get_memory_size(char *buf, size_t buf_size);

// Configuração dos canais de memória via WMI
bool get_memory_channels(char *buf, size_t buf_size);
