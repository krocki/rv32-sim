// SDL2 wrapper for rv32ima.cc emulator to run DOOM
// This creates a modified version of your CPU struct with MMIO support

#include <iostream>
#include <fstream>
#include <cstring>
#include <SDL2/SDL.h>

// Memory-mapped I/O addresses for DOOM
#define MMIO_UART_BASE    0x10000000
#define MMIO_FB_BASE      0x11100000  
#define MMIO_FB_SIZE      (640 * 480 * 4)
#define MMIO_TIMER_BASE   0x11300000

// Modified version of rv32ima.cc with MMIO support
#include <cstdint>
#include <vector>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <climits>

// SDL Framebuffer handler
class SDLFramebuffer {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    uint32_t* pixels;
    int width = 640;
    int height = 480;

public:
    SDLFramebuffer() {
        pixels = new uint32_t[width * height];
        memset(pixels, 0, width * height * sizeof(uint32_t));
        
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow("RV32IMA - DOOM", 
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, width, height);
    }
    
    ~SDLFramebuffer() {
        delete[] pixels;
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    
    void write(uint32_t offset, uint32_t value) {
        uint32_t idx = offset / 4;
        if (idx < width * height) {
            // Convert BGR to RGB
            uint32_t b = (value >> 16) & 0xFF;
            uint32_t g = (value >> 8) & 0xFF;
            uint32_t r = value & 0xFF;
            pixels[idx] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }
    
    void update() {
        SDL_UpdateTexture(texture, NULL, pixels, width * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
    
    bool handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) return false;
        }
        return true;
    }
};

// Include the CPU struct from rv32ima.cc and modify it
struct CPU_DOOM {
    uint32_t pc = 0;
    uint32_t x[32]{};
    uint64_t cycles = 0;
    std::vector<uint8_t> mem;
    bool has_reservation = false;
    uint32_t reservation_addr = 0;
    uint32_t csr[4096]{};
    bool trace_enabled = false;
    
    // SDL framebuffer
    SDLFramebuffer* fb;
    bool quit = false;
    
    explicit CPU_DOOM(size_t mem_size) : mem(mem_size) {
        fb = new SDLFramebuffer();
    }
    
    ~CPU_DOOM() {
        delete fb;
    }
    
    // Modified fetch32 with MMIO support
    uint32_t fetch32(uint32_t addr) {
        // UART status
        if (addr == MMIO_UART_BASE + 5) return 0x60;
        
        // Timer
        if (addr == MMIO_TIMER_BASE) return cycles & 0xFFFFFFFF;
        if (addr == MMIO_TIMER_BASE + 4) return (cycles >> 32) & 0xFFFFFFFF;
        
        // Normal memory
        if (addr + 3 < mem.size()) {
            return mem[addr] | mem[addr+1]<<8 | mem[addr+2]<<16 | mem[addr+3]<<24;
        }
        return 0;
    }
    
    // Modified store32 with MMIO support  
    void store32(uint32_t addr, uint32_t v) {
        // UART output
        if (addr == MMIO_UART_BASE) {
            putchar(v & 0xFF);
            fflush(stdout);
            return;
        }
        
        // Framebuffer
        if (addr >= MMIO_FB_BASE && addr < MMIO_FB_BASE + MMIO_FB_SIZE) {
            fb->write(addr - MMIO_FB_BASE, v);
            return;
        }
        
        // Normal memory
        if (addr + 3 < mem.size()) {
            mem[addr] = v;
            mem[addr+1] = v >> 8;
            mem[addr+2] = v >> 16;
            mem[addr+3] = v >> 24;
        }
    }
    
    // Similar modifications for byte/halfword access
    uint8_t load8(uint32_t addr) {
        if (addr == MMIO_UART_BASE + 5) return 0x60;
        if (addr < mem.size()) return mem[addr];
        return 0;
    }
    
    uint16_t load16(uint32_t addr) {
        if (addr + 1 < mem.size()) {
            return mem[addr] | (mem[addr+1] << 8);
        }
        return 0;
    }
    
    void store8(uint32_t addr, uint8_t v) {
        if (addr == MMIO_UART_BASE) {
            putchar(v);
            fflush(stdout);
            return;
        }
        if (addr < mem.size()) mem[addr] = v;
    }
    
    void store16(uint32_t addr, uint16_t v) {
        if (addr + 1 < mem.size()) {
            mem[addr] = v & 0xFF;
            mem[addr+1] = (v >> 8) & 0xFF;
        }
    }

    // Include the step() function from rv32ima.cc
    // This would be a copy of your step() implementation
    // but using the modified memory access functions above
    void step();
    
    // Run with SDL event handling
    void run(uint64_t max_cycles) {
        while (cycles < max_cycles && !quit) {
            step();
            cycles++;
            
            // Handle SDL events and update display periodically
            if (cycles % 10000 == 0) {
                if (!fb->handle_events()) {
                    quit = true;
                }
            }
            if (cycles % 100000 == 0) {
                fb->update();
            }
            if (cycles % 10000000 == 0) {
                std::cerr << "Cycles: " << cycles << "\r";
            }
        }
        fb->update();
        std::cerr << "\nCompleted after " << cycles << " cycles\n";
    }
};

// Copy the step() implementation from rv32ima.cc here
// This is needed because we need to use the modified memory functions
void CPU_DOOM::step() {
    // This would be your complete step() implementation
    // but using the MMIO-aware fetch32/store32/load8/store8 etc
    
    // For now, just a placeholder that shows the structure:
    uint32_t ins = fetch32(pc);
    
    // Your full instruction decode and execute logic here...
    // This is where you'd copy your step() implementation
    
    pc += 4;  // Simplified - your actual implementation handles branches etc
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " doom.bin [cycles]\n";
        return 1;
    }
    
    uint64_t max_cycles = 500000000;
    if (argc > 2) max_cycles = std::stoull(argv[2]);
    
    // Create CPU with 64MB RAM
    CPU_DOOM cpu(64 * 1024 * 1024);
    
    // Load DOOM binary at 0x10000
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open " << argv[1] << "\n";
        return 1;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0);
    
    uint32_t load_addr = 0x10000;
    file.read(reinterpret_cast<char*>(cpu.mem.data() + load_addr), size);
    cpu.pc = load_addr;
    
    std::cerr << "Loaded " << size << " bytes at 0x" << std::hex << load_addr << std::dec << "\n";
    std::cerr << "Starting emulation...\n";
    
    cpu.run(max_cycles);
    
    return 0;
}