#include "sdl_framebuffer.h"
#include <iostream>

SDL_Framebuffer::SDL_Framebuffer(uint32_t w, uint32_t h) 
  : window(nullptr), renderer(nullptr), texture(nullptr), 
    pixels(nullptr), width(w), height(h), initialized(false) {
}

SDL_Framebuffer::~SDL_Framebuffer() {
  cleanup();
}

bool SDL_Framebuffer::init() {
  if (initialized) return true;
  
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
    return false;
  }
  
  window = SDL_CreateWindow("RV32IMA DOOM",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           width, height, SDL_WINDOW_SHOWN);
  if (!window) {
    std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
    return false;
  }
  
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
    return false;
  }
  
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                             SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!texture) {
    std::cerr << "Texture creation failed: " << SDL_GetError() << std::endl;
    return false;
  }
  
  // Allocate framebuffer memory
  pixels = new uint32_t[width * height];
  memset(pixels, 0, width * height * sizeof(uint32_t));
  
  initialized = true;
  std::cout << "SDL2 framebuffer initialized: " << width << "x" << height << std::endl;
  return true;
}

void SDL_Framebuffer::cleanup() {
  if (pixels) {
    delete[] pixels;
    pixels = nullptr;
  }
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

void SDL_Framebuffer::update_display() {
  if (!initialized) return;
  
  SDL_UpdateTexture(texture, nullptr, pixels, width * sizeof(uint32_t));
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}

bool SDL_Framebuffer::handle_events() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      return false;
    }
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
      return false;
    }
  }
  return true;
}

bool SDL_Framebuffer::is_framebuffer_addr(uint32_t addr) const {
  return addr >= FB_BASE_ADDR && addr < FB_BASE_ADDR + get_fb_size();
}

bool SDL_Framebuffer::is_control_addr(uint32_t addr) const {
  return addr >= FB_CTRL_ADDR && addr < FB_CTRL_ADDR + 0x100;
}

uint32_t SDL_Framebuffer::read32(uint32_t addr) {
  if (!initialized) return 0;
  
  if (is_framebuffer_addr(addr)) {
    uint32_t offset = (addr - FB_BASE_ADDR) / 4;
    if (offset < width * height) {
      return pixels[offset];
    }
  } else if (is_control_addr(addr)) {
    switch (addr - FB_CTRL_ADDR) {
      case 0x00: return width;   // FB_WIDTH
      case 0x04: return height;  // FB_HEIGHT
      case 0x08: return 32;      // FB_BPP
      case 0x0C: return 1;       // FB_ENABLE
      default: return 0;
    }
  }
  return 0;
}

void SDL_Framebuffer::write32(uint32_t addr, uint32_t value) {
  if (!initialized) return;
  
  if (is_framebuffer_addr(addr)) {
    uint32_t offset = (addr - FB_BASE_ADDR) / 4;
    if (offset < width * height) {
      pixels[offset] = value;
    }
  } else if (is_control_addr(addr)) {
    // Control register writes (could implement mode changes, etc.)
    switch (addr - FB_CTRL_ADDR) {
      case 0x10: // FB_FLUSH - force display update
        update_display();
        break;
    }
  }
}

uint8_t SDL_Framebuffer::read8(uint32_t addr) {
  if (is_framebuffer_addr(addr)) {
    uint32_t pixel_addr = addr & ~3;  // Align to 32-bit boundary
    uint32_t byte_offset = addr & 3;
    uint32_t pixel = read32(pixel_addr);
    return (pixel >> (byte_offset * 8)) & 0xFF;
  }
  return read32(addr & ~3) & 0xFF;
}

void SDL_Framebuffer::write8(uint32_t addr, uint8_t value) {
  if (is_framebuffer_addr(addr)) {
    uint32_t pixel_addr = addr & ~3;  // Align to 32-bit boundary
    uint32_t byte_offset = addr & 3;
    uint32_t pixel = read32(pixel_addr);
    uint32_t mask = 0xFF << (byte_offset * 8);
    pixel = (pixel & ~mask) | ((uint32_t)value << (byte_offset * 8));
    write32(pixel_addr, pixel);
  }
}