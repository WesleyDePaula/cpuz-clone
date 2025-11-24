// graphics.h - Informações de GPU e memória de vídeo
// Busca dados da placa gráfica principal via APIs dos fabricantes ou WMI

#ifndef GRAPHICS_INFO_H
#define GRAPHICS_INFO_H

#include <stddef.h>
#include <stdbool.h>

// Nome da placa de vídeo via NVML (NVIDIA), ADL (AMD), IGCL (Intel) ou WMI
bool get_gpu_name(char *buf, size_t buf_size);

// Fabricante da placa (ASUS, Gigabyte, MSI, etc) via ID do subsistema
bool get_gpu_board_manufacturer(char *buf, size_t buf_size);

// Consumo de energia (TDP) em watts - disponível apenas para NVIDIA
bool get_gpu_tdp(char *buf, size_t buf_size);

// Frequência base da GPU em MHz via APIs dos fabricantes
bool get_gpu_base_clock(char *buf, size_t buf_size);

// Quantidade de memória de vídeo via APIs dos fabricantes, DXGI ou WMI
bool get_vram_size(char *buf, size_t buf_size);

// Tipo de memória de vídeo (GDDR5, GDDR6, etc) via APIs dos fabricantes
bool get_vram_type(char *buf, size_t buf_size);

// Fabricante dos chips de memória (Samsung, Micron, etc) - apenas NVIDIA
bool get_vram_vendor(char *buf, size_t buf_size);

// Largura do barramento de memória em bits via APIs dos fabricantes
bool get_vram_bus_width(char *buf, size_t buf_size);

#endif // GRAPHICS_INFO_H
