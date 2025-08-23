// SDL/MMIO Memory Subsystem for DOOM
// Implements framebuffer, UART, and timer MMIO regions

#ifndef MEMORY_SUBSYSTEM_SDL_H
#define MEMORY_SUBSYSTEM_SDL_H

#include "memory_subsystem.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <cstring>
#include <vector>

// Memory-mapped I/O addresses
#define MMIO_UART_BASE    0x10000000
#define MMIO_UART_SIZE    0x100
#define MMIO_FB_BASE      0x11100000
#define MMIO_FB_SIZE      (640 * 480 * 4)  // 640x480 @ 32bpp
#define MMIO_KBD_BASE     0x11200000  // Keyboard input
#define MMIO_KBD_SIZE     0x100
#define MMIO_TIMER_BASE   0x11300000
#define MMIO_TIMER_SIZE   0x100

class SDLMemory : public MemorySubsystem {
private:
    std::vector<uint8_t> mem;
    
    // SDL components
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    uint32_t* framebuffer;
    int fb_width = 640;
    int fb_height = 480;
    bool sdl_initialized;
    bool quit_requested;
    
    // Timing
    uint64_t cycle_counter;
    uint32_t update_counter;
    
    // Keyboard input queue
    std::vector<uint8_t> kbd_queue;
    size_t kbd_read_pos;
    
    // Map SDL keys to DOOM keys
    uint8_t sdl_to_doom_key(SDL_Keycode key) {
        switch(key) {
            case SDLK_LEFT:   return 0xAC;  // KEY_LEFTARROW
            case SDLK_RIGHT:  return 0xAE;  // KEY_RIGHTARROW
            case SDLK_UP:     return 0xAD;  // KEY_UPARROW
            case SDLK_DOWN:   return 0xAF;  // KEY_DOWNARROW
            case SDLK_LCTRL:
            case SDLK_RCTRL:  return 0x1D;  // KEY_RCTRL (fire)
            case SDLK_SPACE:  return ' ';   // Use/open doors
            case SDLK_LSHIFT:
            case SDLK_RSHIFT: return 0x10;  // KEY_RSHIFT (run)
            case SDLK_LALT:
            case SDLK_RALT:   return 0x38;  // KEY_RALT (strafe)
            case SDLK_ESCAPE: return 27;    // Menu
            case SDLK_RETURN: return 13;    // Enter
            case SDLK_TAB:    return 9;     // Map
            case SDLK_F1:     return 0x3B;  // Help
            case SDLK_y:      return 'y';   // Yes in menus
            case SDLK_n:      return 'n';   // No in menus
            default:
                // For regular ASCII keys
                if (key >= 32 && key <= 126) return key;
                return 0;
        }
    }
    
public:
    explicit SDLMemory(size_t mem_size) 
        : mem(mem_size, 0), window(nullptr), renderer(nullptr),
          texture(nullptr), sdl_initialized(false), quit_requested(false),
          cycle_counter(0), update_counter(0), kbd_read_pos(0) {
        
        framebuffer = new uint32_t[fb_width * fb_height];
        memset(framebuffer, 0, fb_width * fb_height * sizeof(uint32_t));
        
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) == 0) {
            window = SDL_CreateWindow("RV32IMA - DOOM",
                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                fb_width, fb_height, SDL_WINDOW_SHOWN);
            
            if (window) {
                renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
                if (renderer) {
                    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING, fb_width, fb_height);
                    if (texture) {
                        sdl_initialized = true;
                        std::cerr << "SDL initialized successfully\n";
                    }
                }
            }
        }
        
        if (!sdl_initialized) {
            std::cerr << "Warning: SDL initialization failed, running without display\n";
        }
    }
    
    ~SDLMemory() {
        delete[] framebuffer;
        if (sdl_initialized) {
            if (texture) SDL_DestroyTexture(texture);
            if (renderer) SDL_DestroyRenderer(renderer);
            if (window) SDL_DestroyWindow(window);
            SDL_Quit();
        }
    }
    
    uint32_t fetch32(uint32_t addr) override {
        // UART status register
        if (addr == MMIO_UART_BASE + 5) {
            return 0x60;  // TX ready, RX ready
        }
        
        // Keyboard input
        if (addr == MMIO_KBD_BASE) {
            // Status register: bit 0 = data available
            return kbd_read_pos < kbd_queue.size() ? 1 : 0;
        }
        if (addr == MMIO_KBD_BASE + 4) {
            // Data register: read next key event
            if (kbd_read_pos < kbd_queue.size()) {
                return kbd_queue[kbd_read_pos++];
            }
            return 0;
        }
        
        // Timer/cycle counter
        if (addr == MMIO_TIMER_BASE) {
            return cycle_counter & 0xFFFFFFFF;
        }
        if (addr == MMIO_TIMER_BASE + 4) {
            return (cycle_counter >> 32) & 0xFFFFFFFF;
        }
        
        // Framebuffer read (usually not used by DOOM)
        if (addr >= MMIO_FB_BASE && addr < MMIO_FB_BASE + MMIO_FB_SIZE) {
            uint32_t offset = (addr - MMIO_FB_BASE) / 4;
            if (offset < fb_width * fb_height) {
                uint32_t pixel = framebuffer[offset];
                // Convert ARGB to RGB (DOOM format)
                uint32_t r = (pixel >> 16) & 0xFF;
                uint32_t g = (pixel >> 8) & 0xFF;
                uint32_t b = pixel & 0xFF;
                return (r << 16) | (g << 8) | b;
            }
            return 0;
        }
        
        // Regular memory - map high addresses down to fit in our memory
        uint32_t mapped_addr = addr & 0x3FFFFFF;  // Map to 64MB range
        if (mapped_addr + 3 >= mem.size()) return 0;
        return mem[mapped_addr] | (mem[mapped_addr+1]<<8) | (mem[mapped_addr+2]<<16) | (mem[mapped_addr+3]<<24);
    }
    
    void store32(uint32_t addr, uint32_t v) override {
        // UART output
        if (addr == MMIO_UART_BASE) {
            putchar(v & 0xFF);
            fflush(stdout);
            return;
        }
        
        // Framebuffer write
        if (addr >= MMIO_FB_BASE && addr < MMIO_FB_BASE + MMIO_FB_SIZE) {
            uint32_t offset = (addr - MMIO_FB_BASE) / 4;
            if (offset < fb_width * fb_height) {
                // Convert RGB (DOOM format) to ARGB for SDL
                uint32_t r = (v >> 16) & 0xFF;
                uint32_t g = (v >> 8) & 0xFF;
                uint32_t b = v & 0xFF;
                framebuffer[offset] = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
            return;
        }
        
        // Keyboard control (write to clear queue)
        if (addr == MMIO_KBD_BASE + 8) {
            kbd_queue.clear();
            kbd_read_pos = 0;
            return;
        }
        
        // Timer (read-only, ignore writes)
        if (addr >= MMIO_TIMER_BASE && addr < MMIO_TIMER_BASE + MMIO_TIMER_SIZE) {
            return;
        }
        
        // Regular memory - map high addresses down to fit in our memory
        uint32_t mapped_addr = addr & 0x3FFFFFF;  // Map to 64MB range
        if (mapped_addr + 3 >= mem.size()) return;
        mem[mapped_addr]   = v;
        mem[mapped_addr+1] = v >> 8;
        mem[mapped_addr+2] = v >> 16;
        mem[mapped_addr+3] = v >> 24;
    }
    
    uint16_t fetch16(uint32_t addr) override {
        // MMIO regions typically don't support 16-bit access
        if (addr >= MMIO_UART_BASE && addr < MMIO_UART_BASE + MMIO_UART_SIZE) return 0;
        if (addr >= MMIO_TIMER_BASE && addr < MMIO_TIMER_BASE + MMIO_TIMER_SIZE) return 0;
        
        // Regular memory - map high addresses down to fit in our memory
        uint32_t mapped_addr = addr & 0x3FFFFFF;  // Map to 64MB range
        if (mapped_addr + 1 >= mem.size()) return 0;
        return mem[mapped_addr] | (mem[mapped_addr+1]<<8);
    }
    
    void store16(uint32_t addr, uint16_t v) override {
        // MMIO regions typically don't support 16-bit access
        if (addr >= MMIO_UART_BASE && addr < MMIO_UART_BASE + MMIO_UART_SIZE) return;
        if (addr >= MMIO_FB_BASE && addr < MMIO_FB_BASE + MMIO_FB_SIZE) return;
        if (addr >= MMIO_TIMER_BASE && addr < MMIO_TIMER_BASE + MMIO_TIMER_SIZE) return;
        
        // Regular memory - map high addresses down to fit in our memory
        uint32_t mapped_addr = addr & 0x3FFFFFF;  // Map to 64MB range
        if (mapped_addr + 1 >= mem.size()) return;
        mem[mapped_addr]   = v;
        mem[mapped_addr+1] = v >> 8;
    }
    
    uint8_t fetch8(uint32_t addr) override {
        // UART status
        if (addr == MMIO_UART_BASE + 5) return 0x60;
        
        // Regular memory - map high addresses down to fit in our memory
        uint32_t mapped_addr = addr & 0x3FFFFFF;  // Map to 64MB range
        if (mapped_addr >= mem.size()) return 0;
        return mem[mapped_addr];
    }
    
    void store8(uint32_t addr, uint8_t v) override {
        // UART output
        if (addr == MMIO_UART_BASE) {
            putchar(v);
            fflush(stdout);
            return;
        }
        
        // Framebuffer byte write (less common)
        if (addr >= MMIO_FB_BASE && addr < MMIO_FB_BASE + MMIO_FB_SIZE) {
            uint32_t pixel_offset = (addr - MMIO_FB_BASE) / 4;
            uint32_t byte_offset = (addr - MMIO_FB_BASE) % 4;
            if (pixel_offset < fb_width * fb_height) {
                uint32_t mask = 0xFF << (byte_offset * 8);
                framebuffer[pixel_offset] = (framebuffer[pixel_offset] & ~mask) | 
                                           ((uint32_t)v << (byte_offset * 8));
            }
            return;
        }
        
        // Regular memory - map high addresses down to fit in our memory
        uint32_t mapped_addr = addr & 0x3FFFFFF;  // Map to 64MB range
        if (mapped_addr >= mem.size()) return;
        mem[mapped_addr] = v;
    }
    
    bool load_binary(const uint8_t* data, size_t size, uint32_t load_addr = 0) override {
        // Map high addresses down to fit in our memory
        uint32_t mapped_addr = load_addr & 0x3FFFFFF;  // Map to 64MB range
        if (mapped_addr + size > mem.size()) return false;
        std::memcpy(mem.data() + mapped_addr, data, size);
        return true;
    }
    
    void update(uint64_t cycles) override {
        cycle_counter = cycles;
        update_counter++;
        
        // Handle SDL events periodically
        if (sdl_initialized && (update_counter % 10000 == 0)) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    quit_requested = true;
                } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                    // Convert SDL key to DOOM key
                    uint8_t doom_key = sdl_to_doom_key(event.key.keysym.sym);
                    if (doom_key != 0) {
                        // Add to queue: high bit set for keydown, clear for keyup
                        uint8_t key_event = doom_key;
                        if (event.type == SDL_KEYDOWN) {
                            key_event |= 0x80;  // Set high bit for keydown
                        }
                        kbd_queue.push_back(key_event);
                        
                        // Limit queue size to prevent overflow
                        if (kbd_queue.size() > 256) {
                            kbd_queue.erase(kbd_queue.begin(), kbd_queue.begin() + 128);
                            if (kbd_read_pos > 128) kbd_read_pos -= 128;
                            else kbd_read_pos = 0;
                        }
                    }
                }
            }
        }
        
        // Update display periodically
        if (sdl_initialized && (update_counter % 100000 == 0)) {
            SDL_UpdateTexture(texture, NULL, framebuffer, fb_width * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }
    }
    
    bool should_quit() override {
        return quit_requested;
    }
    
    size_t size() const override { return mem.size(); }
};

#endif // MEMORY_SUBSYSTEM_SDL_H