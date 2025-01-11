#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MEMORY_ROM_BANK_SIZE 0x4000
#define MEMORY_SIZE 0x10000
#define MEMORY_END_ADDR 0xffff

#define MEMORY_ROM_BANK_0_START_ADDR 0x0000
#define MEMORY_RST_00 0x0000
#define MEMORY_RST_08 0x0008
#define MEMORY_RST_10 0x0010
#define MEMORY_RST_18 0x0018
#define MEMORY_RST_20 0x0020
#define MEMORY_RST_28 0x0028
#define MEMORY_RST_30 0x0030
#define MEMORY_RST_38 0x0038
#define MEMORY_VRAM_START_ADDR 0x8000
#define MEMORY_IO_START_ADDR 0xff00
#define MEMORY_REG_DIV 0xff04
#define MEMORY_REG_TIMA 0xff05
#define MEMORY_REG_TMA 0xff06
#define MEMORY_REG_TAC 0xff07
#define MEMORY_REG_IF 0xff0f
#define MEMORY_REG_NR10 0xff10
#define MEMORY_REG_NR11 0xff11
#define MEMORY_REG_NR12 0xff12
#define MEMORY_REG_NR14 0xff14
#define MEMORY_REG_NR21 0xff16
#define MEMORY_REG_NR22 0xff17
#define MEMORY_REG_NR24 0xff19
#define MEMORY_REG_NR30 0xff1a
#define MEMORY_REG_NR31 0xff1b
#define MEMORY_REG_NR32 0xff1c
#define MEMORY_REG_NR34 0xff1e
#define MEMORY_REG_NR41 0xff20
#define MEMORY_REG_NR42 0xff21
#define MEMORY_REG_NR43 0xff22
#define MEMORY_REG_NR44 0xff23
#define MEMORY_REG_NR50 0xff24
#define MEMORY_REG_NR51 0xff25
#define MEMORY_REG_NR52 0xff26
#define MEMORY_REG_LCDC 0xff40
#define MEMORY_REG_STAT 0xff41
#define MEMORY_REG_SCY 0xff42
#define MEMORY_REG_SCX 0xff43
#define MEMORY_REG_LY 0xff44
#define MEMORY_REG_LYC 0xff45
#define MEMORY_REG_BGP 0xff47
#define MEMORY_REG_OBP0 0xff48
#define MEMORY_REG_OBP1 0xff49
#define MEMORY_REG_WY 0xff4a
#define MEMORY_REG_WX 0xff4b
#define MEMORY_REG_IE 0xffff

#define MEMORY_LCDC_PPU_ENABLED 7                  // 0=Off, 1=On
#define MEMORY_LCDC_WINDOW_TILE_MAP_AREA 6         // 0=9800-9BFF, 1=9C00-9FFF
#define MEMORY_LCDC_WINDOW_ENABLED 5               // 0=Off, 1=On
#define MEMORY_LCDC_BG_AND_WINDOW_TILE_DATA_AREA 4 // 0=8800-97FF, 1=8000-8FFFF
#define MEMORY_LCDC_BG_TILE_MAP_AREA 3             // 0=9800-9BFF, 1=9C00-9FFF
#define MEMORY_LCDC_OBJ_SIZE 2                     // 0=8x8, 1=8x16
#define MEMORY_LCDC_OBJ_ENABLED 1                  // 0=Off, 1=On
#define MEMORY_LCDC_BG_AND_WINDOW_ENABLED 0        // 0=Off, 1=On

#define MEMORY_IEF_VBLANK 0
#define MEMORY_IEF_LCD_STAT 1
#define MEMORY_IEF_TIMER 2
#define MEMORY_IEF_SERIAL 3
#define MEMORY_IEF_JOYPAD 4

#define MEMORY_STAT_COINCID_INT 6  // LYC=LY Coincidence Interrupt
#define MEMORY_STAT_OAM_INT 5      // Mode 2 OAM Interrupt
#define MEMORY_STAT_VBLANK_INT 4   // Mode 1 V-Blank Interrupt
#define MEMORY_STAT_HBLANK_INT 3   // Mode 0 H-Blank Interrupt
#define MEMORY_STAT_COINCID_FLAG 2 // Coincidence Flag
#define MEMORY_STAT_MODE1 1        // Mode Flag 1/2
#define MEMORY_STAT_MODE0 0        // Mode Flag 2/2

#define MEMORY_TAC_TIMER_ENABLED 2
#define MEMORY_TAC_INPUT_CLOCK_MASK 0x3 // Bit 0-1

void memory_init(void);

void memory_read(uint8_t buff[], uint16_t mem_start_addr, uint16_t size);

uint8_t memory_read_8(uint16_t mem_start_addr);

uint16_t memory_read_16(uint16_t mem_start_addr);

void memory_write(uint8_t buff[], uint16_t mem_start_addr, uint16_t size);

void memory_write_8(uint16_t mem_start_addr, uint8_t val);

void memory_write_16(uint16_t mem_start_addr, uint16_t val);

void memory_print(uint16_t mem_start_addr, uint16_t size);

bool memory_get_reg_value(uint16_t reg_addr, uint8_t bit);

void memory_write_reg_value(uint16_t reg_addr, uint8_t bit, bool value);