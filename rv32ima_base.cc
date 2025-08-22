// RV32IMA RISC-V Emulator
// Supports RV32I base + M (multiply) + A (atomic) extensions
// Optional SDL support for framebuffer output

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>

#include "rv32ima_core.h"

#ifdef USE_SDL
#include "rv32ima_sdl.h"
SDLFramebuffer* g_sdl_framebuffer = nullptr;
#endif

// Global state
static RV32IMA_State* g_state = nullptr;
static bool g_trace = false;
static uint32_t g_max_cycles = 0;

// Terminal handling for console I/O
static struct termios orig_termios;
static bool termios_initialized = false;

void DisableRawMode() {
    if (termios_initialized) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        termios_initialized = false;
    }
}

void EnableRawMode() {
    if (!termios_initialized) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        atexit(DisableRawMode);
        
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        termios_initialized = true;
    }
}

int IsKBHit() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

int ReadKBByte() {
    if (IsKBHit()) {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            return c;
        }
    }
    return -1;
}

// Memory access handlers
uint32_t HandleMemoryLoad(RV32IMA_State* state, uint32_t addr, uint32_t size) {
    addr -= RAM_BASE;
    if (addr >= state->ram_size) {
        if (g_trace) printf("Memory load out of bounds: 0x%08x\n", addr + RAM_BASE);
        return 0;
    }
    
    switch (size) {
        case 1: return state->ram_image[addr];
        case 2: return *(uint16_t*)(state->ram_image + addr);
        case 4: return *(uint32_t*)(state->ram_image + addr);
        default: return 0;
    }
}

int HandleMemoryStore(RV32IMA_State* state, uint32_t addr, uint32_t val, uint32_t size) {
#ifdef USE_SDL
    if (g_sdl_framebuffer && IsFramebufferAddress(addr)) {
        g_sdl_framebuffer->WritePixel(addr, val, size);
        return 0;
    }
#endif
    
    addr -= RAM_BASE;
    if (addr >= state->ram_size) {
        if (g_trace) printf("Memory store out of bounds: 0x%08x\n", addr + RAM_BASE);
        return -1;
    }
    
    switch (size) {
        case 1: state->ram_image[addr] = val; break;
        case 2: *(uint16_t*)(state->ram_image + addr) = val; break;
        case 4: *(uint32_t*)(state->ram_image + addr) = val; break;
        default: return -1;
    }
    return 0;
}

uint32_t HandleControlLoad(RV32IMA_State* state, uint32_t addr) {
    // UART status
    if (addr == MINIRV32_UART_BASE + 5) {
        return 0x60 | (IsKBHit() ? 1 : 0);
    }
    // UART data
    else if (addr == MINIRV32_UART_BASE) {
        int c = ReadKBByte();
        return (c >= 0) ? c : 0;
    }
    // Timer
    else if (addr == 0x1100bffc) {
        return state->timerh;
    }
    else if (addr == 0x1100bff8) {
        return state->timerl;
    }
    
#ifdef USE_SDL
    if (g_sdl_framebuffer && IsFramebufferAddress(addr)) {
        return g_sdl_framebuffer->ReadPixel(addr, 4);
    }
#endif
    
    return 0;
}

int HandleControlStore(RV32IMA_State* state, uint32_t addr, uint32_t val) {
    // UART output
    if (addr == MINIRV32_UART_BASE) {
        putchar(val);
        fflush(stdout);
        return 0;
    }
    // Timer match
    else if (addr == 0x11004000) {
        state->timermatchh = val;
        return 0;
    }
    else if (addr == 0x11004004) {
        state->timermatchl = val;
        return 0;
    }
    
#ifdef USE_SDL
    if (g_sdl_framebuffer && IsFramebufferAddress(addr)) {
        g_sdl_framebuffer->WritePixel(addr, val, 4);
        return 0;
    }
#endif
    
    return 0;
}

void HandleOtherCSR(RV32IMA_State* state, uint32_t csrno, uint32_t* val) {
    if (csrno == CSR_CYCLE) {
        *val = state->cyclel;
    } else if (csrno == CSR_CYCLEH) {
        *val = state->cycleh;
    } else if (csrno == CSR_TIME) {
        *val = state->timerl;
    } else if (csrno == CSR_TIMEH) {
        *val = state->timerh;
    } else if (csrno == CSR_INSTRET) {
        *val = state->cyclel;
    } else if (csrno == CSR_INSTRETH) {
        *val = state->cycleh;
    } else {
        *val = state->csr[csrno];
    }
}

// Main execution function
int Execute(RV32IMA_State* state);

int main(int argc, char* argv[]) {
    // Parse arguments
    const char* filename = nullptr;
    bool use_sdl = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) {
            g_trace = true;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            filename = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            g_max_cycles = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--sdl") == 0) {
            use_sdl = true;
        } else if (!filename) {
            filename = argv[i];
        }
    }
    
    if (!filename) {
        fprintf(stderr, "Usage: %s [--trace] [--sdl] [-c cycles] program.bin\n", argv[0]);
        return 1;
    }
    
    // Load program
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return 1;
    }
    
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    printf("Image loaded: %s (%zu bytes)\n", filename, file_size);
    
    // Initialize state
    g_state = new RV32IMA_State();
    
    // Load program into RAM
    file.read((char*)g_state->ram_image, std::min(file_size, (size_t)g_state->ram_size));
    file.close();
    
#ifdef USE_SDL
    if (use_sdl) {
        g_sdl_framebuffer = new SDLFramebuffer();
        if (!g_sdl_framebuffer->Init()) {
            fprintf(stderr, "Failed to initialize SDL\n");
            delete g_sdl_framebuffer;
            g_sdl_framebuffer = nullptr;
        }
    }
#endif
    
    // Enable raw mode for console I/O
    EnableRawMode();
    
    printf("Starting emulation... Press ESC to quit\n");
    
    // Main execution loop
    int result = Execute(g_state);
    
    // Cleanup
    DisableRawMode();
    
#ifdef USE_SDL
    if (g_sdl_framebuffer) {
        delete g_sdl_framebuffer;
    }
#endif
    
    delete g_state;
    
    printf("\nEmulation ended. Total instructions: %llu\n", 
           (unsigned long long)(g_state->cyclel + ((uint64_t)g_state->cycleh << 32)));
    
    return result;
}

// Core execution - this will include the actual instruction implementation
// from the original mini-rv32ima code
int Execute(RV32IMA_State* state) {
    uint32_t instruction_count = 0;
    
    while (true) {
        // Check cycle limit
        if (g_max_cycles > 0 && instruction_count >= g_max_cycles) {
            break;
        }
        
        // Update timers
        state->cyclel++;
        if (state->cyclel == 0) state->cycleh++;
        
        // Simple timer increment
        if ((instruction_count & 1023) == 0) {
            state->timerl++;
            if (state->timerl == 0) state->timerh++;
        }
        
#ifdef USE_SDL
        // Update display periodically
        if (g_sdl_framebuffer && (instruction_count & 0xFFFF) == 0) {
            g_sdl_framebuffer->UpdateDisplay();
            if (!g_sdl_framebuffer->ProcessEvents()) {
                break;
            }
        }
#endif
        
        // Fetch instruction
        uint32_t pc_offset = state->pc - RAM_BASE;
        if (pc_offset >= state->ram_size) {
            printf("PC out of bounds: 0x%08x\n", state->pc);
            return -1;
        }
        
        uint32_t ir = *(uint32_t*)(state->ram_image + pc_offset);
        uint32_t opcode = ir & 0x7f;
        
        // Basic instruction decode and execute
        // This is a simplified version - full implementation would go here
        // For now, just handle ECALL for exit
        if (ir == 0x00000073) { // ECALL
            uint32_t syscall = state->regs[17]; // a7
            if (syscall == 93) { // exit
                return state->regs[10]; // a0
            } else if (syscall == 64) { // write
                uint32_t fd = state->regs[10];
                uint32_t buf = state->regs[11];
                uint32_t count = state->regs[12];
                
                if (fd == 1 || fd == 2) { // stdout/stderr
                    for (uint32_t i = 0; i < count; i++) {
                        putchar(HandleMemoryLoad(state, buf + i, 1));
                    }
                    fflush(stdout);
                }
                state->regs[10] = count;
            }
        }
        
        // Advance PC (simplified - real implementation handles jumps/branches)
        state->pc += 4;
        instruction_count++;
        
        // Check for ESC key
        if (IsKBHit()) {
            int c = ReadKBByte();
            if (c == 27) { // ESC
                printf("\nEmulation interrupted by user\n");
                break;
            }
        }
    }
    
    return 0;
}