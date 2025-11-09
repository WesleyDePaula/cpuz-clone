gcc -O2 -Wall -municode -o cpuinfo.exe   app_win.c   cpu/cpu_basic.c cpu/cpu_cores.c cpu/cpu_cache.c cpu/cpu_clock.c   -Icpu   -lcomctl32 -lPowrProf -lgdi32 -luser32
