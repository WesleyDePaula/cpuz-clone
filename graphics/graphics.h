// graphics.h - interface for retrieving GPU and video memory information
//
// This module exposes a set of functions to query the primary graphics
// adapter present in the system. The intention is to collect similar
// information to what tools like GPU‑Z report for the main GPU and its
// dedicated memory. The functions return their results in ANSI strings
// and will return false when the requested value could not be
// determined. When false is returned the contents of the output buffer
// are left untouched.

#ifndef GRAPHICS_INFO_H
#define GRAPHICS_INFO_H

#include <stddef.h>
#include <stdbool.h>

// Retrieves the name of the primary GPU.  For example "NVIDIA GeForce RTX 3060 Ti".
bool get_gpu_name(char *buf, size_t buf_size);

// Retrieves the board manufacturer (board partner) of the primary GPU.  When
// available, this returns strings such as "Gigabyte", "MSI", "PNY" etc.
// When the vendor cannot be determined the function returns false.
bool get_gpu_board_manufacturer(char *buf, size_t buf_size);

// Retrieves the power draw (TDP/TGP) limit of the primary GPU in human
// readable form.  The returned string should include units, e.g.
// "225.0 W".  When the value cannot be determined the function
// returns false.
bool get_gpu_tdp(char *buf, size_t buf_size);

// Retrieves the base clock of the primary GPU expressed in megahertz.
// The returned string should include units, e.g. "1665.0 MHz".  When
// the value cannot be determined the function returns false.
bool get_gpu_base_clock(char *buf, size_t buf_size);

// Retrieves the size of the video memory (VRAM) on the primary GPU.
// The string should contain a human readable quantity such as
// "8 GBytes".
bool get_vram_size(char *buf, size_t buf_size);

// Retrieves the memory technology used by the video memory (e.g.
// "GDDR6X", "HBM2").  If the type cannot be determined this
// function returns false.
bool get_vram_type(char *buf, size_t buf_size);

// Retrieves the vendor of the video memory chips, for example "Samsung"
// or "Micron".  Returns false when the information is not available.
bool get_vram_vendor(char *buf, size_t buf_size);

// Retrieves the width of the memory bus (in bits).  The returned
// string should include units, e.g. "256 bits".  When the value
// cannot be determined the function returns false.
bool get_vram_bus_width(char *buf, size_t buf_size);

#endif // GRAPHICS_INFO_H