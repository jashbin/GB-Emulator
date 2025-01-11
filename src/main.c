#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include <memory.h>
#include <common.h>
#include <cpu.h>
#include <ppu.h>
#include <cartridge.h>
#include <timer.h>

static void print_usage(const char *filename)
{
    fprintf(stderr, "Usage: %s <ROM>\n", filename);
}

static void print_banner(void)
{
    fprintf(stdout,
            "   _____                      ____                ______                 _       _             \n"
            "  / ____|                    |  _ \\              |  ____|               | |     | |            \n"
            " | |  __  __ _ _ __ ___   ___| |_) | ___  _   _  | |__   _ __ ___  _   _| | __ _| |_ ___  _ __ \n"
            " | | |_ |/ _` | '_ ` _ \\ / _ \\  _ < / _ \\| | | | |  __| | '_ ` _ \\| | | | |/ _` | __/ _ \\| '__|\n"
            " | |__| | (_| | | | | | |  __/ |_) | (_) | |_| | | |____| | | | | | |_| | | (_| | || (_) | |   \n"
            "  \\_____|\\__,_|_| |_| |_|\\___|____/ \\___/ \\__, | |______|_| |_| |_|\\__,_|_|\\__,_|\\__\\___/|_|   \n"
            "                                           __/ |                                               \n"
            "  _               _           _     _     |___/                                                \n"
            " | |             (_)         | |   | |   (_)                                                   \n"
            " | |__  _   _     _  __ _ ___| |__ | |__  _ _ __                                               \n"
            " | '_ \\| | | |   | |/ _` / __| '_ \\| '_ \\| | '_ \\                                              \n"
            " | |_) | |_| |   | | (_| \\__ \\ | | | |_) | | | | |                                             \n"
            " |_.__/ \\__, |   | |\\__,_|___/_| |_|_.__/|_|_| |_|                                             \n"
            "         __/ |  _/ |                                                                           \n"
            "        |___/  |__/                                                                            \n");

    fprintf(stdout, "Welcome to the GameBoy Emulator by jashbin!\n");
}

int main(int argc, char const *argv[])
{
    uint64_t clock_cycles = 0;

    if (argc != 2)
    {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    print_banner();

    fprintf(stdout, "Initializing SDL...");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
    {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    fprintf(stdout, "OK\n");

    fprintf(stdout, "Loading ROM...\n");
    cartridge_load_rom(argv[1]);
    cartridge_print_infos();

    fprintf(stdout, "Initializing Components...\n");
    cpu_init();
    memory_init();
    ppu_init();

    fprintf(stdout, "Starting CPU...\n");
    SDL_Event events;
    bool isRunning = true;
    while (cpu_is_running() && isRunning)
    {
        // Input
        while (SDL_PollEvent(&events))
        {
            switch (events.type)
            {
            case SDL_KEYDOWN:
                switch (events.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    isRunning = false;
                    break;
                }
                break;
            }
        }

#ifdef DEBUG
        cpu_debugger();
        if (!cpu_is_running())
            break;
#endif

        clock_cycles = cpu_execute_inst();
        ppu_execute(clock_cycles);
        timer_execute(clock_cycles);
        // interrupt_execute(clock_cycles?) Probablement mettre ça dans le cpu
    }

    // fprintf(stdout, "Destroying Components...\n");
    ppu_destroy();

    SDL_Quit();

    return EXIT_SUCCESS;
}
