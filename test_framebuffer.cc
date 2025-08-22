#include <cstdint>
#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>

// Simple test program that writes colorful pixels to the framebuffer
int main() {
  const uint32_t FB_BASE = 0x50000000;
  const uint32_t FB_CTRL = 0x50001000;
  
  // Write pattern to framebuffer
  for (int y = 0; y < 100; y++) {
    for (int x = 0; x < 100; x++) {
      uint32_t addr = FB_BASE + (y * 640 + x) * 4;
      uint32_t color = 0xFF000000 | (x * 2) << 16 | (y * 2) << 8 | 0xFF;
      
      // Store pixel
      *reinterpret_cast<uint32_t*>(addr) = color;
    }
  }
  
  // Trigger display update
  *reinterpret_cast<uint32_t*>(FB_CTRL + 0x10) = 1;
  
  // Keep running
  while (true) {
    asm("nop");
  }
  
  return 0;
}