// RISC-V emulator with SDL2 support - displays console output graphically
// This version captures UART output and displays it as text in SDL window
// Perfect for running ASCII art DOOM!

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

// Check for SDL2
#ifdef __APPLE__
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

// Configuration
#define MINIRV32WARN( x... ) fprintf( stderr, x );
#define MINIRV32_RAM_IMAGE_OFFSET  0x80000000
#define MINIRV32_RAM_DEFAULT_SIZE   (64*1024*1024)
#define MINI_RV32_RAM_SIZE          MINIRV32_RAM_DEFAULT_SIZE
#define MINIRV32_MMIO_RANGE_LOW     0x10000000
#define MINIRV32_MMIO_RANGE_HIGH    0x12000000
#define MINIRV32_OTHERCSR_BASE      0x5000000

// Console display configuration
#define CONSOLE_WIDTH  80
#define CONSOLE_HEIGHT 25
#define CHAR_WIDTH     8
#define CHAR_HEIGHT    16
#define WINDOW_WIDTH   (CONSOLE_WIDTH * CHAR_WIDTH)
#define WINDOW_HEIGHT  (CONSOLE_HEIGHT * CHAR_HEIGHT)

// SDL objects
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static uint32_t *framebuffer = NULL;

// Console buffer
static char console_buffer[CONSOLE_HEIGHT][CONSOLE_WIDTH];
static int console_x = 0, console_y = 0;

// Forward declarations
static uint32_t HandleControlStore(uint32_t addy, uint32_t val);
static uint32_t HandleControlLoad(uint32_t addy);
static void HandleOtherCSRWrite(struct MiniRV32IMAState *state, uint32_t csrno, uint32_t value);
static uint32_t HandleOtherCSRRead(struct MiniRV32IMAState *state, uint32_t csrno);

// Include the main emulator
#define MINIRV32_IMPLEMENTATION
#define MINIRV32_DECORATE static
#define MINIRV32_STORE4( addr, val ) *(uint32_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET) = val
#define MINIRV32_STORE2( addr, val ) *(uint16_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET) = val  
#define MINIRV32_STORE1( addr, val ) *(uint8_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET) = val
#define MINIRV32_LOAD4( addr ) (*(uint32_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET))
#define MINIRV32_LOAD2( addr ) (*(uint16_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET))
#define MINIRV32_LOAD1( addr ) (*(uint8_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET))
#define MINIRV32_LOAD2_SIGNED( addr ) ((int16_t)*(uint16_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET))
#define MINIRV32_LOAD1_SIGNED( addr ) ((int8_t)*(uint8_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET))

#include "mini-rv32ima.h"

static uint8_t* ram_image = 0;
static struct MiniRV32IMAState core;

// Simple 8x16 font data (bitmap for basic ASCII characters)
static const uint8_t font_8x16[] = {
    // Character 32 (space)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Character 33 (!)
    0x00, 0x00, 0x18, 0x3C, 0x3C, 0x3C, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
    // More characters would go here... for now just handle basic printable ASCII
};

void put_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color) {
    if (x >= CONSOLE_WIDTH || y >= CONSOLE_HEIGHT) return;
    
    // Simple character rendering - just draw colored blocks for now
    int start_x = x * CHAR_WIDTH;
    int start_y = y * CHAR_HEIGHT;
    
    // Background
    for (int dy = 0; dy < CHAR_HEIGHT; dy++) {
        for (int dx = 0; dx < CHAR_WIDTH; dx++) {
            framebuffer[(start_y + dy) * WINDOW_WIDTH + (start_x + dx)] = bg_color;
        }
    }
    
    // Simple character patterns
    uint32_t color = fg_color;
    if (c >= 32 && c < 127) {
        // Draw simple patterns based on character
        switch(c) {
            case ' ': break; // space - just background
            case '#': case '@': case '%': case '&': case '*':
                // Dense characters - fill most of the space
                for (int dy = 1; dy < CHAR_HEIGHT-1; dy++) {
                    for (int dx = 1; dx < CHAR_WIDTH-1; dx++) {
                        framebuffer[(start_y + dy) * WINDOW_WIDTH + (start_x + dx)] = color;
                    }
                }
                break;
            case '.': case ',': case ':': case ';':
                // Small dots
                framebuffer[(start_y + CHAR_HEIGHT-3) * WINDOW_WIDTH + (start_x + 3)] = color;
                framebuffer[(start_y + CHAR_HEIGHT-3) * WINDOW_WIDTH + (start_x + 4)] = color;
                break;
            case '|':
                // Vertical line
                for (int dy = 2; dy < CHAR_HEIGHT-2; dy++) {
                    framebuffer[(start_y + dy) * WINDOW_WIDTH + (start_x + CHAR_WIDTH/2)] = color;
                }
                break;
            case '-': case '_':
                // Horizontal line
                for (int dx = 1; dx < CHAR_WIDTH-1; dx++) {
                    framebuffer[(start_y + CHAR_HEIGHT/2) * WINDOW_WIDTH + (start_x + dx)] = color;
                }
                break;
            default:
                // For other characters, draw a simple block pattern
                if (c >= 'A' && c <= 'Z') {
                    // Letters - draw upper block
                    for (int dy = 2; dy < CHAR_HEIGHT/2; dy++) {
                        for (int dx = 2; dx < CHAR_WIDTH-2; dx++) {
                            framebuffer[(start_y + dy) * WINDOW_WIDTH + (start_x + dx)] = color;
                        }
                    }
                } else if (c >= 'a' && c <= 'z') {
                    // Lower case - draw lower block
                    for (int dy = CHAR_HEIGHT/2; dy < CHAR_HEIGHT-2; dy++) {
                        for (int dx = 2; dx < CHAR_WIDTH-2; dx++) {
                            framebuffer[(start_y + dy) * WINDOW_WIDTH + (start_x + dx)] = color;
                        }
                    }
                } else {
                    // Numbers and symbols - draw middle block
                    for (int dy = 4; dy < CHAR_HEIGHT-4; dy++) {
                        for (int dx = 2; dx < CHAR_WIDTH-2; dx++) {
                            framebuffer[(start_y + dy) * WINDOW_WIDTH + (start_x + dx)] = color;
                        }
                    }
                }
                break;
        }
    }
}

void console_put_char(char c) {
    if (c == '\n') {
        console_x = 0;
        console_y++;
        if (console_y >= CONSOLE_HEIGHT) {
            // Scroll up
            memmove(console_buffer[0], console_buffer[1], (CONSOLE_HEIGHT-1) * CONSOLE_WIDTH);
            memset(console_buffer[CONSOLE_HEIGHT-1], ' ', CONSOLE_WIDTH);
            console_y = CONSOLE_HEIGHT - 1;
        }
    } else if (c == '\r') {
        console_x = 0;
    } else if (c == '\b') {
        if (console_x > 0) console_x--;
    } else if (c >= 32 && c < 127) {
        if (console_x < CONSOLE_WIDTH) {
            console_buffer[console_y][console_x] = c;
            console_x++;
            if (console_x >= CONSOLE_WIDTH) {
                console_x = 0;
                console_y++;
                if (console_y >= CONSOLE_HEIGHT) {
                    // Scroll up
                    memmove(console_buffer[0], console_buffer[1], (CONSOLE_HEIGHT-1) * CONSOLE_WIDTH);
                    memset(console_buffer[CONSOLE_HEIGHT-1], ' ', CONSOLE_WIDTH);
                    console_y = CONSOLE_HEIGHT - 1;
                }
            }
        }
    }
}

void update_console_display() {
    // Clear framebuffer
    memset(framebuffer, 0, WINDOW_WIDTH * WINDOW_HEIGHT * 4);
    
    // Render console buffer
    for (int y = 0; y < CONSOLE_HEIGHT; y++) {
        for (int x = 0; x < CONSOLE_WIDTH; x++) {
            char c = console_buffer[y][x];
            uint32_t fg = 0xFF00FF00; // Green text
            uint32_t bg = 0xFF000000; // Black background
            put_char(x, y, c, fg, bg);
        }
    }
    
    // Update SDL texture and render
    SDL_UpdateTexture(texture, NULL, framebuffer, WINDOW_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

static uint64_t GetTimeMicroseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

static uint32_t HandleControlStore(uint32_t addy, uint32_t val) {
    if (addy == 0x10000000) {
        // UART TX - display character
        char c = val & 0xff;
        console_put_char(c);
        update_console_display();
        return 0;
    }
    return 0;
}

static uint32_t HandleControlLoad(uint32_t addy) {
    if (addy == 0x10000005) {
        // UART status - always ready
        return 0x60;
    }
    return 0;
}

static void HandleOtherCSRWrite(struct MiniRV32IMAState *state, uint32_t csrno, uint32_t value) {
    // Handle CSR writes
}

static uint32_t HandleOtherCSRRead(struct MiniRV32IMAState *state, uint32_t csrno) {
    if (csrno == 0xC00 || csrno == 0xC01) { // rdcycle, rdtime
        return GetTimeMicroseconds() / 1000;
    }
    return 0;
}

int InitSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
        return -1;
    }
    
    window = SDL_CreateWindow("RISC-V Console DOOM",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        return -1;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
        return -1;
    }
    
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING,
                               WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!texture) {
        fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
        return -1;
    }
    
    framebuffer = (uint32_t*)calloc(WINDOW_WIDTH * WINDOW_HEIGHT, sizeof(uint32_t));
    if (!framebuffer) {
        fprintf(stderr, "Failed to allocate framebuffer\n");
        return -1;
    }
    
    // Initialize console buffer
    for (int y = 0; y < CONSOLE_HEIGHT; y++) {
        for (int x = 0; x < CONSOLE_WIDTH; x++) {
            console_buffer[y][x] = ' ';
        }
    }
    
    return 0;
}

void CleanupSDL() {
    if (framebuffer) free(framebuffer);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image.bin>\n", argv[0]);
        return 1;
    }
    
    printf("RISC-V Console DOOM Emulator with SDL2\n");
    printf("RAM: 64 MB\n");
    
    // Initialize SDL
    if (InitSDL() < 0) {
        return 1;
    }
    
    // Load image
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", argv[1]);
        CleanupSDL();
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long image_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    printf("Image: %s (%ld bytes)\n", argv[1], image_size);
    
    // Allocate RAM
    ram_image = (uint8_t*)malloc(MINIRV32_RAM_DEFAULT_SIZE);
    if (!ram_image) {
        fprintf(stderr, "Failed to allocate RAM\n");
        fclose(f);
        CleanupSDL();
        return 1;
    }
    
    // Load image into RAM
    if (fread(ram_image, 1, image_size, f) != image_size) {
        fprintf(stderr, "Failed to read image\n");
        fclose(f);
        free(ram_image);
        CleanupSDL();
        return 1;
    }
    fclose(f);
    
    // Setup DTB (Device Tree Blob) - simplified
    uint32_t dtb_ptr = MINIRV32_RAM_DEFAULT_SIZE - 0x200000; // Place DTB at end of RAM
    printf("DTB at: 0x%08x\n", MINIRV32_RAM_IMAGE_OFFSET + dtb_ptr);
    
    // Initialize core
    memset(&core, 0, sizeof(core));
    core.pc = MINIRV32_RAM_IMAGE_OFFSET;
    core.regs[11] = dtb_ptr + MINIRV32_RAM_IMAGE_OFFSET; // DTB pointer in a1
    
    printf("Press ESC to quit\n\n");
    
    int running = 1;
    SDL_Event event;
    
    while (running) {
        // Handle SDL events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || 
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            }
        }
        
        // Run emulator steps
        for (int i = 0; i < 1024 && running; i++) {
            int ret = MiniRV32IMAStep(&core, ram_image, 0, 0, 0);
            if (ret) {
                running = 0;
                printf("\nCPU halted at PC=0x%08x\n", core.pc);
                break;
            }
        }
        
        // Small delay
        SDL_Delay(1);
    }
    
    printf("\nEmulation ended. Total instructions: %u\n", core.extraflags);
    
    free(ram_image);
    CleanupSDL();
    return 0;
}