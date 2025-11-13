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

// Retorna informações do chipset
// Retorna o número de entradas preenchidas (geralmente 1 para chipset)
size_t get_chipset_info(ChipsetInfo* info, size_t max_entries);

// Retorna informações do southbridge
// Retorna o número de entradas preenchidas (geralmente 1 para southbridge)
size_t get_southbridge_info(ChipsetInfo* info, size_t max_entries);

// Função auxiliar para formatar as informações em formato de tabela
// labels: array de labels (ex: "Chipset", "Southbridge")
// vendors: array de vendors
// models: array de models (sem revision)
// revisions: array de revisions
// max_rows: tamanho máximo dos arrays
// Retorna o número de linhas preenchidas
size_t build_chipset_rows(wchar_t labels[][32], wchar_t vendors[][64],
                         wchar_t models[][64], wchar_t revisions[][16], size_t max_rows);

#endif // MAINBOARD_CHIPSET_H
