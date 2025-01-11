#include <ppu.h>

#include <memory.h>
#include <common.h>
#include <cpu.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <SDL2/SDL.h>

#define WINDOW_TILES_WIDTH 512
#define WINDOW_TILES_HEIGHT 512

static bool get_tile_data_start_addr(uint16_t *start_addr);
static void print_tiles(void);
static void print_bg_tiles_map(void);
static void print_window_tiles_map(void);

uint64_t scan_line_clock = 0;
static struct timeval time_last_frame;
static uint64_t diff_sum = 0;
static uint64_t nb_frame = 0;

// Tiles data in VRAM
static SDL_Window *pWindowTiles = NULL;
static SDL_Renderer *pRendererTiles = NULL;
static SDL_Texture *pTextureTiles = NULL;
static uint16_t frameBufferTiles[WINDOW_TILES_WIDTH * WINDOW_TILES_HEIGHT];

// Tiles BG Map
static SDL_Window *pWindowTilesBGMap = NULL;
static SDL_Renderer *pRendererTilesBGMap = NULL;
static SDL_Texture *pTextureTilesBGMap = NULL;
static uint16_t frameBufferTilesBGMap[WINDOW_TILES_WIDTH * WINDOW_TILES_HEIGHT];

// Tiles Window Map
static SDL_Window *pWindowTilesWindowMap = NULL;
static SDL_Renderer *pRendererTilesWindowMap = NULL;
static SDL_Texture *pTextureTilesWindowMap = NULL;
static uint16_t frameBufferTilesWindowMap[WINDOW_TILES_WIDTH * WINDOW_TILES_HEIGHT];

void ppu_init(void)
{
#ifdef DEBUG
    gettimeofday(&time_last_frame, NULL);

    pWindowTiles = SDL_CreateWindow("[DEBUG] Tiles", 0, 0, WINDOW_TILES_WIDTH, WINDOW_TILES_HEIGHT, 0);
    if (pWindowTiles == NULL)
    {
        fprintf(stderr, P_FATAL "Could not create Window\n");
        exit(EXIT_FAILURE);
    }

    pRendererTiles = SDL_CreateRenderer(pWindowTiles, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (pRendererTiles == NULL)
    {
        fprintf(stderr, P_FATAL "Could not create Renderer\n");
        exit(EXIT_FAILURE);
    }

    pTextureTiles = SDL_CreateTexture(
        pRendererTiles,
        SDL_PIXELFORMAT_ABGR1555,
        SDL_TEXTUREACCESS_STREAMING,
        WINDOW_TILES_WIDTH,
        WINDOW_TILES_HEIGHT);

    pWindowTilesBGMap = SDL_CreateWindow("[DEBUG] Tiles Background Map", 512, 0, WINDOW_TILES_WIDTH, WINDOW_TILES_HEIGHT, 0);
    if (pWindowTilesBGMap == NULL)
    {
        fprintf(stderr, P_FATAL "Could not create Window\n");
        exit(EXIT_FAILURE);
    }

    pRendererTilesBGMap = SDL_CreateRenderer(pWindowTilesBGMap, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (pRendererTilesBGMap == NULL)
    {
        fprintf(stderr, P_FATAL "Could not create Renderer\n");
        exit(EXIT_FAILURE);
    }

    pTextureTilesBGMap = SDL_CreateTexture(
        pRendererTilesBGMap,
        SDL_PIXELFORMAT_ABGR1555,
        SDL_TEXTUREACCESS_STREAMING,
        WINDOW_TILES_WIDTH,
        WINDOW_TILES_HEIGHT);

    pWindowTilesWindowMap = SDL_CreateWindow("[DEBUG] Tiles Window Map", 1024, 0, WINDOW_TILES_WIDTH, WINDOW_TILES_HEIGHT, 0);
    if (pWindowTilesWindowMap == NULL)
    {
        fprintf(stderr, P_FATAL "Could not create Window\n");
        exit(EXIT_FAILURE);
    }

    pRendererTilesWindowMap = SDL_CreateRenderer(pWindowTilesWindowMap, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (pRendererTilesWindowMap == NULL)
    {
        fprintf(stderr, P_FATAL "Could not create Renderer\n");
        exit(EXIT_FAILURE);
    }

    pTextureTilesWindowMap = SDL_CreateTexture(
        pRendererTilesWindowMap,
        SDL_PIXELFORMAT_ABGR1555,
        SDL_TEXTUREACCESS_STREAMING,
        WINDOW_TILES_WIDTH,
        WINDOW_TILES_HEIGHT);

    SDL_DisplayMode mode;
    mode.refresh_rate = FRAMERATE;
    SDL_SetWindowDisplayMode(pWindowTiles, &mode);
    SDL_SetWindowDisplayMode(pWindowTilesBGMap, &mode);
    SDL_SetWindowDisplayMode(pWindowTilesWindowMap, &mode);
#endif
}

void ppu_destroy(void)
{
#ifdef DEBUG
    if (pRendererTiles)
        SDL_DestroyRenderer(pRendererTiles);
    if (pWindowTiles)
        SDL_DestroyWindow(pWindowTiles);

    if (pRendererTilesBGMap)
        SDL_DestroyRenderer(pRendererTilesBGMap);
    if (pWindowTilesBGMap)
        SDL_DestroyWindow(pWindowTilesBGMap);
#endif
}

void ppu_execute(uint64_t clock_cycles)
{
    bool lcd_on = memory_get_reg_value(MEMORY_REG_LCDC, MEMORY_LCDC_PPU_ENABLED);
    uint8_t ly;
    memory_read(&ly, MEMORY_REG_LY, 1);

    if (!lcd_on)
    {
        // fprintf(stderr, P_PPU "LCD OFF\n");
        memory_write_8(MEMORY_REG_LY, 0);
        scan_line_clock = 0;
        return;
    }

    // Update Clock cycles
    scan_line_clock += clock_cycles;

    if (scan_line_clock >= CLOCK_CYCLES_PER_SCANLINE)
    {
        scan_line_clock = 0;
        ly = (ly == 153) ? 0 : ly + 1;
        memory_write_8(MEMORY_REG_LY, ly);

        // Render Scan lines (144 pixels in height)
        if (ly < 144)
        {
#ifdef DEBUG
            if (verbose & VERBOSE_PPU)
            {
                // fprintf(stderr, P_PPU "Rendered Line %u\n", ly);
            }
#endif
        }
        else if (ly == 144)
        {
#ifdef DEBUG
            if (verbose & VERBOSE_PPU)
            {
                fprintf(stderr, P_PPU "Request VBlank interrupt\n");
            }
            // if (memory_get_reg_value(MEMORY_REG_IF, MEMORY_IEF_VBLANK))
            // {
            print_tiles();
            print_bg_tiles_map();
            print_window_tiles_map();
            // SDL_Delay(50);

            // FPS
            struct timeval curr_time;
            gettimeofday(&curr_time, NULL);
            uint64_t diff = (curr_time.tv_sec - time_last_frame.tv_sec) * 1000000 + curr_time.tv_usec - time_last_frame.tv_usec;
            nb_frame++;
            diff_sum += diff;
            // fprintf(stderr, P_PPU "curr_time: ");
            fprintf(stderr, P_PPU "FPS: %g\n", (1 / (diff_sum / (double)nb_frame)) * 1000000);
            time_last_frame = curr_time;
            // }
#endif
            // End of frame
            // Request VBlank interrupt
            memory_write_reg_value(MEMORY_REG_IF, MEMORY_IEF_VBLANK, true);
        }

        // Check LYC == LY
        if (memory_read_8(MEMORY_REG_LYC) == ly)
        {
#ifdef DEBUG
            if (verbose & VERBOSE_PPU)
            {
                fprintf(stderr, P_PPU "LYC == LY\n");
            }
#endif
            memory_write_reg_value(MEMORY_REG_STAT, MEMORY_STAT_COINCID_FLAG, true);

            // If the LY == LYC interrupt is enabled, request it
            if (memory_get_reg_value(MEMORY_REG_STAT, MEMORY_STAT_COINCID_INT))
            {
                memory_write_reg_value(MEMORY_REG_IF, MEMORY_IEF_LCD_STAT, true);
            }
        }
        else
        {
            memory_write_reg_value(MEMORY_REG_STAT, MEMORY_STAT_COINCID_FLAG, false);
        }
    }
}

// Return True if the tile number is a signed integer
static bool get_tile_data_start_addr(uint16_t *start_addr)
{
    // Check Reg
    if (!memory_get_reg_value(MEMORY_REG_LCDC, MEMORY_LCDC_BG_AND_WINDOW_TILE_DATA_AREA))
    {
        *start_addr = 0x8000;
        return false;
    }
    else
    {
        *start_addr = 0x8800;
        return true;
    }
}

static void get_tile_bg_map_start_addr(uint16_t *start_addr)
{
    if (memory_get_reg_value(MEMORY_REG_LCDC, MEMORY_LCDC_BG_TILE_MAP_AREA))
        *start_addr = 0x9C00;
    else
        *start_addr = 0x9800;
}

static void get_tile_window_map_start_addr(uint16_t *start_addr)
{
    if (memory_get_reg_value(MEMORY_REG_LCDC, MEMORY_LCDC_WINDOW_TILE_MAP_AREA))
        *start_addr = 0x9C00;
    else
        *start_addr = 0x9800;
}

static void decode_tile_line(uint8_t line[2], uint8_t decoded_line[8])
{
    for (int8_t bit = 7; bit >= 0; bit--)
    {
        uint8_t mask = 1 << bit;
        uint8_t bit_right = (line[0] & mask) >> bit;
        uint8_t bit_left = (line[1] & mask) >> bit;

        decoded_line[7 - bit] = (bit_left << 1) | (bit_right);
    }
}

static void get_tile_from_index(uint8_t index, uint8_t tile[16])
{
    uint16_t start_addr;
    bool signed_addr = get_tile_data_start_addr(&start_addr);

    if (index < 128)
    {
        uint16_t offset = 0;
        if (signed_addr)
            offset = 128;

        memory_read(tile, start_addr + offset + index * 16, 16);
    }
    else
    {
        memory_read(tile, start_addr + index * 16, 16);
    }
}

static uint16_t rgba2abgr1555(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    /*
        ABGR1555 Encoding
                R         | G         | B         | A
        16bit | 1 1 1 1 1 | 1 1 1 1 1 | 1 1 1 1 1 | 1
    */

    float colorAjust = 0xff / 0x1f;

    uint16_t color = 0;
    color |= (alpha > 128) ? 1 : 0;
    color |= (uint16_t)(blue / colorAjust) << 1;
    color |= (uint16_t)(green / colorAjust) << 6;
    color |= (uint16_t)(red / colorAjust) << 11;

    return color;
}

static void print_tiles(void)
{
    uint16_t start_addr;
    bool signed_addr = get_tile_data_start_addr(&start_addr);
    // uint16_t start_addr = 0x8000;
    // bool signed_addr = false;
    if (verbose & VERBOSE_PPU)
        fprintf(stderr, P_PPU "Tiles start addr: 0x%x, Signed:%d\n", start_addr, signed_addr);

    uint16_t offset = 0;
    if (signed_addr)
        offset = 128;

    SDL_SetRenderDrawColor(pRendererTiles, 255, 255, 255, 0);
    SDL_RenderClear(pRendererTiles);

    // Display Bank 0/2
    for (uint16_t i = 0; i < 128; i++)
    {
        uint8_t tile[16];
        memory_read(tile, start_addr + offset + i * 16, 16);

        // for (uint8_t j = 0; j < 16; j++)
        //     fprintf(stderr, "%02x", tile[j]);
        // fprintf(stderr, "\n");

        for (uint8_t j = 0; j < 16; j += 2)
        {
            uint8_t decoded_line[8];
            decode_tile_line(&tile[j], decoded_line);

            for (uint8_t pixel = 0; pixel < 8; pixel++)
            {
                // fprintf(stderr, "%d ", decoded_line[pixel]);
                uint8_t tmp_color = decoded_line[pixel] * 85;
                uint16_t color = rgba2abgr1555(tmp_color, tmp_color, tmp_color, 255);

                uint32_t x = ((i % 16) * 8) + pixel;
                uint32_t y = ((i / 16) * 8) + (j / 2);

                SDL_Rect rect = {
                    x * 4,
                    y * 4,
                    4,
                    4};

                // SDL_SetRenderDrawColor(pRendererTiles, color, color, color, 255);
                // SDL_RenderFillRect(pRendererTiles, &rect);
                for (int64_t rect_y = rect.y; rect_y < rect.y + rect.h; rect_y++)
                {
                    for (int64_t rect_x = rect.x; rect_x < rect.x + rect.w; rect_x++)
                    {
                        frameBufferTiles[rect_y * WINDOW_TILES_WIDTH + rect_x] = color;
                    }
                }
            }
            // fprintf(stderr, "\n");
        }
    }

    // Display Bank 1
    for (uint16_t i = 0; i < 128; i++)
    {
        uint8_t tile[16];
        memory_read(tile, start_addr + 128 + i * 16, 16);

        // for (uint8_t j = 0; j < 16; j++)
        //     fprintf(stderr, "%02x", tile[j]);
        // fprintf(stderr, "\n");

        for (uint8_t j = 0; j < 16; j += 2)
        {
            uint8_t decoded_line[8];
            decode_tile_line(&tile[j], decoded_line);

            for (uint8_t pixel = 0; pixel < 8; pixel++)
            {
                uint8_t tmp_color = decoded_line[pixel] * 85;
                uint16_t color = rgba2abgr1555(tmp_color, tmp_color, tmp_color, 255);

                uint32_t x = ((i % 16) * 8) + pixel;
                uint32_t y = ((i / 16) * 8) + (j / 2);

                SDL_Rect rect = {
                    x * 4,
                    y * 4 + 128 * 2,
                    4,
                    4};

                // SDL_SetRenderDrawColor(pRendererTiles, color, color, color, 255);
                // SDL_RenderFillRect(pRendererTiles, &rect);
                for (int64_t rect_y = rect.y; rect_y < rect.y + rect.h; rect_y++)
                {
                    for (int64_t rect_x = rect.x; rect_x < rect.x + rect.w; rect_x++)
                    {
                        frameBufferTiles[rect_y * WINDOW_TILES_WIDTH + rect_x] = color;
                    }
                }
            }
        }
    }

    SDL_UpdateTexture(
        pTextureTiles,
        NULL,
        frameBufferTiles,
        WINDOW_TILES_WIDTH * sizeof(uint16_t));

    SDL_RenderCopy(pRendererTiles, pTextureTiles, NULL, NULL);
    SDL_RenderPresent(pRendererTiles);
}

static void print_bg_tiles_map(void)
{
    uint16_t bg_map_addr;
    get_tile_bg_map_start_addr(&bg_map_addr);

    if (verbose & VERBOSE_PPU)
        fprintf(stderr, P_PPU "Tiles BG Map start addr: 0x%x\n", bg_map_addr);

    SDL_SetRenderDrawColor(pRendererTilesBGMap, 255, 255, 255, 0);
    SDL_RenderClear(pRendererTilesBGMap);

    for (uint8_t y = 0; y < 32; y++)
    {
        for (uint8_t x = 0; x < 32; x++)
        {
            // fprintf(stderr, "%02x", memory_read_8(bg_map_addr + x + y));
            uint8_t tile[16];
            uint8_t index = memory_read_8(bg_map_addr + y * 32 + x);

            get_tile_from_index(index, tile);

            for (uint8_t j = 0; j < 16; j += 2)
            {
                uint8_t decoded_line[8];
                decode_tile_line(&tile[j], decoded_line);

                for (uint8_t pixel = 0; pixel < 8; pixel++)
                {
                    // fprintf(stderr, "%d ", decoded_line[pixel]);
                    uint8_t tmp_color = decoded_line[pixel] * 85;
                    uint16_t color = rgba2abgr1555(tmp_color, tmp_color, tmp_color, 255);

                    uint32_t x_display = (x * 8) + pixel;
                    uint32_t y_display = (y * 8) + (j / 2);

                    SDL_Rect rect = {
                        x_display * 2,
                        y_display * 2,
                        2,
                        2};

                    // SDL_SetRenderDrawColor(pRendererTilesBGMap, color, color, color, 255);
                    // SDL_RenderFillRect(pRendererTilesBGMap, &rect);
                    for (int64_t rect_y = rect.y; rect_y < rect.y + rect.h; rect_y++)
                    {
                        for (int64_t rect_x = rect.x; rect_x < rect.x + rect.w; rect_x++)
                        {
                            frameBufferTilesBGMap[rect_y * WINDOW_TILES_WIDTH + rect_x] = color;
                        }
                    }
                }
                // fprintf(stderr, "\n");
            }
        }
        // fprintf(stderr, "\n");
    }

    SDL_UpdateTexture(
        pTextureTilesBGMap,
        NULL,
        frameBufferTilesBGMap,
        WINDOW_TILES_WIDTH * sizeof(uint16_t));

    SDL_RenderCopy(pRendererTilesBGMap, pTextureTilesBGMap, NULL, NULL);
    SDL_RenderPresent(pRendererTilesBGMap);
}

static void print_window_tiles_map(void)
{
    uint16_t window_map_addr;
    get_tile_window_map_start_addr(&window_map_addr);

    if (verbose & VERBOSE_PPU)
        fprintf(stderr, P_PPU "Tiles Window Map start addr: 0x%x\n", window_map_addr);

    SDL_SetRenderDrawColor(pRendererTilesWindowMap, 255, 255, 255, 0);
    SDL_RenderClear(pRendererTilesWindowMap);

    for (uint8_t y = 0; y < 32; y++)
    {
        for (uint8_t x = 0; x < 32; x++)
        {
            // fprintf(stderr, "%02x", memory_read_8(window_map_addr + x + y));
            uint8_t tile[16];
            uint8_t index = memory_read_8(window_map_addr + y * 32 + x);

            get_tile_from_index(index, tile);

            for (uint8_t j = 0; j < 16; j += 2)
            {
                uint8_t decoded_line[8];
                decode_tile_line(&tile[j], decoded_line);

                for (uint8_t pixel = 0; pixel < 8; pixel++)
                {
                    // fprintf(stderr, "%d ", decoded_line[pixel]);
                    uint8_t tmp_color = decoded_line[pixel] * 85;
                    uint16_t color = rgba2abgr1555(tmp_color, tmp_color, tmp_color, 255);

                    uint32_t x_display = (x * 8) + pixel;
                    uint32_t y_display = (y * 8) + (j / 2);

                    SDL_Rect rect = {
                        x_display * 2,
                        y_display * 2,
                        2,
                        2};

                    // SDL_SetRenderDrawColor(pRendererTilesWindowMap, color, color, color, 255);
                    // SDL_RenderFillRect(pRendererTilesWindowMap, &rect);

                    for (int64_t rect_y = rect.y; rect_y < rect.y + rect.h; rect_y++)
                    {
                        for (int64_t rect_x = rect.x; rect_x < rect.x + rect.w; rect_x++)
                        {
                            frameBufferTilesWindowMap[rect_y * WINDOW_TILES_WIDTH + rect_x] = color;
                        }
                    }
                }
                // fprintf(stderr, "\n");
            }
        }
        // fprintf(stderr, "\n");
    }

    SDL_UpdateTexture(
        pTextureTilesWindowMap,
        NULL,
        frameBufferTilesWindowMap,
        WINDOW_TILES_WIDTH * sizeof(uint16_t));

    SDL_RenderCopy(pRendererTilesWindowMap, pTextureTilesWindowMap, NULL, NULL);
    SDL_RenderPresent(pRendererTilesWindowMap);
}