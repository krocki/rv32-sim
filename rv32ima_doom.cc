// RISC-V emulator with SDL2 support for DOOM
// Based on cnlohr's mini-rv32ima

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

// Forward declarations
static uint32_t HandleControlStore(uint32_t addy, uint32_t val);
static uint32_t HandleControlLoad(uint32_t addy);
static void HandleOtherCSRWrite(struct MiniRV32IMAState *state, uint32_t csrno, uint32_t value);
static uint32_t HandleOtherCSRRead(struct MiniRV32IMAState *state, uint32_t csrno);

// Default DTB for 64MB RAM
static uint32_t default_dtb[] = {
    0xedfe0dd0, 0x38030000, 0x38020000, 0x38000000,
    0x28000000, 0x11000000, 0x10000000, 0x00000000,
    0x4d010000, 0x00000000, 0x00010000, 0x00000000,
    0x03000000, 0x04000000, 0x00000000, 0x02000000,
    0x03000000, 0x04000000, 0x0f000000, 0x02000000,
    0x01000000, 0x726f6d65, 0x0079006d, 0x00000000,
    0x03000000, 0x00000400, 0x20000000, 0x00000008,
    0x02000000, 0x01000000, 0x6d697300, 0x00656c70,
    0x03000000, 0x10000000, 0x25000000, 0x616c632f,
    0x30407373, 0x00000000, 0x02000000, 0x02000000,
    0x02000000, 0x01000000, 0x736f6863, 0x00006e65,
    0x03000000, 0x04000000, 0x2d000000, 0x00000001,
    0x03000000, 0x04000000, 0x36000000, 0x6d697200,
    0x00000000, 0x01000000, 0x736f6863, 0x00006e65,
    0x03000000, 0x04000000, 0x2d000000, 0x00000000,
    0x03000000, 0x04000000, 0x36000000, 0x6b636f73,
    0x00007465, 0x03000000, 0x08000000, 0x24000000,
    0x00000011, 0x00005000, 0x02000000, 0x02000000,
    0x01000000, 0x6f730063, 0x00000063, 0x03000000,
    0x04000000, 0x00000000, 0x02000000, 0x03000000,
    0x04000000, 0x0f000000, 0x00000000, 0x03000000,
    0x04000000, 0x36000000, 0x6373696d, 0x00000076,
    0x03000000, 0x08000000, 0x24000000, 0x00000211,
    0x00004b00, 0x03000000, 0x0d000000, 0x40000000,
    0x6e617571, 0x312d6d74, 0x00000030, 0x00000000,
    0x02000000, 0x02000000, 0x02000000, 0x01000000,
    0x00757063, 0x03000000, 0x04000000, 0x00000000,
    0x00000000, 0x03000000, 0x04000000, 0x0f000000,
    0x00000000, 0x03000000, 0x04000000, 0x36000000,
    0x63736972, 0x00000076, 0x03000000, 0x05000000,
    0x4b000000, 0x00007675, 0x00003233, 0x03000000,
    0x04000000, 0x4f000000, 0x00000001, 0x03000000,
    0x3a000000, 0x55000000, 0x33327672, 0x61616d69,
    0x7a5f3270, 0x73637269, 0x70305f72, 0x66697a5f,
    0x65636e65, 0x70305f69, 0x6373697a, 0x70305f72,
    0x00000000, 0x03000000, 0x0a000000, 0x5f000000,
    0x30327672, 0x00616d69, 0x00000000, 0x03000000,
    0x10000000, 0x1b000000, 0x00000000, 0x80000000,
    0x00000000, 0x00000004, 0x03000000, 0x04000000,
    0x6b000000, 0x00000010, 0x01000000, 0x00757063,
    0x03000000, 0x04000000, 0x00000000, 0x00000000,
    0x03000000, 0x04000000, 0x0f000000, 0x00000000,
    0x03000000, 0x04000000, 0x36000000, 0x63736972,
    0x00000076, 0x03000000, 0x05000000, 0x4b000000,
    0x00007675, 0x00003233, 0x03000000, 0x04000000,
    0x4f000000, 0x00000000, 0x03000000, 0x3a000000,
    0x55000000, 0x33327672, 0x61616d69, 0x7a5f3270,
    0x73637269, 0x70305f72, 0x66697a5f, 0x65636e65,
    0x70305f69, 0x6373697a, 0x70305f72, 0x00000000,
    0x03000000, 0x0a000000, 0x5f000000, 0x30327672,
    0x00616d69, 0x00000000, 0x03000000, 0x0f000000,
    0x1b000000, 0x20637075, 0x74734000, 0x74726175,
    0x00000000, 0x01000000, 0x75706300, 0x00000000,
    0x03000000, 0x04000000, 0x74000000, 0x00000001,
    0x03000000, 0x04000000, 0x36000000, 0x00757063,
    0x03000000, 0x00000400, 0x77000000, 0x00000002,
    0x02000000, 0x02000000, 0x01000000, 0x7275746e, 0x63746e69,
    0x02000000, 0x02000000, 0x01000000, 0x75706300, 0x00000073,
    0x03000000, 0x04000000, 0x00000000, 0x00000001,
    0x03000000, 0x04000000, 0x0f000000, 0x00000001,
    0x03000000, 0x04000000, 0x36000000, 0x00757063,
    0x02000000, 0x02000000, 0x02000000, 0x01000000
};

// Global state (needed by macros)
static uint8_t *ram_image = 0;

// Memory access macros
#define MINIRV32_STORE4( addr, val ) *(uint32_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET) = val
#define MINIRV32_STORE2( addr, val ) *(uint16_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET) = val  
#define MINIRV32_STORE1( addr, val ) *(uint8_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET) = val
#define MINIRV32_LOAD4( addr ) (*(uint32_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET))
#define MINIRV32_LOAD2( addr ) (*(uint16_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET))
#define MINIRV32_LOAD1( addr ) (*(uint8_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET))
#define MINIRV32_LOAD2_SIGNED( addr ) ((int16_t)*(uint16_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET))
#define MINIRV32_LOAD1_SIGNED( addr ) ((int8_t)*(uint8_t*)(ram_image + (addr) - MINIRV32_RAM_IMAGE_OFFSET))

// Mini-rv32ima header
#define MINIRV32_IMPLEMENTATION
#define MINIRV32_HANDLE_MEM_STORE_CONTROL( addy, val ) if( HandleControlStore( addy, val ) ) return val;
#define MINIRV32_HANDLE_MEM_LOAD_CONTROL( addy, rval ) rval = HandleControlLoad( addy );
#define MINIRV32_OTHERCSR_WRITE( csrno, value ) HandleOtherCSRWrite( NULL, csrno, value );
#define MINIRV32_OTHERCSR_READ( csrno, value ) value = HandleOtherCSRRead( NULL, csrno );

#include "mini-rv32ima.h"

// Global state
static struct MiniRV32IMAState *core;
static uint32_t ram_amt = MINIRV32_RAM_DEFAULT_SIZE;
static int fail_on_all_faults = 0;
static uint64_t lastTime = 0;
static int time_divisor = 1;

// SDL2 state
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static uint32_t *framebuffer = NULL;
static int fb_width = 640;
static int fb_height = 480;
static int should_quit = 0;

// Framebuffer at 0x11100000
#define FRAMEBUFFER_BASE 0x11100000
#define FRAMEBUFFER_SIZE (640 * 480 * 4)

static uint64_t GetTimeMicroseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_usec + ((uint64_t)(tv.tv_sec)) * 1000000LL;
}

static uint32_t HandleControlStore(uint32_t addy, uint32_t val) {
    // UART at 0x11000000
    if (addy == 0x11000000) {
        putchar(val & 0xFF);
        fflush(stdout);
        return 0;
    }
    
    // Framebuffer writes
    if (addy >= FRAMEBUFFER_BASE && addy < FRAMEBUFFER_BASE + FRAMEBUFFER_SIZE) {
        uint32_t offset = (addy - FRAMEBUFFER_BASE) / 4;
        if (offset < (fb_width * fb_height)) {
            framebuffer[offset] = val;
        }
        return 0;
    }
    
    // System control at 0x11300000
    if (addy == 0x11300000 && val == 0x5555) {
        printf("System shutdown requested\n");
        should_quit = 1;
        return 0;
    }
    
    return 0; // Success
}

static uint32_t HandleControlLoad(uint32_t addy) {
    // UART status at 0x11000005
    if (addy == 0x11000005) {
        return 0x60; // Always ready to transmit
    }
    
    // Timer at 0x10000000
    if (addy == MINIRV32_MMIO_RANGE_LOW + 0xBFF8) {
        uint64_t now = GetTimeMicroseconds();
        return (now / time_divisor) & 0xFFFFFFFF;
    }
    if (addy == MINIRV32_MMIO_RANGE_LOW + 0xBFFC) {
        uint64_t now = GetTimeMicroseconds();
        return ((now / time_divisor) >> 32) & 0xFFFFFFFF;
    }
    
    // Framebuffer reads
    if (addy >= FRAMEBUFFER_BASE && addy < FRAMEBUFFER_BASE + FRAMEBUFFER_SIZE) {
        uint32_t offset = (addy - FRAMEBUFFER_BASE) / 4;
        if (offset < (fb_width * fb_height)) {
            return framebuffer[offset];
        }
        return 0;
    }
    
    return 0; // Default value for unmapped reads
}

static void HandleOtherCSRWrite(struct MiniRV32IMAState *state, uint32_t csrno, uint32_t value) {
    if (csrno == 0x136) {
        printf("CSR: %d\n", value);
    }
}

static uint32_t HandleOtherCSRRead(struct MiniRV32IMAState *state, uint32_t csrno) {
    if (csrno == 0x136) {
        return 0;
    }
    return 0;
}

static int InitSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    
    window = SDL_CreateWindow("RISC-V DOOM",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        fb_width, fb_height, SDL_WINDOW_SHOWN);
    
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return -1;
    }
    
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, fb_width, fb_height);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return -1;
    }
    
    framebuffer = (uint32_t*)calloc(fb_width * fb_height, sizeof(uint32_t));
    if (!framebuffer) {
        fprintf(stderr, "Failed to allocate framebuffer\n");
        return -1;
    }
    
    // Clear screen to black
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    
    return 0;
}

static void UpdateDisplay() {
    SDL_UpdateTexture(texture, NULL, framebuffer, fb_width * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

static void HandleSDLEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                should_quit = 1;
                break;
            case SDL_KEYDOWN:
                // TODO: Map keyboard to UART input
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    should_quit = 1;
                }
                break;
        }
    }
}

static void CleanupSDL() {
    if (framebuffer) free(framebuffer);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char **argv) {
    int do_sleep = 1;
    int fixed_update = 0;
    int dtb_ptr = 0;
    const char *image_file = NULL;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) {
            fixed_update = 1;
        } else if (strcmp(argv[i], "-n") == 0) {
            do_sleep = 0;
        } else if (strcmp(argv[i], "-s") == 0) {
            time_divisor = 2;  // Slow down time
        } else if (!image_file) {
            image_file = argv[i];
        }
    }
    
    if (!image_file) {
        fprintf(stderr, "Usage: %s [-f] [-n] [-s] image.bin\n", argv[0]);
        fprintf(stderr, "  -f: Fixed update rate\n");
        fprintf(stderr, "  -n: No sleep\n");
        fprintf(stderr, "  -s: Slow time\n");
        return 1;
    }
    
    // Load kernel image
    FILE *f = fopen(image_file, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open image file: %s\n", image_file);
        return 2;
    }
    
    fseek(f, 0, SEEK_END);
    long flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (flen > ram_amt) {
        fprintf(stderr, "Image too large (%ld bytes, max %d)\n", flen, ram_amt);
        return 3;
    }
    
    ram_image = (uint8_t*)calloc(ram_amt, 1);
    if (!ram_image) {
        fprintf(stderr, "Failed to allocate RAM\n");
        return 4;
    }
    
    if (fread(ram_image, 1, flen, f) != flen) {
        fprintf(stderr, "Failed to read image\n");
        return 5;
    }
    fclose(f);
    
    // Copy DTB to end of RAM (at 62MB for 64MB RAM)
    dtb_ptr = 62 * 1024 * 1024;
    if (dtb_ptr + sizeof(default_dtb) < ram_amt) {
        memcpy(ram_image + dtb_ptr, default_dtb, sizeof(default_dtb));
    }
    
    // Initialize SDL
    if (InitSDL() < 0) {
        fprintf(stderr, "Failed to initialize SDL\n");
        return 6;
    }
    
    // Initialize CPU
    core = (struct MiniRV32IMAState*)calloc(sizeof(struct MiniRV32IMAState), 1);
    core->pc = MINIRV32_RAM_IMAGE_OFFSET;
    core->regs[10] = 0x00; // hart ID
    core->regs[11] = MINIRV32_RAM_IMAGE_OFFSET + dtb_ptr; // DTB pointer
    
    printf("RISC-V Emulator with SDL2 Display\n");
    printf("RAM: %d MB\n", ram_amt / (1024*1024));
    printf("Image: %s (%ld bytes)\n", image_file, flen);
    printf("DTB at: 0x%08x\n", MINIRV32_RAM_IMAGE_OFFSET + dtb_ptr);
    printf("Press ESC to quit\n\n");
    
    lastTime = GetTimeMicroseconds();
    int instct = 0;
    int display_counter = 0;
    
    // Main emulation loop
    while (!should_quit) {
        uint64_t now = GetTimeMicroseconds();
        uint64_t elapsed = now - lastTime;
        
        if (elapsed > 1000) { // Run in ~1ms chunks
            int instructions = fixed_update ? 1024 : (elapsed * 3); // ~3 MIPS
            
            instct += MiniRV32IMAStep(core, ram_image, 0, elapsed, instructions);
            
            lastTime = now;
            
            if (do_sleep) {
                usleep(1000);
            }
        }
        
        // Update display every ~16ms (60 FPS)
        if (++display_counter > 16) {
            display_counter = 0;
            UpdateDisplay();
            HandleSDLEvents();
        }
        
        // Check for halt
        if (core->pc == 0x00000000 || core->pc == 0xFFFFFFFF) {
            printf("\nCPU halted at PC=0x%08x\n", core->pc);
            break;
        }
    }
    
    printf("\nEmulation ended. Total instructions: %d\n", instct);
    
    // Cleanup
    CleanupSDL();
    free(ram_image);
    free(core);
    
    return 0;
}