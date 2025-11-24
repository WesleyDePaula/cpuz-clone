gcc -O2 -Wall -municode \
  -o "UMBAHIU 2025 Edition XYZ.exe" \
  app_win.c \
  cpu/cpu_basic.c cpu/cpu_cores.c cpu/cpu_cache.c cpu/cpu_clock.c \
  mainboard/mainboard_basic.c mainboard/mainboard_chipset.c mainboard/mainboard_bios.c \
  memory/memory_general.c memory/memory_timings.c \
  graphics/graphics.c \
  -Icpu -Imainboard -Imemory \
  -Igraphics \
  -lcomctl32 -lPowrProf -lsetupapi -lole32 -loleaut32 -lwbemuuid -lgdi32 -luser32
