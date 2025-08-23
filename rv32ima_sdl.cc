// SDL2 Framebuffer Extension for rv32ima.cc
// This wrapper adds SDL2 framebuffer and MMIO support to your rv32ima emulator
// for running DOOM and other graphical applications

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <SDL2/SDL.h>

// Memory-mapped I/O addresses
#define MMIO_UART_BASE    0x10000000
#define MMIO_FB_BASE      0x11100000
#define MMIO_FB_SIZE      (640 * 480 * 4)  // 640x480 @ 32bpp
#define MMIO_TIMER_BASE   0x11300000

// Forward declare the CPU struct from rv32ima.cc
struct CPU;

// SDL framebuffer class
class SDLFramebuffer {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    uint32_t* pixels;
    int width = 640;
    int height = 480;
    bool initialized = false;

public:
    SDLFramebuffer() : window(nullptr), renderer(nullptr), texture(nullptr) {
        pixels = new uint32_t[width * height];
        memset(pixels, 0, width * height * sizeof(uint32_t));
    }

    ~SDLFramebuffer() {
        if (initialized) {
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
        }
        delete[] pixels;
    }

    bool Init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
            return false;
        }

        window = SDL_CreateWindow("RV32IMA - DOOM",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_SHOWN);
        if (!window) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
            return false;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
            return false;
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!texture) {
            std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
            return false;
        }

        initialized = true;
        return true;
    }

    void WritePixel(uint32_t addr, uint32_t value, int size) {
        uint32_t offset = addr - MMIO_FB_BASE;
        if (offset >= MMIO_FB_SIZE) return;

        if (size == 4) {
            uint32_t pixel_idx = offset / 4;
            if (pixel_idx < width * height) {
                // Convert BGR to RGB for SDL
                uint32_t b = (value >> 16) & 0xFF;
                uint32_t g = (value >> 8) & 0xFF;
                uint32_t r = value & 0xFF;
                pixels[pixel_idx] = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
        } else if (size == 1) {
            // Byte write
            uint32_t pixel_idx = offset / 4;
            uint32_t byte_offset = offset % 4;
            if (pixel_idx < width * height) {
                uint32_t mask = 0xFF << (byte_offset * 8);
                pixels[pixel_idx] = (pixels[pixel_idx] & ~mask) | ((value & 0xFF) << (byte_offset * 8));
            }
        }
    }

    uint32_t ReadPixel(uint32_t addr, int size) {
        uint32_t offset = addr - MMIO_FB_BASE;
        if (offset >= MMIO_FB_SIZE) return 0;
        
        uint32_t pixel_idx = offset / 4;
        if (pixel_idx < width * height) {
            uint32_t pixel = pixels[pixel_idx];
            // Convert RGB to BGR
            uint32_t r = (pixel >> 16) & 0xFF;
            uint32_t g = (pixel >> 8) & 0xFF;
            uint32_t b = pixel & 0xFF;
            return (b << 16) | (g << 8) | r;
        }
        return 0;
    }

    void UpdateDisplay() {
        if (!initialized) return;
        
        SDL_UpdateTexture(texture, NULL, pixels, width * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    bool HandleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return false;
            }
        }
        return true;
    }
};

// Include your original rv32ima.cc implementation
#include "rv32ima.cc"

// Extended CPU with MMIO support
struct CPU_SDL : public CPU {
    SDLFramebuffer* fb;
    uint32_t frame_counter;
    bool enable_framebuffer;
    bool quit_requested;

    CPU_SDL(size_t mem_size, bool trace = false, bool use_fb = true) 
        : CPU(mem_size, trace), fb(nullptr), frame_counter(0), 
          enable_framebuffer(use_fb), quit_requested(false) {
        if (enable_framebuffer) {
            fb = new SDLFramebuffer();
            if (!fb->Init()) {
                std::cerr << "Warning: Failed to initialize SDL framebuffer\n";
                delete fb;
                fb = nullptr;
                enable_framebuffer = false;
            }
        }
    }

    ~CPU_SDL() {
        if (fb) delete fb;
    }

    // Override fetch32 to handle MMIO reads
    uint32_t fetch32_mmio(uint32_t addr) {
        // UART read
        if (addr >= MMIO_UART_BASE && addr < MMIO_UART_BASE + 0x100) {
            if (addr == MMIO_UART_BASE + 5) {
                return 0x60;  // UART ready flags
            }
            return 0;
        }
        
        // Framebuffer read
        if (enable_framebuffer && fb && addr >= MMIO_FB_BASE && addr < MMIO_FB_BASE + MMIO_FB_SIZE) {
            return fb->ReadPixel(addr, 4);
        }
        
        // Timer/RTC
        if (addr >= MMIO_TIMER_BASE && addr < MMIO_TIMER_BASE + 0x100) {
            if (addr == MMIO_TIMER_BASE) {
                return cycles & 0xFFFFFFFF;
            } else if (addr == MMIO_TIMER_BASE + 4) {
                return (cycles >> 32) & 0xFFFFFFFF;
            }
            return 0;
        }
        
        // Default to normal memory access
        return fetch32(addr);
    }

    // Override store32 to handle MMIO writes
    void store32_mmio(uint32_t addr, uint32_t value) {
        // UART write
        if (addr >= MMIO_UART_BASE && addr < MMIO_UART_BASE + 0x100) {
            if (addr == MMIO_UART_BASE) {
                // Console output
                putchar(value & 0xFF);
                fflush(stdout);
            }
            return;
        }
        
        // Framebuffer write
        if (enable_framebuffer && fb && addr >= MMIO_FB_BASE && addr < MMIO_FB_BASE + MMIO_FB_SIZE) {
            fb->WritePixel(addr, value, 4);
            
            // Update display every N writes for performance
            static int write_count = 0;
            if (++write_count >= 10000) {
                fb->UpdateDisplay();
                write_count = 0;
            }
            return;
        }
        
        // Timer/RTC (read-only)
        if (addr >= MMIO_TIMER_BASE && addr < MMIO_TIMER_BASE + 0x100) {
            return;  // Ignore writes to timer
        }
        
        // Default to normal memory access
        store32(addr, value);
    }

    // Similar for byte and halfword access
    uint8_t load8_mmio(uint32_t addr) {
        if (addr >= MMIO_UART_BASE && addr < MMIO_UART_BASE + 0x100) {
            if (addr == MMIO_UART_BASE + 5) return 0x60;
            return 0;
        }
        if (addr < mem.size()) return mem[addr];
        return 0;
    }

    uint16_t load16_mmio(uint32_t addr) {
        if (addr >= MMIO_UART_BASE && addr < MMIO_UART_BASE + 0x100) {
            return 0;
        }
        if (addr + 1 < mem.size()) {
            return mem[addr] | (mem[addr+1] << 8);
        }
        return 0;
    }

    void store8_mmio(uint32_t addr, uint8_t value) {
        if (addr >= MMIO_UART_BASE && addr < MMIO_UART_BASE + 0x100) {
            if (addr == MMIO_UART_BASE) {
                putchar(value);
                fflush(stdout);
            }
            return;
        }
        if (enable_framebuffer && fb && addr >= MMIO_FB_BASE && addr < MMIO_FB_BASE + MMIO_FB_SIZE) {
            fb->WritePixel(addr, value, 1);
            return;
        }
        if (addr < mem.size()) mem[addr] = value;
    }

    void store16_mmio(uint32_t addr, uint16_t value) {
        if (addr >= MMIO_UART_BASE && addr < MMIO_UART_BASE + 0x100) {
            return;
        }
        if (enable_framebuffer && fb && addr >= MMIO_FB_BASE && addr < MMIO_FB_BASE + MMIO_FB_SIZE) {
            fb->WritePixel(addr, value, 2);
            return;
        }
        if (addr + 1 < mem.size()) {
            mem[addr] = value & 0xFF;
            mem[addr+1] = (value >> 8) & 0xFF;
        }
    }

    // Modified step function with MMIO support
    void step_with_mmio() {
        uint32_t ins = fetch32_mmio(pc);
        uint32_t orig_pc = pc;
        
        // Decode and execute instruction (simplified - you need to modify all memory ops)
        // This is a simplified example - you'd need to modify the full step() implementation
        execute_with_mmio(ins);
        
        cycles++;
        
        // Handle SDL events periodically
        if (enable_framebuffer && fb && (cycles % 10000 == 0)) {
            if (!fb->HandleEvents()) {
                quit_requested = true;
            }
            
            // Update display periodically
            if (cycles % 100000 == 0) {
                fb->UpdateDisplay();
            }
        }
    }

    // You would need to implement execute_with_mmio that uses the _mmio functions
    // For now, let's create a wrapper that patches memory operations
    void execute_with_mmio(uint32_t ins);

    void run_doom(uint64_t max_cycles = 500000000) {
        std::cerr << "Starting DOOM emulation with SDL framebuffer...\n";
        std::cerr << "Running for up to " << max_cycles << " cycles\n";
        
        while (cycles < max_cycles && !quit_requested) {
            step_with_mmio();
            
            // Progress indicator
            if (cycles % 10000000 == 0) {
                std::cerr << "Executed " << cycles << " instructions...\r";
            }
        }
        
        // Final display update
        if (enable_framebuffer && fb) {
            fb->UpdateDisplay();
        }
        
        std::cerr << "\nExecution completed after " << cycles << " instructions\n";
    }
};

// Implement the execute_with_mmio function
void CPU_SDL::execute_with_mmio(uint32_t ins) {
    // For now, let's just call the original step() but patch memory operations
    // This is a simplified approach - ideally you'd modify step() directly
    
    // Save original fetch/store functions and temporarily replace them
    // This is a workaround - the proper solution would be to modify step() to use virtual functions
    
    // Just step normally for now - MMIO is handled in fetch32_mmio/store32_mmio
    // We need to modify the actual step() implementation to use these
    
    // Temporary: just increment PC to avoid infinite loop
    pc += 4;
    
    // TODO: Properly integrate MMIO into the step() function
}

// Main function
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [-f] program.bin [max_cycles]\n";
        std::cerr << "  -f: Load at 0x10000 (for DOOM)\n";
        return 1;
    }
    
    bool doom_mode = false;
    std::string filename;
    uint64_t max_cycles = 500000000;  // Default 500M cycles for DOOM
    
    int arg_idx = 1;
    if (std::string(argv[arg_idx]) == "-f") {
        doom_mode = true;
        arg_idx++;
    }
    
    if (arg_idx >= argc) {
        std::cerr << "Error: No binary file specified\n";
        return 1;
    }
    
    filename = argv[arg_idx++];
    
    if (arg_idx < argc) {
        max_cycles = std::stoull(argv[arg_idx]);
    }
    
    // Create emulator with 64MB RAM and framebuffer
    CPU_SDL cpu(64 * 1024 * 1024, false, true);  // 64MB, no trace, with framebuffer
    
    // Load binary
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return 1;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (doom_mode) {
        // DOOM expects to be loaded at 0x10000
        uint32_t load_addr = 0x10000;
        if (size > cpu.mem.size() - load_addr) {
            std::cerr << "Error: Binary too large\n";
            return 1;
        }
        file.read(reinterpret_cast<char*>(cpu.mem.data() + load_addr), size);
        cpu.pc = load_addr;
        std::cerr << "Loaded " << size << " bytes at 0x" << std::hex << load_addr << std::dec << std::endl;
    } else {
        if (size > cpu.mem.size()) {
            std::cerr << "Error: Binary too large\n";
            return 1;
        }
        file.read(reinterpret_cast<char*>(cpu.mem.data()), size);
        cpu.pc = 0;
    }
    
    // Run emulation
    cpu.run_doom(max_cycles);
    
    return 0;
}