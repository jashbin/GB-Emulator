#include <timer.h>

#include <stdbool.h>
#include <stdio.h>

#include <memory.h>
#include <common.h>
#include <cpu.h>

static uint64_t tima_clock = 0;

void timer_init(void)
{
    ;
}

static uint64_t get_clock_speed(void)
{
    uint8_t select_clock = memory_read_8(MEMORY_REG_TAC) & MEMORY_TAC_INPUT_CLOCK_MASK;

    uint64_t clock_speed = 1;

    switch (select_clock)
    {
    case 0:
        clock_speed = 1024;
        break;

    case 1:
        clock_speed = 16;
        break;

    case 2:
        clock_speed = 64;
        break;

    case 3:
        clock_speed = 256;
        break;
    }

    return clock_speed;
}

void timer_execute(uint64_t clock_cycles)
{
    bool timer_on = memory_get_reg_value(MEMORY_REG_TAC, MEMORY_TAC_TIMER_ENABLED);
    if (!timer_on)
    {
        tima_clock = 0;
        return;
    }

    tima_clock += clock_cycles;

    uint64_t clock_speed = get_clock_speed();
    if (tima_clock >= clock_speed)
    {
        clock_speed = 0;

        uint8_t timer_counter = memory_read_8(MEMORY_REG_TIMA);
        if (timer_counter == 0xff)
        {
#ifdef DEBUG
            if (verbose & VERBOSE_TIMER)
            {
                fprintf(stderr, P_TIMER "Request Timer Interrupt\n");
            }
#endif

            uint8_t tma_value = memory_read_8(MEMORY_REG_TMA);
            memory_write_8(MEMORY_REG_TIMA, tma_value);

            // Request Timer Interrupt
            memory_write_reg_value(MEMORY_REG_IF, MEMORY_IEF_TIMER, true);
        }
        else
        {
            memory_write_8(MEMORY_REG_TIMA, timer_counter + 1);
        }
    }
}