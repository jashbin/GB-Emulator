#include <cpu.h>

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <memory.h>
#include <common.h>

#define FLAG_Z 7 // Bit position in Flags register
#define FLAG_N 6
#define FLAG_H 5
#define FLAG_C 4

#define COMMAND_MAX_SIZE 128

#define SET_BIT(val, nb_bit) (val |= (1U << nb_bit))
#define CLEAR_BIT(val, nb_bit) (val &= ~(1U << nb_bit))
#define FLIP_BIT(val, nb_bit) (val ^= (1U << nb_bit))
#define CHECK_BIT(val, nb_bit) ((val >> nb_bit) & 1U)

#define HAS_HALF_CARRY_ON_SUB(op1, result) (((result) & (~(op1))) & 0xf)
#define HAS_HALF_CARRY_ON_ADD(op1, op2) (((op1) & 0xf) & ((op2) & 0xf))

static struct
{
    union
    {
        struct
        {
            uint8_t f;
            uint8_t a;
        };
        uint16_t af;
    };

    union
    {
        struct
        {
            uint8_t c;
            uint8_t b;
        };
        uint16_t bc;
    };

    union
    {
        struct
        {
            uint8_t e;
            uint8_t d;
        };
        uint16_t de;
    };

    union
    {
        struct
        {
            uint8_t l;
            uint8_t h;
        };
        uint16_t hl;
    };

    uint16_t sp;
    uint16_t pc;
    bool ime;

} registers;

static uint64_t nb_exec_inst = 0;

static bool running = true;
static bool to_continue = false;
static uint16_t to_execute = 0;
uint8_t verbose = VERBOSE_CPU; // Extern
static uint16_t breakpoint_addr = 0x0000;

static char last_command[COMMAND_MAX_SIZE];

void cpu_init(void)
{
    registers.pc = 0x100; // EntryPoint
    registers.af = 0x01B0;
    registers.bc = 0x0013;
    registers.de = 0x00D8;
    registers.hl = 0x014D;
    registers.sp = 0xFFFE;
}

bool cpu_is_running(void)
{
    return running;
}

void cpu_debugger(void)
{
    // Update Step
    if (to_execute)
    {
        to_execute--;
    }

    while (running && !to_continue && !to_execute)
    {
        char command[COMMAND_MAX_SIZE];
        fprintf(stderr, YELLOW "DBG" WHITE "> ");
        if (!fgets(command, COMMAND_MAX_SIZE, stdin))
        {
            fprintf(stderr, P_ERROR "Can't read on stdin\n");
        }
        else if (!strncmp(command, "\n", 1))
        {
            strncpy(command, last_command, COMMAND_MAX_SIZE);
        }
        else
        {
            strncpy(last_command, command, COMMAND_MAX_SIZE);
        }

        if (!strncmp(command, "help", 4))
        {
            fprintf(stderr, "Debugger commands:\n");
            fprintf(stderr, "- quit\n");
            fprintf(stderr, "- step [NB]\n");
            fprintf(stderr, "- continue\n");
            fprintf(stderr, "- verbose (0-NONE, 1-CPU, 2-PPU, 3-TIMER, 4-ALL)\n");
            fprintf(stderr, "- breakpoint <address>\n");
        }
        else if (!strncmp(command, "quit", 4) || !strncmp(command, "q\n", 2))
        {
            running = false;
            fprintf(stderr, "Quitting...\n");
        }
        else if (!strncmp(command, "step", 4))
        {
            char *check;
            uint16_t val = strtol(command + 4, &check, 10);
            if (command + 4 == check)
            {
                to_execute = 1;
            }
            else
            {
                to_execute = val;
            }
        }
        else if (!strncmp(command, "continue", 7) || !strncmp(command, "c\n", 2))
        {
            to_continue = true;
            fprintf(stderr, "Continuing...\n");
        }
        else if (!strncmp(command, "verbose", 7))
        {
            char *check;
            uint8_t value = strtol(command + 7, &check, 10);
            if (command + 7 == check || value > 3)
            {
                fprintf(stderr, "Verbose value invalid!\n");
            }
            else
            {
                if (value == 0)
                    verbose = VERBOSE_NONE;
                else if (value == 1)
                    verbose = VERBOSE_CPU;
                else if (value == 2)
                    verbose = VERBOSE_PPU;
                else if (value == 3)
                    verbose = VERBOSE_TIMER;
                else if (value == 4)
                    verbose = VERBOSE_ALL;
                fprintf(stderr, "Verbose set to %u\n", value);
            }
        }
        else if (!strncmp(command, "breakpoint", 10))
        {
            char *check;
            uint16_t addr = strtol(command + 10, &check, 16);
            if (command + 10 == check)
            {
                fprintf(stderr, "Breakpoint address invalid!\n");
            }
            else
            {
                breakpoint_addr = addr;
                fprintf(stderr, "Breakpoint set to 0x%x\n", breakpoint_addr);
            }
        }
        else
        {
            fprintf(stderr, "Unknown command\n");
        }
    }
}

static void print_flags(void)
{
    fprintf(stderr, P_INFO "Flags [Z=%d, N=%d, H=%d, C=%d]\n",
            CHECK_BIT(registers.f, FLAG_Z),
            CHECK_BIT(registers.f, FLAG_N),
            CHECK_BIT(registers.f, FLAG_H),
            CHECK_BIT(registers.f, FLAG_C));
}

static void breakpoint_check(void)
{
    if (breakpoint_addr == registers.pc)
    {
        to_execute = 0;
        to_continue = false;
        fprintf(stderr, P_DEBUG "Hit breakpoint at 0x%x\n", breakpoint_addr);
    }
}

uint64_t handle_cb_inst(uint8_t inst)
{
    uint64_t clock_cycles = 0;

    switch (inst)
    {
    case 0x37: // SWAP A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Exec CB 0x%x - SWAP A\n", inst);
        }
#endif
        registers.a = ((registers.a & 0x0f) << 4) | ((registers.a & 0xf0) >> 4);

        registers.f = 0;
        if (!registers.a)
            SET_BIT(registers.f, FLAG_Z);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x%x\n", registers.a);
            print_flags();
        }
#endif

        clock_cycles = 8;
        break;

    case 0x87: // RES 0, A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Exec CB 0x%x - RES 0, A\n", inst);
        }
#endif
        CLEAR_BIT(registers.a, 0);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x%x\n", registers.a);
            print_flags();
        }
#endif

        clock_cycles = 8;
        break;

    default:
        running = false;
        fprintf(stderr, P_FATAL "Successfully executed %ld instructions\n", nb_exec_inst);
        fprintf(stderr, P_FATAL "At 0x%x - CB instruction 0x%02x not implemented yet!\n", registers.pc, inst);
        break;
    }

    return clock_cycles;
}

uint64_t cpu_execute_inst(void)
{
    uint64_t clock_cycles = 0;

    // Get Instruction
    uint8_t inst = memory_read_8(registers.pc);

    uint8_t offset;
    uint8_t value;
    uint8_t cb_inst;
    uint16_t call_addr;
    uint16_t addr;
    uint16_t value_addr;
    uint8_t mem_val;
    int16_t sub;

    switch (inst)
    {
    case 0x00: // NOP
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - NOP\n", registers.pc, inst);
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x01: // LD BC, nnnn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD BC, nnnn\n", registers.pc, inst);
        }
#endif
        registers.bc = memory_read_16(registers.pc + 1);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load 0x%x in BC\n", registers.bc);
        }
#endif
        registers.pc += 3;
        clock_cycles = 12;
        break;

    case 0x03: // INC BC
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - INC BC\n", registers.pc, inst);
        }
#endif
        registers.bc++;

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set BC to 0x%x\n", registers.bc);
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x04: // INC B
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - INC B\n", registers.pc, inst);
        }
#endif
        registers.b++;

        CLEAR_BIT(registers.f, FLAG_N);
        if (!registers.b)
            SET_BIT(registers.f, FLAG_Z);
        else
            CLEAR_BIT(registers.f, FLAG_Z);
        if (HAS_HALF_CARRY_ON_ADD(registers.b - 1, 1))
            SET_BIT(registers.f, FLAG_H);
        else
            CLEAR_BIT(registers.f, FLAG_H);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Inc B to 0x%x\n", registers.b);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x05: // DEC B
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - DEC B\n", registers.pc, inst);
        }
#endif
        registers.b--;

        SET_BIT(registers.f, FLAG_N);
        if (!registers.b)
            SET_BIT(registers.f, FLAG_Z);
        else
            CLEAR_BIT(registers.f, FLAG_Z);
        if (HAS_HALF_CARRY_ON_SUB(registers.b + 1, registers.b))
            SET_BIT(registers.f, FLAG_H);
        else
            CLEAR_BIT(registers.f, FLAG_H);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Dec B to 0x%x\n", registers.b);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x06: // LD B, nn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD B, nn\n", registers.pc, inst);
        }
#endif
        registers.b = memory_read_8(registers.pc + 1);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load 0x%x in B\n", registers.b);
        }
#endif
        registers.pc += 2;
        clock_cycles = 8;
        break;

    case 0x0b: // DEC BC
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - DEC BC\n", registers.pc, inst);
        }
#endif
        registers.bc--;

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Dec BC to 0x%x\n", registers.bc);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x0c: // INC C
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - INC C\n", registers.pc, inst);
        }
#endif
        registers.c++;

        CLEAR_BIT(registers.f, FLAG_N);
        if (!registers.c)
            SET_BIT(registers.f, FLAG_Z);
        else
            CLEAR_BIT(registers.f, FLAG_Z);
        if (HAS_HALF_CARRY_ON_ADD(registers.c - 1, 1))
            SET_BIT(registers.f, FLAG_H);
        else
            CLEAR_BIT(registers.f, FLAG_H);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Inc C to 0x%x\n", registers.c);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x0d: // DEC C
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - DEC C\n", registers.pc, inst);
        }
#endif
        registers.c--;

        SET_BIT(registers.f, FLAG_N);
        if (!registers.c)
            SET_BIT(registers.f, FLAG_Z);
        else
            CLEAR_BIT(registers.f, FLAG_Z);
        if (HAS_HALF_CARRY_ON_SUB(registers.c + 1, registers.c))
            SET_BIT(registers.f, FLAG_H);
        else
            CLEAR_BIT(registers.f, FLAG_H);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Dec C to 0x%x\n", registers.c);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x0e: // LD C, nn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD C, nn\n", registers.pc, inst);
        }
#endif
        registers.c = memory_read_8(registers.pc + 1);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load 0x%x in C\n", registers.c);
        }
#endif
        registers.pc += 2;
        clock_cycles = 8;
        break;

    case 0x11: // LD DE, nnnn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD DE, nnnn\n", registers.pc, inst);
        }
#endif
        registers.de = memory_read_16(registers.pc + 1);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load 0x%x in DE\n", registers.de);
        }
#endif
        registers.pc += 3;
        clock_cycles = 12;
        break;

    case 0x12: // LD (DE), A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD (DE), A\n", registers.pc, inst);
        }
#endif
        memory_write_8(registers.de, registers.a);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Save A(0x%x) at *DE(0x%x)\n", registers.a, registers.de);
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x13: // INC DE
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - INC DE\n", registers.pc, inst);
        }
#endif
        registers.de++;

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set DE to 0x%x\n", registers.de);
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x14: // INC D
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - INC D\n", registers.pc, inst);
        }
#endif
        registers.d++;

        CLEAR_BIT(registers.f, FLAG_N);
        if (!registers.d)
            SET_BIT(registers.f, FLAG_Z);
        else
            CLEAR_BIT(registers.f, FLAG_Z);
        if (HAS_HALF_CARRY_ON_ADD(registers.d - 1, 1))
            SET_BIT(registers.f, FLAG_H);
        else
            CLEAR_BIT(registers.f, FLAG_H);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Inc D to 0x%x\n", registers.d);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x15: // DEC D
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - DEC D\n", registers.pc, inst);
        }
#endif
        registers.d--;

        SET_BIT(registers.f, FLAG_N);
        if (!registers.d)
            SET_BIT(registers.f, FLAG_Z);
        else
            CLEAR_BIT(registers.f, FLAG_Z);
        if (HAS_HALF_CARRY_ON_SUB(registers.d + 1, registers.d))
            SET_BIT(registers.f, FLAG_H);
        else
            CLEAR_BIT(registers.f, FLAG_H);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Dec D to 0x%x\n", registers.d);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x16: // LD D, nn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD D, nn\n", registers.pc, inst);
        }
#endif
        registers.d = memory_read_8(registers.pc + 1);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load 0x%x in D\n", registers.d);
        }
#endif
        registers.pc += 2;
        clock_cycles = 8;
        break;

    case 0x19: // ADD HL, DE
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - ADD HL, DE\n", registers.pc, inst);
        }
#endif
        registers.hl += registers.de;

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set HL to 0x%x\n", registers.hl);
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x1a: // LD A, (DE)
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD A, (DE)\n", registers.pc, inst);
        }
#endif
        registers.a = memory_read_8(registers.de);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load *DE(0x%x) in A(0x%x)\n", registers.de, registers.a);
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x1c: // INC E
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - INC E\n", registers.pc, inst);
        }
#endif
        registers.e++;

        CLEAR_BIT(registers.f, FLAG_N);
        if (!registers.e)
            SET_BIT(registers.f, FLAG_Z);
        else
            CLEAR_BIT(registers.f, FLAG_Z);
        if (HAS_HALF_CARRY_ON_ADD(registers.e - 1, 1))
            SET_BIT(registers.f, FLAG_H);
        else
            CLEAR_BIT(registers.f, FLAG_H);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Inc E to 0x%x\n", registers.c);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x18: // JR nn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - JR nn\n", registers.pc, inst);
        }
#endif
        registers.pc += (int8_t)memory_read_8(registers.pc + 1);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Jump at 0x%x\n", registers.pc + 2);
        }
#endif

        registers.pc += 2;
        clock_cycles = 12;
        break;

    case 0x20: // JR NZ,nn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - JR NZ,nn\n", registers.pc, inst);
        }
#endif
        clock_cycles = 8;
        if (!CHECK_BIT(registers.f, FLAG_Z))
        {
            registers.pc += (int8_t)memory_read_8(registers.pc + 1);

            clock_cycles = 12;
#ifdef DEBUG
            if (verbose & VERBOSE_CPU)
            {
                fprintf(stderr, P_INFO "Jump at 0x%x\n", registers.pc + 2);
            }
#endif
        }
#ifdef DEBUG
        else
        {
            if (verbose & VERBOSE_CPU)
            {
                fprintf(stderr, P_INFO "No Jump\n");
            }
        }
#endif
        registers.pc += 2;
        // clock_cycles = 12; // 12 or 8
        break;

    case 0x21: // LD HL, nnnn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD HL, nnnn\n", registers.pc, inst);
        }
#endif
        registers.hl = memory_read_16(registers.pc + 1);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load 0x%x in HL\n", registers.hl);
        }
#endif
        registers.pc += 3;
        clock_cycles = 12;
        break;

    case 0x22: // LDI (HL), A - (HL)=A, HL=HL+1
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LDI (HL), A - (HL)=A, HL=HL+1\n", registers.pc, inst);
        }
#endif
        memory_write_8(registers.hl, registers.a);
        registers.hl++;

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Save A(0x%x) at *HL(0x%x)\n", registers.a, registers.hl - 1);
            fprintf(stderr, P_INFO "Set HL to 0x%x\n", registers.hl);
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x23: // INC HL
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - INC HL\n", registers.pc, inst);
        }
#endif
        registers.hl++;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load 0x%x in HL\n", registers.hl);
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x28: // JR Z,nn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - JR Z,nn\n", registers.pc, inst);
        }
#endif
        clock_cycles = 8;
        if (CHECK_BIT(registers.f, FLAG_Z))
        {
            registers.pc += (int8_t)memory_read_8(registers.pc + 1);

            clock_cycles = 12;
#ifdef DEBUG
            if (verbose & VERBOSE_CPU)
            {
                fprintf(stderr, P_INFO "Jump at 0x%x\n", registers.pc + 2);
            }
#endif
        }
#ifdef DEBUG
        else
        {
            if (verbose & VERBOSE_CPU)
            {
                fprintf(stderr, P_INFO "No Jump\n");
            }
        }
#endif
        registers.pc += 2;
        // clock_cycles = 12; // 12 or 8
        break;

    case 0x2a: // LDI A, (HL); HL=HL+1
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LDI A, (HL); HL=HL+1\n", registers.pc, inst);
        }
#endif
        registers.a = memory_read_8(registers.hl);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load *HL(0x%x) in A(0x%x)\n", registers.hl, registers.a);
        }
#endif
        registers.hl++;
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x2f: // CPL, A = A xor FF
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - CPL, A = A xor FF\n", registers.pc, inst);
        }
#endif
        registers.a ^= 0xff;
        SET_BIT(registers.f, FLAG_N);
        SET_BIT(registers.f, FLAG_H);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x%x\n", registers.a);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x30: // JR NC, nn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - JR NC, nn\n", registers.pc, inst);
        }
#endif
        clock_cycles = 8;
        if (!CHECK_BIT(registers.f, FLAG_C))
        {
            registers.pc += (int8_t)memory_read_8(registers.pc + 1);

            clock_cycles = 12;
#ifdef DEBUG
            if (verbose & VERBOSE_CPU)
            {
                fprintf(stderr, P_INFO "Jump at 0x%x\n", registers.pc + 2);
            }
#endif
        }
#ifdef DEBUG
        else
        {
            if (verbose & VERBOSE_CPU)
            {
                fprintf(stderr, P_INFO "No Jump\n");
            }
        }
#endif
        registers.pc += 2;
        // clock_cycles = 12; // 12 or 8
        break;

    case 0x31: // LD SP, nnnn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD SP, nnnn\n", registers.pc, inst);
        }
#endif
        registers.sp = memory_read_16(registers.pc + 1);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load 0x%x in SP\n", registers.sp);
        }
#endif
        registers.pc += 3;
        clock_cycles = 12;
        break;

    case 0x32: // LDD (HL), A; HL=HL-1
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LDD (HL), A; HL=HL-1\n", registers.pc, inst);
        }
#endif
        memory_write_8(registers.hl, registers.a);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Save A(0x%x) in *HL(0x%x)\n", registers.a, registers.hl);
        }
#endif
        registers.hl--;
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x36: // LD (HL), nn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD (HL), nn\n", registers.pc, inst);
        }
#endif
        value = memory_read_8(registers.pc + 1);
        memory_write_8(registers.hl, value);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Save 0x%x in *HL(0x%x)\n", value, registers.hl);
        }
#endif
        registers.pc += 2;
        clock_cycles = 12;
        break;

    case 0x3e: // LD A, nn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD A, nn\n", registers.pc, inst);
        }
#endif
        registers.a = memory_read_8(registers.pc + 1);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load 0x%x in A\n", registers.a);
        }
#endif
        registers.pc += 2;
        clock_cycles = 8;
        break;

    case 0x3f: // CCF
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - CCF\n", registers.pc, inst);
        }
#endif
        CLEAR_BIT(registers.f, FLAG_N);
        CLEAR_BIT(registers.f, FLAG_H);
        FLIP_BIT(registers.f, FLAG_C);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            print_flags();
        }
#endif

        registers.pc += 1;
        clock_cycles = 4;
        break;

    case 0x40: // LD B, B
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD B, B\n", registers.pc, inst);
        }
#endif
        registers.b = registers.b;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load B(0x%x) in B\n", registers.b);
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x47: // LD B, A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD B, A\n", registers.pc, inst);
        }
#endif
        registers.b = registers.a;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load A(0x%x) in B\n", registers.a);
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x4f: // LD C, A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD C, A\n", registers.pc, inst);
        }
#endif
        registers.c = registers.a;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load A(0x%x) in C\n", registers.a);
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x50: // LD D, B
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD D, B\n", registers.pc, inst);
        }
#endif
        registers.d = registers.b;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load B(0x%x) in D\n", registers.a);
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x56: // LD D, (HL)
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD D, (HL)\n", registers.pc, inst);
        }
#endif
        registers.d = memory_read_8(registers.hl);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load *HL(0x%x) in D(0x%x)\n", registers.hl, registers.d);
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x5e: // LD E, (HL)
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD E, (HL)\n", registers.pc, inst);
        }
#endif
        registers.e = memory_read_8(registers.hl);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load *HL(0x%x) in E(0x%x)\n", registers.hl, registers.e);
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x5f: // LD E, A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD E, A\n", registers.pc, inst);
        }
#endif
        registers.e = registers.a;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load A(0x%x) in E\n", registers.a);
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x70: // LD (HL), B
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD (HL), B\n", registers.pc, inst);
        }
#endif
        memory_write_8(registers.hl, registers.b);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Save B(0x%x) to *HL(0x%x)\n", registers.b, registers.hl);
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x78: // LD A, B
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD A, B\n", registers.pc, inst);
        }
#endif
        registers.a = registers.b;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load B(0x%x) in A\n", registers.b);
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x79: // LD C, A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD C, A\n", registers.pc, inst);
        }
#endif
        registers.c = registers.a;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load A(0x%x) in C\n", registers.a);
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x7c: // LD A, H
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD A, H\n", registers.pc, inst);
        }
#endif
        registers.a = registers.h;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load H(0x%x) in A\n", registers.h);
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x7d: // LD A, L
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD A, L\n", registers.pc, inst);
        }
#endif
        registers.a = registers.l;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load L(0x%x) in A\n", registers.l);
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x7e: // LD A, (HL)
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD A, (HL)\n", registers.pc, inst);
        }
#endif
        registers.a = memory_read_8(registers.hl);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load *HL(0x%x) in A(0x%x)\n", registers.hl, registers.a);
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0x7f: // LD A, A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD A, A\n", registers.pc, inst);
        }
#endif
        registers.a = registers.a;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load A(0x%x) in A\n", registers.a);
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x80: // ADD A, B
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - ADD A, B\n", registers.pc, inst);
        }
#endif
        registers.f = 0;
        if (HAS_HALF_CARRY_ON_ADD(registers.a, registers.b))
            SET_BIT(registers.f, FLAG_H);
        if (((uint16_t)registers.a + (uint16_t)registers.b) > 0xff)
            SET_BIT(registers.f, FLAG_C);

        registers.a += registers.b;

        if (!registers.a)
            SET_BIT(registers.f, FLAG_Z);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x%x\n", registers.a);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x81: // ADD A, C
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - ADD A, C\n", registers.pc, inst);
        }
#endif
        registers.f = 0;
        if (HAS_HALF_CARRY_ON_ADD(registers.a, registers.c))
            SET_BIT(registers.f, FLAG_H);
        if (((uint16_t)registers.a + (uint16_t)registers.c) > 0xff)
            SET_BIT(registers.f, FLAG_C);

        registers.a += registers.c;

        if (!registers.a)
            SET_BIT(registers.f, FLAG_Z);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x%x\n", registers.a);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0x87: // ADD A, A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - ADD A, A\n", registers.pc, inst);
        }
#endif
        registers.f = 0;
        if (HAS_HALF_CARRY_ON_ADD(registers.a, registers.a))
            SET_BIT(registers.f, FLAG_H);
        if (((uint16_t)registers.a + (uint16_t)registers.a) > 0xff)
            SET_BIT(registers.f, FLAG_C);

        registers.a += registers.a;

        if (!registers.a)
            SET_BIT(registers.f, FLAG_Z);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x%x\n", registers.a);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0xa1: // AND A, C
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - AND A, C\n", registers.pc, inst);
        }
#endif
        registers.a &= registers.c;
        registers.f = 0;
        SET_BIT(registers.f, FLAG_H);
        if (!registers.a)
            SET_BIT(registers.f, FLAG_Z);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x%x\n", registers.a);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0xa7: // AND A, A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - AND A, A\n", registers.pc, inst);
        }
#endif
        registers.a &= registers.a;
        registers.f = 0;
        SET_BIT(registers.f, FLAG_H);
        if (!registers.a)
            SET_BIT(registers.f, FLAG_Z);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x%x\n", registers.a);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0xa9: // XOR A, C
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - XOR A, C\n", registers.pc, inst);
        }
#endif
        registers.a ^= registers.c;
        registers.f = 0;
        if (!registers.a)
            SET_BIT(registers.f, FLAG_Z);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x%x\n", registers.a);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0xaf: // XOR A, A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - XOR A, A\n", registers.pc, inst);
        }
#endif
        registers.a = 0;
        registers.f = 0;
        if (!registers.a)
            SET_BIT(registers.f, FLAG_Z);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x0\n");
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0xb0: // OR B, A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - OR B, A\n", registers.pc, inst);
        }
#endif
        registers.a |= registers.b;
        registers.f = 0;
        if (!registers.a)
            SET_BIT(registers.f, FLAG_Z);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x%x\n", registers.a);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0xb1: // OR C, A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - OR C, A\n", registers.pc, inst);
        }
#endif
        registers.a |= registers.c;
        registers.f = 0;
        if (!registers.a)
            SET_BIT(registers.f, FLAG_Z);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x%x\n", registers.a);
            print_flags();
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0xbf: // CP A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - CP A\n", registers.pc, inst);
        }
#endif
        int16_t cmp = registers.a - registers.a;

        SET_BIT(registers.f, FLAG_N);
        if (!cmp)
            SET_BIT(registers.f, FLAG_Z);
        else
            CLEAR_BIT(registers.f, FLAG_Z);
        if (HAS_HALF_CARRY_ON_SUB(registers.a, cmp))
            SET_BIT(registers.f, FLAG_H);
        else
            CLEAR_BIT(registers.f, FLAG_H);
        if (cmp < 0)
            SET_BIT(registers.f, FLAG_C);
        else
            CLEAR_BIT(registers.f, FLAG_C);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            print_flags();
        }
#endif

        registers.pc++;
        clock_cycles = 4;
        break;

    case 0xc1: // POP BC - RR=(SP), SP=SP+2
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - POP BC\n", registers.pc, inst);
        }
#endif
        registers.bc = memory_read_16(registers.sp);
        registers.sp += 2;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Pop 0x%x in BC\n", registers.bc);
        }
#endif
        registers.pc++;
        clock_cycles = 12;
        break;

    case 0xc3: // JP nnnn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - JP nnnn\n", registers.pc, inst);
        }
#endif
        registers.pc = memory_read_16(registers.pc + 1);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Jump at 0x%x\n", registers.pc);
        }
#endif
        clock_cycles = 16;
        break;

    case 0xc4: // call NZ, nnnn  , SP=SP-2, (SP)=PC, PC=nnnn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - call NZ, nnnn    , SP=SP-2, (SP)=PC, PC=nnnn\n", registers.pc, inst);
        }
#endif
        call_addr = memory_read_16(registers.pc + 1);

        if (!CHECK_BIT(registers.f, FLAG_Z))
        {
            registers.sp -= 2;
            memory_write_16(registers.sp, registers.pc + 3);
            registers.pc = call_addr;
#ifdef DEBUG
            if (verbose & VERBOSE_CPU)
            {
                fprintf(stderr, P_INFO "Save PC(0x%x) at 0x%x\n", memory_read_16(registers.sp), registers.sp);
                fprintf(stderr, P_INFO "Call to 0x%x\n", registers.pc);
            }
#endif
            clock_cycles = 24;
        }
        else
        {
#ifdef DEBUG
            if (verbose & VERBOSE_CPU)
            {
                fprintf(stderr, P_INFO "No call\n");
            }
#endif
            registers.pc += 3;
            clock_cycles = 12;
        }
        break;

    case 0xc5: // PUSH BC - SP=SP-2, (SP)=rr
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - PUSH BC\n", registers.pc, inst);
        }
#endif
        registers.sp -= 2;
        memory_write_16(registers.sp, registers.bc);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "PUSH BC(0x%x) at SP(0x%x)\n", registers.bc, registers.sp);
        }
#endif
        registers.pc++;
        clock_cycles = 16;
        break;

    case 0xc8: // RET Z, PC=(SP), SP=SP+2
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - RET Z, PC=(SP), SP=SP+2\n", registers.pc, inst);
        }
#endif
        if (CHECK_BIT(registers.f, FLAG_Z))
        {
            clock_cycles = 20;
            registers.pc = memory_read_16(registers.sp);
            registers.sp += 2;

#ifdef DEBUG
            if (verbose & VERBOSE_CPU)
            {
                fprintf(stderr, P_INFO "RET to 0x%x\n", registers.pc);
                fprintf(stderr, P_INFO "Set SP to 0x%x\n", registers.sp);
            }
#endif
        }
        else
        {
            clock_cycles = 8;
            registers.pc++;
#ifdef DEBUG
            if (verbose & VERBOSE_CPU)
            {
                fprintf(stderr, P_INFO "No Ret\n");
            }
#endif
        }

        break;

    case 0xc9: // RET, PC=(SP), SP=SP+2
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - RET, PC=(SP), SP=SP+2\n", registers.pc, inst);
        }
#endif
        registers.pc = memory_read_16(registers.sp);
        registers.sp += 2;

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "RET to 0x%x\n", registers.pc);
            fprintf(stderr, P_INFO "Set SP to 0x%x\n", registers.sp);
        }
#endif
        clock_cycles = 16;
        break;

    case 0xca: // JP Z, nnnn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - JP Z, nnnn\n", registers.pc, inst);
        }
#endif
        clock_cycles = 12;
        if (CHECK_BIT(registers.f, FLAG_Z))
        {
            clock_cycles = 16;
            registers.pc = memory_read_16(registers.pc + 1);
#ifdef DEBUG
            if (verbose & VERBOSE_CPU)
            {
                fprintf(stderr, P_INFO "Jump at 0x%x\n", registers.pc);
            }
#endif
        }
        else
        {
            registers.pc += 3;
#ifdef DEBUG
            if (verbose & VERBOSE_CPU)
            {
                fprintf(stderr, P_INFO "No Jump\n");
            }
#endif
        }
        break;

    case 0xcb: // CB nn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - CB NN\n", registers.pc, inst);
        }
#endif
        cb_inst = memory_read_8(registers.pc + 1);
        clock_cycles = handle_cb_inst(cb_inst);

        registers.pc += 2;
        break;

    case 0xcd: // call nnnn, SP=SP-2, (SP)=PC, PC=nnnn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - call nnnn, SP=SP-2, (SP)=PC, PC=nnnn\n", registers.pc, inst);
        }
#endif
        call_addr = memory_read_16(registers.pc + 1);
        registers.sp -= 2;
        memory_write_16(registers.sp, registers.pc + 3);
        registers.pc = call_addr;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Save PC(0x%x) at 0x%x\n", memory_read_16(registers.sp), registers.sp);
            fprintf(stderr, P_INFO "Call to 0x%x\n", registers.pc);
        }
#endif
        clock_cycles = 24;
        break;

    case 0xd1: // POP DE - RR=(SP), SP=SP+2
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - POP DE\n", registers.pc, inst);
        }
#endif
        registers.de = memory_read_16(registers.sp);
        registers.sp += 2;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Pop 0x%x in DE\n", registers.de);
        }
#endif
        registers.pc++;
        clock_cycles = 12;
        break;

    case 0xd5: // PUSH DE - SP=SP-2, (SP)=rr
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - PUSH DE\n", registers.pc, inst);
        }
#endif
        registers.sp -= 2;
        memory_write_16(registers.sp, registers.de);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "PUSH DE(0x%x) at SP(0x%x)\n", registers.de, registers.sp);
        }
#endif
        registers.pc++;
        clock_cycles = 16;
        break;

    case 0xe0: // LD (FF00+nn),A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD (FF00+nn),A\n", registers.pc, inst);
        }
#endif
        offset = memory_read_8(registers.pc + 1);
        memory_write_8(MEMORY_IO_START_ADDR + offset, registers.a);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Save A(0x%x) at 0x%x\n", registers.a, MEMORY_IO_START_ADDR + offset);
        }
#endif
        registers.pc += 2;
        clock_cycles = 12;
        break;

    case 0xe1: // POP HL - RR=(SP), SP=SP+2
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - POP HL\n", registers.pc, inst);
        }
#endif
        registers.hl = memory_read_16(registers.sp);
        registers.sp += 2;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Pop 0x%x in HL\n", registers.hl);
        }
#endif
        registers.pc++;
        clock_cycles = 12;
        break;

    case 0xe2: // LD (FF00+C), A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD (FF00+C),A\n", registers.pc, inst);
        }
#endif
        memory_write_8(MEMORY_IO_START_ADDR + registers.c, registers.a);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Save A(0x%x) at 0x%x\n", registers.a, MEMORY_IO_START_ADDR + registers.c);
        }
#endif
        registers.pc++;
        clock_cycles = 8;
        break;

    case 0xe5: // PUSH HL - SP=SP-2, (SP)=rr
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - PUSH HL\n", registers.pc, inst);
        }
#endif
        registers.sp -= 2;
        memory_write_16(registers.sp, registers.hl);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "PUSH HL(0x%x) at SP(0x%x)\n", registers.hl, registers.sp);
        }
#endif
        registers.pc++;
        clock_cycles = 16;
        break;

    case 0xe6: // AND nn, A=A & nn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - AND nn, A=A & n\n", registers.pc, inst);
        }
#endif
        registers.a &= memory_read_8(registers.pc + 1);
        registers.f = 0;
        SET_BIT(registers.f, FLAG_H);
        if (!registers.a)
            SET_BIT(registers.f, FLAG_Z);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set A to 0x%x\n", registers.a);
            print_flags();
        }
#endif
        registers.pc += 2;
        clock_cycles = 8;
        break;

    case 0xe9: // JP HL
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - JP HL\n", registers.pc, inst);
        }
#endif
        registers.pc = registers.hl;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Jump to HL(0x%x)\n", registers.pc);
            print_flags();
        }
#endif
        clock_cycles = 4;
        break;

    case 0xea: // LD (nnnn), A
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD (nnnn), A\n", registers.pc, inst);
        }
#endif
        addr = memory_read_16(registers.pc + 1);
        memory_write_8(addr, registers.a);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Save A(0x%x) at 0x%x\n", registers.a, addr);
        }
#endif
        registers.pc += 3;
        clock_cycles = 16;
        break;

    case 0xef: // RST 28 - Call to 0x28, SP=SP-2, (SP)=PC, PC=nnnn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - RST 28 - Call to 0x28\n", registers.pc, inst);
        }
#endif

        registers.sp -= 2;
        memory_write_16(registers.sp, registers.pc + 1);
        registers.pc = MEMORY_RST_28;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Save PC(0x%x) at 0x%x\n", memory_read_16(registers.sp), registers.sp);
            fprintf(stderr, P_INFO "Call to 0x%x\n", registers.pc);
        }
#endif
        clock_cycles = 16;
        break;

    case 0xf0: // LD A,(FF00+nn)
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD A,(FF00+nn)\n", registers.pc, inst);
        }
#endif
        offset = memory_read_8(registers.pc + 1);
        registers.a = memory_read_8(MEMORY_IO_START_ADDR + offset);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load 0x%x from 0x%x in A\n", registers.a, MEMORY_IO_START_ADDR + offset);
        }
#endif
        registers.pc += 2;
        clock_cycles = 12;
        break;

    case 0xf1: // POP AF - RR=(SP), SP=SP+2
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - POP AF\n", registers.pc, inst);
        }
#endif
        registers.af = memory_read_16(registers.sp);
        registers.sp += 2;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Pop 0x%x in AF\n", registers.af);
        }
#endif
        registers.pc++;
        clock_cycles = 12;
        break;

    case 0xf3: // DI
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - DI(Disable Interrupt)\n", registers.pc, inst);
        }
#endif
        registers.ime = false;

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set IME to 0\n");
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0xf5: // PUSH AF - SP=SP-2, (SP)=rr
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - PUSH AF\n", registers.pc, inst);
        }
#endif
        registers.sp -= 2;
        memory_write_16(registers.sp, registers.af);
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "PUSH AF(0x%x) at SP(0x%x)\n", registers.af, registers.sp);
        }
#endif
        registers.pc++;
        clock_cycles = 16;
        break;

    case 0xfa: // LD A, (nnnn)
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - LD A, (nnnn)\n", registers.pc, inst);
        }
#endif
        value_addr = memory_read_16(registers.pc + 1);
        registers.a = memory_read_8(value_addr);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Load 0x%x from 0x%x in A\n", registers.a, value_addr);
        }
#endif
        registers.pc += 3;
        clock_cycles = 16;
        break;

    case 0xfb: // EI
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - EI(Enable Interrupt)\n", registers.pc, inst);
        }
#endif
        registers.ime = true;

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Set IME to 1\n");
        }
#endif
        registers.pc++;
        clock_cycles = 4;
        break;

    case 0xfe: // CP nn - compare A-n
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - CP nn\n", registers.pc, inst);
        }
#endif
        mem_val = memory_read_8(registers.pc + 1);

        sub = (int16_t)registers.a - (int16_t)mem_val;

        SET_BIT(registers.f, FLAG_N);
        if (!sub)
            SET_BIT(registers.f, FLAG_Z);
        else
            CLEAR_BIT(registers.f, FLAG_Z);
        if (HAS_HALF_CARRY_ON_SUB(registers.a, sub))
            SET_BIT(registers.f, FLAG_H);
        else
            CLEAR_BIT(registers.f, FLAG_H);
        if (sub < 0)
            SET_BIT(registers.f, FLAG_C);
        else
            CLEAR_BIT(registers.f, FLAG_C);

#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "CP A(0x%x) to 0x%x\n", registers.a, mem_val);
            print_flags();
        }
#endif
        registers.pc += 2;
        clock_cycles = 8;
        break;

    case 0xff: // RST 38 - Call to 0x38, SP=SP-2, (SP)=PC, PC=nnnn
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO_INST "(0x%04x) Exec 0x%x - RST 38 - Call to 0x38\n", registers.pc, inst);
        }
#endif

        registers.sp -= 2;
        memory_write_16(registers.sp, registers.pc + 1);
        registers.pc = MEMORY_RST_38;
#ifdef DEBUG
        if (verbose & VERBOSE_CPU)
        {
            fprintf(stderr, P_INFO "Save PC(0x%x) at 0x%x\n", memory_read_16(registers.sp), registers.sp);
            fprintf(stderr, P_INFO "Call to 0x%x\n", registers.pc);
        }
#endif
        clock_cycles = 16;
        break;

    default:
        running = false;
        fprintf(stderr, P_FATAL "Successfully executed %ld instructions\n", nb_exec_inst);
        fprintf(stderr, P_FATAL "At 0x%x - Instruction 0x%02x not implemented yet!\n", registers.pc, inst);
        break;
    }

#ifdef DEBUG
    // Check Breakpoint
    breakpoint_check();
#endif

    nb_exec_inst++;
    return clock_cycles;
}