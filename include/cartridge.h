#pragma once

#include <stdint.h>

#define CARTRIDGE_HEADER_TITLE 0x134
#define CARTRIDGE_HEADER_TITLE_SIZE 15
#define CARTRIDGE_HEADER_CGB_FLAG 0x143
#define CARTRIDGE_HEADER_CGB_SUPPORT 0x80
#define CARTRIDGE_HEADER_CGB_ONLY 0xc0
#define CARTRIDGE_HEADER_TYPE 0x147
#define CARTRIDGE_HEADER_ROM_SIZE 0x148
#define CARTRIDGE_HEADER_RAM_SIZE 0x149

typedef enum
{
    CGB_SUPPORT,
    CGB_ONLY,
    UNKNOWN_MODE
} cgb_mode_t;

typedef struct
{
    char title[CARTRIDGE_HEADER_TITLE_SIZE];
    cgb_mode_t mode;
    uint8_t type;
    uint8_t rom_size; // # of banks
    uint8_t ram_size; // # of banks
} cartridge_t;

void cartridge_load_rom(const char *filepath);

void cartridge_print_infos(void);