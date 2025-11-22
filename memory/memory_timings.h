// memory_timings.h - declarações para informação de temporização de memória
//
// Este módulo expõe funções para recuperar (quando possível)
// parâmetros de temporização de memória, como frequência DRAM,
// latência CAS, atrasos e taxa de comando. No Windows, a API
// pública não fornece acesso direto aos valores SPD, portanto a
// maioria destas funções retornará false e o valor deve ser
// considerado desconhecido.

#pragma once

#include <stdbool.h>
#include <stddef.h>

// Frequência efetiva da memória DRAM (não o clock do controlador).
// Exemplo: "1064.7 MHz". Retorna false se não for possível
// determinar a frequência.
bool get_dram_frequency(char *buf, size_t buf_size);

// Latência CAS (CL) em ciclos de clock. Exemplo: "15.0 clocks".
// Retorna false se a informação não estiver disponível.
// (OBS) Outras funções de temporização não são expostas neste
// header porque a maioria não está disponível em user-mode no
// Windows. Apenas `get_dram_frequency` permanece exportada.