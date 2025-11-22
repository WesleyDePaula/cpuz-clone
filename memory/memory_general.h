// memory_general.h - declarações para informação geral de memória
//
// Este módulo fornece funções para consultar dados gerais
// sobre a memória instalada, tais como tipo, tamanho total,
// número de canais e frequência do controlador. As funções
// retornam uma string em ANSI (char[]) contendo o valor
// solicitado. A função retorna true em caso de sucesso ou
// false se a informação não pôde ser recuperada.

#pragma once

#include <stdbool.h>
#include <stddef.h>

// Obtém o tipo de memória (por exemplo, DDR4, DDR3, etc.).
// Escreve o resultado em buf (ANSI) e garante terminação NUL.
// Retorna true em caso de sucesso, false caso contrário.
bool get_memory_type(char *buf, size_t buf_size);

// Obtém o tamanho total da memória física instalada em GiB.
// O valor retornado é do tipo "32 GBytes" ou similar.
bool get_memory_size(char *buf, size_t buf_size);

// Obtém a configuração de canais de memória.
// O valor retornado é do tipo "2 x 64-bit" indicando o
// número de módulos e a largura do barramento de dados.
bool get_memory_channels(char *buf, size_t buf_size);

// (Nota) A frequência do controlador pode ser deduzida de várias
// fontes; funcionalidades relacionadas foram movidas/utilizadas
// em outros módulos. Nenhuma função extra é exportada aqui.