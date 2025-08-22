/*
 * Debug version of i_video.c
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "doomdef.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "config.h"

uint32_t pal[256];
static int frame_count = 0;

void I_InitGraphics(void) {
    printf("I_InitGraphics: Initializing video at %08x\n", VID_BASE);
    
    // Draw init pattern
    volatile uint32_t *fb = (uint32_t*)VID_BASE;
    for (int i = 0; i < 640 * 50; i++) {
        fb[i] = 0xFF00FF00; // Green stripe to show init
    }
    
    usegamma = 1;
}

void I_ShutdownGraphics(void) {
    printf("I_ShutdownGraphics\n");
}

void I_SetPalette(byte *palette) {
    byte r, g, b;
    
    printf("I_SetPalette: Setting palette\n");
    
    for (int i = 0; i < 256; i++) {
        r = gammatable[usegamma][*palette++];
        g = gammatable[usegamma][*palette++];
        b = gammatable[usegamma][*palette++];
        pal[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b | 0xff000000;
    }
}

void I_UpdateNoBlit(void) {}

void I_FinishUpdate(void) {
    volatile uint32_t *framebuffer = (uint32_t *)VID_BASE;
    
    // Show frame counter
    if (frame_count++ < 10) {
        printf("I_FinishUpdate: Frame %d, copying from %08x to %08x\n", 
               frame_count, (uint32_t)screens[0], VID_BASE);
    }
    
    // Copy screen buffer to framebuffer
    if (screens[0]) {
        for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; ++i) {
            framebuffer[i] = pal[((uint8_t *)screens[0])[i]];
        }
        
        // Draw frame indicator (blue dot in corner)
        framebuffer[frame_count % 640] = 0xFF0000FF;
    } else {
        printf("ERROR: screens[0] is NULL!\n");
    }
}

void I_WaitVBL(int count) {
    // Simple delay
    for (volatile int i = 0; i < count * 1000; i++);
}

void I_ReadScreen(byte *scr) {
    memcpy(scr, screens[0], SCREENHEIGHT * SCREENWIDTH);
}