# DOOM Extensions for RV32IMA Simulator

## Required Modifications

### 1. Memory-Mapped I/O Handler

```cpp
// Add to CPU struct:
struct CPU {
    // ... existing members ...
    
    // Framebuffer (320x200, 8-bit indexed color)
    uint8_t framebuffer[320 * 200];
    uint8_t palette[256 * 3];  // RGB palette
    
    // Input state
    uint32_t keyboard_state;
    uint32_t mouse_x, mouse_y, mouse_buttons;
    
    // UART buffer
    std::queue<uint8_t> uart_rx_queue;
    
    // Timer
    uint64_t mtime_start;
};

// MMIO address map
#define MMIO_BASE       0x10000000
#define CLINT_BASE      0x10000000
#define CLINT_MTIME     (CLINT_BASE + 0xBFF8)
#define CLINT_MTIMECMP  (CLINT_BASE + 0x4000)
#define UART_BASE       0x11000000
#define FRAMEBUFFER_BASE 0x11100000
#define INPUT_BASE      0x11200000
```

### 2. Modified Memory Access

```cpp
uint32_t mmio_read(uint32_t addr) {
    // CLINT timer
    if (addr == CLINT_MTIME) {
        return (get_time_us() - mtime_start) & 0xFFFFFFFF;
    }
    if (addr == CLINT_MTIME + 4) {
        return ((get_time_us() - mtime_start) >> 32) & 0xFFFFFFFF;
    }
    
    // UART
    if (addr == UART_BASE) {
        // UART data register
        if (!uart_rx_queue.empty()) {
            uint8_t data = uart_rx_queue.front();
            uart_rx_queue.pop();
            return data;
        }
        return 0;
    }
    if (addr == UART_BASE + 4) {
        // UART status register
        return uart_rx_queue.empty() ? 0 : 1;
    }
    
    // Input devices
    if (addr == INPUT_BASE) {
        return keyboard_state;
    }
    if (addr == INPUT_BASE + 4) {
        return mouse_x | (mouse_y << 16);
    }
    if (addr == INPUT_BASE + 8) {
        return mouse_buttons;
    }
    
    // Framebuffer read
    if (addr >= FRAMEBUFFER_BASE && addr < FRAMEBUFFER_BASE + sizeof(framebuffer)) {
        return framebuffer[addr - FRAMEBUFFER_BASE];
    }
    
    return 0;
}

void mmio_write(uint32_t addr, uint32_t value, int size) {
    // UART
    if (addr == UART_BASE) {
        // Output character
        putchar(value & 0xFF);
        fflush(stdout);
        return;
    }
    
    // Framebuffer write
    if (addr >= FRAMEBUFFER_BASE && addr < FRAMEBUFFER_BASE + sizeof(framebuffer)) {
        if (size == 4) {
            // Write 4 pixels at once
            uint32_t offset = addr - FRAMEBUFFER_BASE;
            framebuffer[offset] = value & 0xFF;
            framebuffer[offset + 1] = (value >> 8) & 0xFF;
            framebuffer[offset + 2] = (value >> 16) & 0xFF;
            framebuffer[offset + 3] = (value >> 24) & 0xFF;
        } else if (size == 1) {
            framebuffer[addr - FRAMEBUFFER_BASE] = value & 0xFF;
        }
        // Trigger screen update if needed
        update_display();
        return;
    }
    
    // Palette write
    if (addr >= FRAMEBUFFER_BASE + 0x10000 && 
        addr < FRAMEBUFFER_BASE + 0x10000 + sizeof(palette)) {
        palette[addr - FRAMEBUFFER_BASE - 0x10000] = value & 0xFF;
        return;
    }
}
```

### 3. Linux Syscall Extensions

```cpp
void handle_syscall() {
    uint32_t syscall_num = x[17];  // a7
    
    switch (syscall_num) {
        case 93: // exit
            exit(x[10]);
            
        case 64: // write
            // ... existing implementation ...
            
        case 63: // read
            {
                uint32_t fd = x[10];
                uint32_t buf = x[11];
                uint32_t count = x[12];
                
                if (fd == 0) { // stdin
                    for (uint32_t i = 0; i < count; i++) {
                        int ch = getchar();
                        if (ch == EOF) break;
                        mem[buf + i] = ch;
                    }
                    x[10] = count;
                }
            }
            break;
            
        case 214: // brk (memory allocation)
            {
                static uint32_t heap_end = 0x80000000;
                uint32_t new_brk = x[10];
                if (new_brk == 0) {
                    x[10] = heap_end;
                } else if (new_brk > heap_end) {
                    heap_end = new_brk;
                    x[10] = heap_end;
                } else {
                    x[10] = heap_end;
                }
            }
            break;
            
        case 222: // mmap
            {
                // Simplified mmap - just allocate from high memory
                static uint32_t mmap_base = 0x70000000;
                uint32_t length = x[11];
                x[10] = mmap_base;
                mmap_base -= (length + 0xFFF) & ~0xFFF;
            }
            break;
            
        case 403: // clock_gettime
            {
                uint32_t clk_id = x[10];
                uint32_t timespec_ptr = x[11];
                uint64_t ns = get_time_ns();
                store32(timespec_ptr, ns / 1000000000);      // seconds
                store32(timespec_ptr + 4, ns % 1000000000);  // nanoseconds
                x[10] = 0;
            }
            break;
    }
}
```

### 4. Display Output (SDL2 or Terminal)

```cpp
// Option A: SDL2 for graphical output
#include <SDL2/SDL.h>

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* texture;

void init_display() {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("DOOM on RV32IMA", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 400, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
        SDL_TEXTUREACCESS_STREAMING, 320, 200);
}

void update_display() {
    uint32_t pixels[320 * 200];
    // Convert indexed color to RGB using palette
    for (int i = 0; i < 320 * 200; i++) {
        uint8_t index = framebuffer[i];
        uint8_t r = palette[index * 3];
        uint8_t g = palette[index * 3 + 1];
        uint8_t b = palette[index * 3 + 2];
        pixels[i] = (r << 16) | (g << 8) | b;
    }
    
    SDL_UpdateTexture(texture, NULL, pixels, 320 * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

// Option B: Terminal ASCII art output
void update_display_ascii() {
    // Convert framebuffer to ASCII art
    const char* ascii = " .:-=+*#%@";
    for (int y = 0; y < 200; y += 4) {
        for (int x = 0; x < 320; x += 2) {
            uint8_t pixel = framebuffer[y * 320 + x];
            putchar(ascii[pixel / 26]);
        }
        putchar('\n');
    }
}
```

### 5. Input Handling

```cpp
void poll_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                keyboard_state |= (1 << map_sdl_to_doom_key(event.key.keysym.sym));
                break;
            case SDL_KEYUP:
                keyboard_state &= ~(1 << map_sdl_to_doom_key(event.key.keysym.sym));
                break;
            case SDL_MOUSEMOTION:
                mouse_x = event.motion.x;
                mouse_y = event.motion.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
                mouse_buttons |= (1 << event.button.button);
                break;
            case SDL_MOUSEBUTTONUP:
                mouse_buttons &= ~(1 << event.button.button);
                break;
        }
    }
}
```

## Build Instructions

1. Install SDL2 (optional, for graphical output):
```bash
brew install sdl2  # macOS
sudo apt-get install libsdl2-dev  # Linux
```

2. Compile with DOOM extensions:
```bash
g++ -O3 -o rv32ima_doom rv32ima_doom.cc -lSDL2
```

3. Get DOOM WAD file and Linux image:
```bash
# Download mini-rv32ima Linux image with DOOM
wget https://github.com/cnlohr/mini-rv32ima-images/raw/master/doom.img

# Or build your own with buildroot
```

4. Run:
```bash
./rv32ima_doom doom.img
```

## Performance Considerations

- The original mini-rv32ima achieves ~450 CoreMark
- DOOM needs about 25-35 MHz equivalent performance
- Our simulator with -O3 should be fast enough
- Consider implementing block translation for better performance

## Limitations

- No sound support (could add via SDL_audio)
- Simplified memory model (no MMU)
- Basic input handling
- No save game support (no persistent storage)