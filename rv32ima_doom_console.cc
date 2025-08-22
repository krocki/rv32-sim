// RISC-V emulator for DOOM - Console version
// This version outputs DOOM to the terminal using ANSI escape sequences
// Based on cnlohr's mini-rv32ima

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <termios.h>
#include <fcntl.h>

// Configuration
#define MINIRV32WARN( x... ) fprintf( stderr, x );
#define MINIRV32_RAM_IMAGE_OFFSET  0x80000000
#define MINIRV32_RAM_DEFAULT_SIZE   (64*1024*1024)
#define MINI_RV32_RAM_SIZE          MINIRV32_RAM_DEFAULT_SIZE
#define MINIRV32_MMIO_RANGE_LOW     0x10000000
#define MINIRV32_MMIO_RANGE_HIGH    0x12000000
#define MINIRV32_OTHERCSR_BASE      0x5000000
#define MINIRV32_DECORATE           static

// Forward declarations
static uint32_t HandleControlStore(uint32_t addy, uint32_t val);
static uint32_t HandleControlLoad(uint32_t addy);
static void HandleOtherCSRWrite(struct MiniRV32IMAState *state, uint32_t csrno, uint32_t value);
static uint32_t HandleOtherCSRRead(struct MiniRV32IMAState *state, uint32_t csrno);
static int IsKBHit();
static int ReadKBByte();

// Default DTB for 64MB RAM
static uint32_t default_dtb[] = {
    0xedfe0dd0, 0x38030000, 0x38020000, 0x38000000,
    0x28000000, 0x11000000, 0x10000000, 0x00000000,
    0x4d010000, 0x00000000, 0x00010000, 0x00000000,
    0x03000000, 0x04000000, 0x00000000, 0x02000000,
    0x03000000, 0x04000000, 0x0f000000, 0x02000000,
    0x01000000, 0x6d656d00, 0x0079726f, 0x00000000,
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
    0x04000000, 0x36000000, 0x63736972, 0x00000076
};

// Global state (needed by macros)
static uint8_t *ram_image = 0;
static uint32_t ram_amt = MINIRV32_RAM_DEFAULT_SIZE;
static uint64_t lastTime = 0;
static int time_divisor = 1;
static int should_quit = 0;
static struct termios old_tio;

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
#define MINIRV32_POSTEXEC( pc, ir, retval ) { if( retval > 0 ) { if( retval > 1 ) { printf( "FAULT %d @ %08x\n", retval, pc ); } return retval; } }
#define MINIRV32_HANDLE_MEM_STORE_CONTROL( addy, val ) if( HandleControlStore( addy, val ) ) return val;
#define MINIRV32_HANDLE_MEM_LOAD_CONTROL( addy, rval ) rval = HandleControlLoad( addy );
#define MINIRV32_OTHERCSR_WRITE( csrno, value ) HandleOtherCSRWrite( state, csrno, value );
#define MINIRV32_OTHERCSR_READ( csrno, value ) value = HandleOtherCSRRead( state, csrno );

#include "mini-rv32ima.h"

// Global state
static struct MiniRV32IMAState *core;

static uint64_t GetTimeMicroseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_usec + ((uint64_t)(tv.tv_sec)) * 1000000LL;
}

static uint32_t HandleControlStore(uint32_t addy, uint32_t val) {
    // UART at 0x10000000 (different from our SDL version)
    if (addy == 0x10000000) {
        // Output character - this is where DOOM draws!
        putchar(val & 0xFF);
        fflush(stdout);
        return 0;
    }
    
    // System control
    if (addy == 0x11100000) {
        if (val == 0x5555) {
            should_quit = 1;
        }
        return 0;
    }
    
    return 0;
}

static uint32_t HandleControlLoad(uint32_t addy) {
    // UART status at 0x10000000
    if (addy == 0x10000000) {
        // Check if keyboard has data
        if (IsKBHit()) {
            return 0x100 | ReadKBByte();
        }
        return 0;
    }
    
    // Timer
    if (addy >= 0x10000000 && addy < 0x10000100) {
        // Simple timer handling
        uint64_t now = GetTimeMicroseconds();
        if (addy == 0x1000BFF8) {
            return (now / time_divisor) & 0xFFFFFFFF;
        }
        if (addy == 0x1000BFFC) {
            return ((now / time_divisor) >> 32) & 0xFFFFFFFF;
        }
    }
    
    return 0;
}

static void HandleOtherCSRWrite(struct MiniRV32IMAState *state, uint32_t csrno, uint32_t value) {
    if (csrno == 0x136) {
        // Custom CSR for debugging
        printf("CSR 0x136: %d\n", value);
    }
}

static uint32_t HandleOtherCSRRead(struct MiniRV32IMAState *state, uint32_t csrno) {
    if (csrno == 0x136) {
        return 0;
    }
    return 0;
}

static int IsKBHit() {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

static int ReadKBByte() {
    if (IsKBHit()) {
        char c;
        if (read(0, &c, 1) == 1) {
            return c;
        }
    }
    return 0;
}

static void SetupTerminal() {
    struct termios new_tio;
    
    // Get current terminal settings
    tcgetattr(STDIN_FILENO, &old_tio);
    
    // Copy and modify for raw mode
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    new_tio.c_cc[VMIN] = 0;
    new_tio.c_cc[VTIME] = 0;
    
    // Apply new settings
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    
    // Make stdin non-blocking
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

static void RestoreTerminal() {
    // Restore original terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}

static int HandleException(uint32_t ir, uint32_t code) {
    // Handle exceptions
    return code;
}

int main(int argc, char **argv) {
    int do_sleep = 1;
    int fixed_update = 0;
    int dtb_ptr = 0;
    const char *image_file = NULL;
    int fail_on_all_faults = 0;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) {
            fixed_update = 1;
        } else if (strcmp(argv[i], "-n") == 0) {
            do_sleep = 0;
        } else if (strcmp(argv[i], "-s") == 0) {
            time_divisor = 2;
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
    
    // Copy DTB to end of RAM
    dtb_ptr = ram_amt - sizeof(default_dtb) - 1024;
    memcpy(ram_image + dtb_ptr, default_dtb, sizeof(default_dtb));
    
    // Setup terminal for raw input
    SetupTerminal();
    
    // Clear screen
    printf("\033[2J\033[H");
    printf("RISC-V Console Emulator for DOOM\n");
    printf("RAM: %d MB\n", ram_amt / (1024*1024));
    printf("Image: %s (%ld bytes)\n", image_file, flen);
    printf("DTB at: 0x%08x\n", MINIRV32_RAM_IMAGE_OFFSET + dtb_ptr);
    printf("Press Ctrl+C to quit\n");
    printf("=====================================\n");
    
    // Initialize CPU
    core = (struct MiniRV32IMAState*)calloc(sizeof(struct MiniRV32IMAState), 1);
    core->pc = MINIRV32_RAM_IMAGE_OFFSET;
    core->regs[10] = 0x00; // hart ID
    core->regs[11] = MINIRV32_RAM_IMAGE_OFFSET + dtb_ptr; // DTB pointer
    
    lastTime = GetTimeMicroseconds();
    int instct = 0;
    
    // Main emulation loop
    while (!should_quit) {
        uint64_t now = GetTimeMicroseconds();
        uint64_t elapsed = now - lastTime;
        
        if (elapsed > 1000) { // Run in ~1ms chunks
            int instructions = fixed_update ? 1024 : (elapsed * 3); // ~3 MIPS
            
            int ret = MiniRV32IMAStep(core, ram_image, 0, elapsed, instructions);
            if (ret) {
                break;
            }
            
            instct += instructions;
            lastTime = now;
            
            if (do_sleep) {
                usleep(1000);
            }
        }
        
        // Check for Ctrl+C
        if (IsKBHit()) {
            int c = ReadKBByte();
            if (c == 3) { // Ctrl+C
                should_quit = 1;
            }
        }
        
        // Check for halt
        if (core->pc == 0x00000000 || core->pc == 0xFFFFFFFF) {
            printf("\nCPU halted at PC=0x%08x\n", core->pc);
            break;
        }
    }
    
    printf("\nEmulation ended. Total instructions: %d\n", instct);
    
    // Cleanup
    RestoreTerminal();
    free(ram_image);
    free(core);
    
    return 0;
}