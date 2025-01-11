#pragma once

#include <stdint.h>

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144
#define FRAMERATE 60.0
#define CLOCK_CYCLES_PER_SCANLINE 15

void ppu_init(void);

void ppu_destroy(void);

void ppu_execute(uint64_t clock_cycles);