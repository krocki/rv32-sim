// SDL2 Support for RV32IMA Emulator
// Provides framebuffer and input handling for graphical applications

#ifndef RV32IMA_SDL_H
#define RV32IMA_SDL_H

#ifdef USE_SDL

#ifdef __APPLE__
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include "rv32ima_core.h"

// SDL framebuffer configuration
#define FRAMEBUFFER_ADDR    0x11100000
#define FRAMEBUFFER_WIDTH   640
#define FRAMEBUFFER_HEIGHT  480
#define FRAMEBUFFER_SIZE    (FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * 4)

class SDLFramebuffer {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    uint32_t* pixels;
    bool initialized;
    
public:
    SDLFramebuffer() : window(nullptr), renderer(nullptr), texture(nullptr), 
                       pixels(nullptr), initialized(false) {
        pixels = new uint32_t[FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT];
        memset(pixels, 0, FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * sizeof(uint32_t));
    }
    
    ~SDLFramebuffer() {
        Cleanup();
        delete[] pixels;
    }
    
    bool Init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
            return false;
        }
        
        window = SDL_CreateWindow("RV32IMA Emulator",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT,
            SDL_WINDOW_SHOWN);
        
        if (!window) {
            printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
            return false;
        }
        
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
            return false;
        }
        
        texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);
        
        if (!texture) {
            printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
            return false;
        }
        
        printf("SDL2 Display initialized (%dx%d)\n", FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);
        initialized = true;
        return true;
    }
    
    void Cleanup() {
        if (texture) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        if (initialized) {
            SDL_Quit();
            initialized = false;
        }
    }
    
    void UpdateDisplay() {
        if (!initialized) return;
        
        SDL_UpdateTexture(texture, NULL, pixels, FRAMEBUFFER_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
    
    bool ProcessEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                return false;
            }
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    return false;
                }
            }
        }
        return true;
    }
    
    void WritePixel(uint32_t addr, uint32_t value, uint32_t size) {
        uint32_t offset = addr - FRAMEBUFFER_ADDR;
        if (offset < FRAMEBUFFER_SIZE) {
            if (size == 4) {
                uint32_t pixel_idx = offset / 4;
                if (pixel_idx < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT) {
                    pixels[pixel_idx] = value;
                }
            } else if (size == 2) {
                uint32_t pixel_idx = offset / 4;
                uint32_t byte_offset = offset % 4;
                if (pixel_idx < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT) {
                    uint16_t* ptr = (uint16_t*)&pixels[pixel_idx];
                    ptr[byte_offset / 2] = value;
                }
            } else if (size == 1) {
                uint32_t pixel_idx = offset / 4;
                uint32_t byte_offset = offset % 4;
                if (pixel_idx < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT) {
                    uint8_t* ptr = (uint8_t*)&pixels[pixel_idx];
                    ptr[byte_offset] = value;
                }
            }
        }
    }
    
    uint32_t ReadPixel(uint32_t addr, uint32_t size) {
        uint32_t offset = addr - FRAMEBUFFER_ADDR;
        if (offset < FRAMEBUFFER_SIZE) {
            if (size == 4) {
                uint32_t pixel_idx = offset / 4;
                if (pixel_idx < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT) {
                    return pixels[pixel_idx];
                }
            }
        }
        return 0;
    }
};

// Global SDL framebuffer instance
extern SDLFramebuffer* g_sdl_framebuffer;

// SDL-specific memory handlers
inline bool IsFramebufferAddress(uint32_t addr) {
    return addr >= FRAMEBUFFER_ADDR && addr < (FRAMEBUFFER_ADDR + FRAMEBUFFER_SIZE);
}

#endif // USE_SDL

#endif // RV32IMA_SDL_H