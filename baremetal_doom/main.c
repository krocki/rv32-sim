// Bare-metal DOOM-like demo for RISC-V
// This is a simple 3D maze renderer inspired by DOOM

#include <stdint.h>
#include <stdbool.h>

// Hardware addresses
#define UART_BASE    0x10000000
#define FB_BASE      0x11100000
#define CLINT_BASE   0x11000000

// Framebuffer configuration
#define FB_WIDTH     640
#define FB_HEIGHT    480

// Game constants
#define MAP_WIDTH    16
#define MAP_HEIGHT   16
#define TILE_SIZE    64
#define FOV          60
#define HALF_FOV     30
#define RAY_COUNT    FB_WIDTH
#define MAX_DEPTH    1000
#define WALL_HEIGHT  64

// Simple map (1 = wall, 0 = empty)
static const uint8_t map[MAP_HEIGHT][MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,1,0,1,0,0,1,0,1,1,1,0,1},
    {1,0,1,0,0,0,1,0,0,1,0,0,0,1,0,1},
    {1,0,1,0,0,0,1,1,1,1,0,0,0,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,0,0,0,0,0,0,0,0,1,1,0,1},
    {1,0,0,0,0,0,1,1,1,1,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1},
    {1,0,1,1,0,0,1,0,0,1,0,0,1,1,0,1},
    {1,0,1,1,0,0,0,0,0,0,0,0,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,0,0,1,1,0,0,1,1,0,0,1,0,1},
    {1,0,1,1,0,0,0,0,0,0,0,0,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// Player state
typedef struct {
    float x, y;
    float angle;
    float dx, dy;
} Player;

static Player player = {
    .x = 150.0f,
    .y = 150.0f,
    .angle = 0.0f,
    .dx = 1.0f,
    .dy = 0.0f
};

// Framebuffer pointer
static volatile uint32_t* fb = (uint32_t*)FB_BASE;

// Simple math functions
static float fabs(float x) {
    return x < 0 ? -x : x;
}

static float sin_approx(float x) {
    // Simple sine approximation
    while (x > 3.14159f) x -= 6.28318f;
    while (x < -3.14159f) x += 6.28318f;
    
    float x2 = x * x;
    return x * (1.0f - x2 * (0.16666f - x2 * 0.00833f));
}

static float cos_approx(float x) {
    return sin_approx(x + 1.5708f);
}

static int min(int a, int b) {
    return a < b ? a : b;
}

static int max(int a, int b) {
    return a > b ? a : b;
}

// Draw a pixel
static void draw_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        fb[y * FB_WIDTH + x] = color;
    }
}

// Fill rectangle
static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            draw_pixel(x + dx, y + dy, color);
        }
    }
}

// Clear screen
static void clear_screen(uint32_t color) {
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb[i] = color;
    }
}

// Cast a ray and return distance to wall
static float cast_ray(float angle) {
    float ray_cos = cos_approx(angle);
    float ray_sin = sin_approx(angle);
    
    for (float dist = 0; dist < MAX_DEPTH; dist += 2.0f) {
        float x = player.x + ray_cos * dist;
        float y = player.y + ray_sin * dist;
        
        int map_x = (int)(x / TILE_SIZE);
        int map_y = (int)(y / TILE_SIZE);
        
        if (map_x >= 0 && map_x < MAP_WIDTH && 
            map_y >= 0 && map_y < MAP_HEIGHT) {
            if (map[map_y][map_x] == 1) {
                return dist;
            }
        }
    }
    
    return MAX_DEPTH;
}

// Render the 3D view
static void render_3d() {
    // Draw sky (top half)
    fill_rect(0, 0, FB_WIDTH, FB_HEIGHT/2, 0xFF87CEEB);
    
    // Draw floor (bottom half)
    fill_rect(0, FB_HEIGHT/2, FB_WIDTH, FB_HEIGHT/2, 0xFF404040);
    
    // Cast rays for each column
    float angle_step = (float)FOV / (float)RAY_COUNT;
    float start_angle = player.angle - HALF_FOV;
    
    for (int x = 0; x < FB_WIDTH; x++) {
        float ray_angle = start_angle + (x * angle_step * 3.14159f / 180.0f);
        float dist = cast_ray(ray_angle);
        
        // Fish-eye correction
        float corrected_dist = dist * cos_approx(ray_angle - player.angle);
        
        // Calculate wall height based on distance
        int wall_height = (int)((WALL_HEIGHT * FB_HEIGHT) / (corrected_dist + 1));
        wall_height = min(wall_height, FB_HEIGHT);
        
        int wall_top = (FB_HEIGHT - wall_height) / 2;
        int wall_bottom = wall_top + wall_height;
        
        // Calculate shading based on distance
        int shade = 255 - (int)(dist * 255 / MAX_DEPTH);
        shade = max(shade, 50);
        
        // Draw wall column
        for (int y = wall_top; y < wall_bottom; y++) {
            uint32_t color = 0xFF000000 | (shade << 16) | ((shade/2) << 8) | (shade/4);
            draw_pixel(x, y, color);
        }
    }
    
    // Draw crosshair
    for (int i = -10; i <= 10; i++) {
        draw_pixel(FB_WIDTH/2 + i, FB_HEIGHT/2, 0xFFFFFFFF);
        draw_pixel(FB_WIDTH/2, FB_HEIGHT/2 + i, 0xFFFFFFFF);
    }
    
    // Draw mini-map in corner
    int mini_scale = 4;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            uint32_t color = map[y][x] ? 0xFFFFFFFF : 0xFF000000;
            fill_rect(x * mini_scale, y * mini_scale, mini_scale, mini_scale, color);
        }
    }
    
    // Draw player on mini-map
    int px = (int)(player.x / TILE_SIZE) * mini_scale;
    int py = (int)(player.y / TILE_SIZE) * mini_scale;
    fill_rect(px, py, mini_scale, mini_scale, 0xFFFF0000);
}

// Simple UART output
static void uart_putc(char c) {
    volatile uint8_t* uart = (uint8_t*)UART_BASE;
    *uart = c;
}

static void uart_puts(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}

// Main game loop
void main(void) {
    uart_puts("Bare-metal DOOM starting...\r\n");
    uart_puts("Use WASD to move, Q/E to turn\r\n");
    
    // Main game loop
    int frame = 0;
    while (1) {
        // Clear and render
        render_3d();
        
        // Simple animation - rotate slowly
        player.angle += 0.01f;
        if (player.angle > 6.28318f) {
            player.angle -= 6.28318f;
        }
        
        // Move forward slowly for demo
        if (frame % 100 == 0) {
            float new_x = player.x + cos_approx(player.angle) * 5;
            float new_y = player.y + sin_approx(player.angle) * 5;
            
            int map_x = (int)(new_x / TILE_SIZE);
            int map_y = (int)(new_y / TILE_SIZE);
            
            // Check collision
            if (map_x >= 0 && map_x < MAP_WIDTH && 
                map_y >= 0 && map_y < MAP_HEIGHT &&
                map[map_y][map_x] == 0) {
                player.x = new_x;
                player.y = new_y;
            }
        }
        
        frame++;
        
        // Simple delay
        for (volatile int i = 0; i < 100000; i++);
    }
}