#pragma once

#include <cstdint>
#include <vector>
#include <SDL2/SDL.h>

// Memory-mapped framebuffer for SDL2 display
class SDL_Framebuffer {
private:
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
  uint32_t* pixels;
  uint32_t width, height;
  bool initialized;
  
public:
  static constexpr uint32_t FB_BASE_ADDR = 0x50000000;  // Framebuffer base address
  static constexpr uint32_t FB_CTRL_ADDR = 0x50001000;  // Control registers
  
  SDL_Framebuffer(uint32_t w = 640, uint32_t h = 480);
  ~SDL_Framebuffer();
  
  bool init();
  void cleanup();
  void update_display();
  bool handle_events();
  
  // Memory interface
  bool is_framebuffer_addr(uint32_t addr) const;
  bool is_control_addr(uint32_t addr) const;
  uint32_t read32(uint32_t addr);
  void write32(uint32_t addr, uint32_t value);
  uint8_t read8(uint32_t addr);
  void write8(uint32_t addr, uint8_t value);
  
  uint32_t get_width() const { return width; }
  uint32_t get_height() const { return height; }
  uint32_t get_fb_size() const { return width * height * 4; }
};