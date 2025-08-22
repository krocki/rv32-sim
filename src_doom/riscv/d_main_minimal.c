/*
 * Minimal D_DoomMain to find where it crashes
 */

#include "doomdef.h"
#include "doomstat.h"
#include "dstrings.h"
#include "sounds.h"
#include "z_zone.h"
#include "w_wad.h"
#include "s_sound.h"
#include "v_video.h"
#include "f_finale.h"
#include "f_wipe.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "p_setup.h"
#include "r_local.h"
#include "d_main.h"
#include "config.h"

// Draw status on screen
void draw_status(const char* msg, int color) {
    volatile uint32_t *fb = (uint32_t*)VID_BASE;
    static int line = 0;
    
    // Clear line
    for (int i = line * 640 * 20; i < (line + 1) * 640 * 20; i++) {
        fb[i] = 0xFF000000;
    }
    
    // Draw text indicator (simple colored bar)
    for (int i = line * 640 * 20; i < line * 640 * 20 + 2000; i++) {
        fb[i] = color;
    }
    
    line++;
    if (line > 20) line = 0;
}

void D_DoomMain(void) {
    // Show we're starting
    draw_status("D_DoomMain: Starting", 0xFF00FF00);
    
    // Initialize subsystems one by one
    draw_status("Z_Init: Memory", 0xFF00FF00);
    Z_Init();
    
    draw_status("M_LoadDefaults", 0xFF00FF00);
    M_LoadDefaults();
    
    draw_status("W_Init: WAD files", 0xFF00FF00);
    // Embedded WAD
    extern char _binary_doom1_wad_start[];
    extern char _binary_doom1_wad_end[];
    W_InitMultipleFiles(&"doom1.wad");
    
    draw_status("V_Init: Video", 0xFF00FF00);
    V_Init();
    
    draw_status("I_InitGraphics", 0xFF00FF00);
    I_InitGraphics();
    
    draw_status("HU_Init: Heads up", 0xFF00FF00);
    HU_Init();
    
    draw_status("ST_Init: Status bar", 0xFF00FF00);
    ST_Init();
    
    draw_status("R_Init: Renderer", 0xFF00FF00);
    R_Init();
    
    // Show success
    draw_status("DOOM Ready!", 0xFFFFFF00);
    
    // Main loop - just show test pattern
    while (1) {
        // Draw animated pattern to show we're alive
        static int frame = 0;
        volatile uint32_t *fb = (uint32_t*)VID_BASE;
        
        for (int x = 0; x < 640; x++) {
            fb[240 * 640 + x] = ((frame + x) % 100 < 50) ? 0xFFFF0000 : 0xFF00FF00;
        }
        
        frame++;
        for (volatile int i = 0; i < 100000; i++);
    }
}