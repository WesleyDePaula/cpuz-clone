// mainboard_chipset.h - Funções para obter informações de chipset e southbridge
#ifndef MAINBOARD_CHIPSET_H
#define MAINBOARD_CHIPSET_H

#include <windows.h>
#include <wchar.h>

// Estrutura para armazenar informações de chipset/southbridge
typedef struct {
    wchar_t vendor[64];
    wchar_t model[64];
    wchar_t revision[16];
} ChipsetInfo;

// Retorna informações do chipset (SoC / PCH principal).
// max_entries é ignorado aqui (sempre 1), mas mantido por compatibilidade.
// Retorna o número de entradas preenchidas (0 ou 1).
size_t get_chipset_info(ChipsetInfo* info, size_t max_entries);

// Retorna informações do southbridge (PCH/FCH).
// Retorna o número de entradas preenchidas (0 ou 1).
size_t get_southbridge_info(ChipsetInfo* info, size_t max_entries);

// Função auxiliar para montar as linhas que vão para a GUI.
// labels:    "Chipset", "Southbridge"
// vendors:   nome do fabricante (AMD, Intel, etc.)
// models:    modelo (ex.: "Ryzen SOC", "B550")
// revisions: revisão ("Rev. xx")
// max_rows:  tamanho máximo dos arrays
// Retorna o número de linhas preenchidas.
size_t build_chipset_rows(wchar_t labels[][32], wchar_t vendors[][64],
                          wchar_t models[][64], wchar_t revisions[][16],
                          size_t max_rows);

#endif // MAINBOARD_CHIPSET_H
