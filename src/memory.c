#include <memory.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

uint8_t memory[MEMORY_SIZE] = {0};

void memory_init(void)
{
    memory[MEMORY_REG_TIMA] = 0x00;
    memory[MEMORY_REG_TMA] = 0x00;
    memory[MEMORY_REG_TAC] = 0x00;
    memory[MEMORY_REG_NR10] = 0x80;
    memory[MEMORY_REG_NR11] = 0xBF;
    memory[MEMORY_REG_NR12] = 0xF3;
    memory[MEMORY_REG_NR14] = 0xBF;
    memory[MEMORY_REG_NR21] = 0x3F;
    memory[MEMORY_REG_NR22] = 0x00;
    memory[MEMORY_REG_NR24] = 0xBF;
    memory[MEMORY_REG_NR30] = 0x7F;
    memory[MEMORY_REG_NR31] = 0xFF;
    memory[MEMORY_REG_NR32] = 0x9F;
    memory[MEMORY_REG_NR34] = 0xBF;
    memory[MEMORY_REG_NR41] = 0xFF;
    memory[MEMORY_REG_NR42] = 0x00;
    memory[MEMORY_REG_NR43] = 0x00;
    memory[MEMORY_REG_NR44] = 0xBF;
    memory[MEMORY_REG_NR50] = 0x77;
    memory[MEMORY_REG_NR51] = 0xF3;
    memory[MEMORY_REG_NR52] = 0xF1;
    memory[MEMORY_REG_LCDC] = 0x91;
    memory[MEMORY_REG_SCY] = 0x00;
    memory[MEMORY_REG_SCX] = 0x00;
    memory[MEMORY_REG_LYC] = 0x00;
    memory[MEMORY_REG_BGP] = 0xFC;
    memory[MEMORY_REG_OBP0] = 0xFF;
    memory[MEMORY_REG_OBP1] = 0xFF;
    memory[MEMORY_REG_WY] = 0x00;
    memory[MEMORY_REG_WX] = 0x00;
    memory[MEMORY_REG_IE] = 0x00;
}

void memory_read(uint8_t buff[], uint16_t mem_start_addr, uint16_t size)
{
    if (buff == NULL)
        return;
    if (mem_start_addr + size >= MEMORY_SIZE)
        return;

    memcpy(buff, memory + mem_start_addr, size);
}

inline uint8_t memory_read_8(uint16_t mem_start_addr)
{
    return memory[mem_start_addr];
}

inline uint16_t memory_read_16(uint16_t mem_start_addr)
{
    uint16_t buff = 0;
    memory_read((uint8_t *)&buff, mem_start_addr, 2);
    return buff;
}

void memory_write(uint8_t buff[], uint16_t mem_start_addr, uint16_t size)
{
    if (buff == NULL)
        return;
    if (mem_start_addr + size >= MEMORY_SIZE)
        return;

    memcpy(memory + mem_start_addr, buff, size);
}

inline void memory_write_8(uint16_t mem_start_addr, uint8_t val)
{
    memory[mem_start_addr] = val;
}

inline void memory_write_16(uint16_t mem_start_addr, uint16_t val)
{
    memory_write((uint8_t *)&val, mem_start_addr, 2);
}

void memory_print(uint16_t mem_start_addr, uint16_t size)
{
    if (mem_start_addr + size >= MEMORY_SIZE)
        return;

    uint8_t nb_cols = 16;
    uint8_t cur_col = 0;
    for (uint16_t i = mem_start_addr; i < mem_start_addr + size; i++)
    {
        if (cur_col == 0)
            fprintf(stderr, "0x%4x:", i);

        if (cur_col % 2 == 0)
            fprintf(stderr, " ");

        fprintf(stderr, "%02x", memory[i]);

        cur_col = (cur_col + 1) % nb_cols;
        if (cur_col == 0)
            fprintf(stderr, "\n");
    }
    if (cur_col != 0)
        fprintf(stderr, "\n");
}

inline bool memory_get_reg_value(uint16_t reg_addr, uint8_t bit)
{
    if (bit > 7)
        return false;

    uint8_t mask = 1 << bit;
    return (memory[reg_addr] & mask);
}

inline void memory_write_reg_value(uint16_t reg_addr, uint8_t bit, bool value)
{
    if (bit > 7)
        return;

    uint8_t mask = value << bit;
    memory[reg_addr] |= mask;
}