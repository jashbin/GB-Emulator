#include <cartridge.h>

#include <stdio.h>
#include <stdlib.h>

#include <common.h>
#include <memory.h>

static cartridge_t cartridge;

static void parse_header(void)
{
    // Title
    memory_read((uint8_t *)&cartridge.title, CARTRIDGE_HEADER_TITLE, CARTRIDGE_HEADER_TITLE_SIZE);

    // CGB Flag
    uint8_t cgb_flag = memory_read_8(CARTRIDGE_HEADER_CGB_FLAG);
    if (cgb_flag & CARTRIDGE_HEADER_CGB_SUPPORT)
        cartridge.mode = CGB_SUPPORT;
    else if (cgb_flag & CARTRIDGE_HEADER_CGB_ONLY)
        cartridge.mode = CGB_ONLY;
    else
        cartridge.mode = UNKNOWN_MODE;

    // Type
    cartridge.type = memory_read_8(CARTRIDGE_HEADER_TYPE);

    // ROM Size
    cartridge.rom_size = ((2 * MEMORY_ROM_BANK_SIZE) << memory_read_8(CARTRIDGE_HEADER_ROM_SIZE)) / MEMORY_ROM_BANK_SIZE;

    // RAM Size
    switch (memory_read_8(CARTRIDGE_HEADER_RAM_SIZE))
    {
    case 0x0:
        cartridge.ram_size = 0;
        break;
    case 0x1:
    case 0x2:
        cartridge.ram_size = 1;
        break;
    case 0x3:
        cartridge.ram_size = 4;
        break;
    case 0x4:
        cartridge.ram_size = 16;
        break;
    case 0x5:
        cartridge.ram_size = 8;
        break;
    }

    // Update MBC handler with type
}

void cartridge_load_rom(const char *filepath)
{
    // Open ROM file
    FILE *fd = fopen(filepath, "rb");
    if (fd == NULL)
    {
        fprintf(stderr, P_ERROR "Can't open file %s\n", filepath);
        exit(EXIT_FAILURE);
    }

    // Get file size
    fseek(fd, 0, SEEK_END);
    uint64_t filesize = ftell(fd);
    rewind(fd);

    // Get ROM data
    uint8_t *rom_data = malloc(filesize * sizeof(uint8_t));
    if (fread(rom_data, 1, filesize, fd) <= 0)
    {
        fprintf(stderr, P_ERROR "Can't read ROM data\n");
        exit(EXIT_FAILURE);
    }

    // Put ROM data in memory
    memory_write(rom_data, MEMORY_ROM_BANK_0_START_ADDR, filesize);
    free(rom_data);

    // Parse cartridge header
    parse_header();
}

void cartridge_print_infos(void)
{
    fprintf(stdout, "Cartridge Information:\n");
    fprintf(stdout, "Title: %.*s\n", CARTRIDGE_HEADER_TITLE_SIZE, cartridge.title);
    fprintf(stdout, "CGB Flag: %s\n", cartridge.mode == CGB_SUPPORT ? "CGB Support" : "CGB Only");
    fprintf(stdout, "Type: 0x%x\n", cartridge.type);
    fprintf(stdout, "ROM Size: %d Banks\n", cartridge.rom_size);
    fprintf(stdout, "RAM Size: %d Banks\n", cartridge.ram_size);
}