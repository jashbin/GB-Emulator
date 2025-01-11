#pragma once

#include <stdbool.h>
#include <stdint.h>

#define VERBOSE_NONE 0
#define VERBOSE_CPU 1
#define VERBOSE_PPU 1 << 1
#define VERBOSE_TIMER 1 << 2
#define VERBOSE_ALL (VERBOSE_CPU | VERBOSE_PPU | VERBOSE_TIMER)

extern uint8_t verbose;

void cpu_init(void);

// Return Clock Cycles
uint64_t cpu_execute_inst(void);

void cpu_debugger(void);

bool cpu_is_running(void);