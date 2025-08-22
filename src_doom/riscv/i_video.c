/*
 * i_video.c
 *
 * Video system support code
 *
 * Copyright (C) 2021 Sylvain Munaut
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdint.h>
#include <string.h>

#include "doomdef.h"

#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

#include "config.h"

uint32_t pal[256];

void I_InitGraphics(void) {
  /* Don't need to do anything really ... */
  printf("I_InitGraphics: Initializing graphics system\n");
  
  // Initialize a basic palette for testing
  printf("I_InitGraphics: Initializing test palette\n");
  for (int i = 0; i < 256; i++) {
    // Create a rainbow palette
    int r = (i < 85) ? (i * 3) : ((i < 170) ? 255 : (255 - (i - 170) * 3));
    int g = (i < 85) ? 0 : ((i < 170) ? ((i - 85) * 3) : 255);
    int b = (i < 85) ? 0 : ((i < 170) ? 0 : ((i - 170) * 3));
    pal[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
  }
  
  // Draw a test pattern to verify framebuffer works
  uint32_t *fb = (uint32_t *)VID_BASE;
  printf("I_InitGraphics: Drawing test pattern to framebuffer at 0x%08x\n", VID_BASE);
  
  // Clear entire 640x480 framebuffer first
  for (int i = 0; i < 640 * 480; i++) {
    fb[i] = 0xFF000000; // Black
  }
  
  // Draw a simple gradient pattern in the center (scaled 2x)
  for (int y = 0; y < 100; y++) {
    for (int x = 0; x < 320; x++) {
      uint32_t color = 0xFF000000 | ((y*2) << 16) | (x & 0xFF);
      // Write 2x2 block
      int fb_y = (y * 2) + 40; // 40 pixel top border
      int fb_x = x * 2;
      int fb_base = fb_y * 640 + fb_x;
      fb[fb_base] = color;
      fb[fb_base + 1] = color;
      fb[fb_base + 640] = color;
      fb[fb_base + 641] = color;
    }
  }
  
  printf("I_InitGraphics: Test pattern drawn\n");

  /* Ok, maybe just set gamma default */
  usegamma = 1;
  
  printf("I_InitGraphics: COMPLETE\n");
}

void I_ShutdownGraphics(void) { /* Don't need to do anything really ... */ }

void I_SetPalette(byte *palette) {
  byte r, g, b;

  printf("I_SetPalette: Setting palette\n");
  for (int i = 0; i < 256; i++) {
    r = gammatable[usegamma][*palette++];
    g = gammatable[usegamma][*palette++];
    b = gammatable[usegamma][*palette++];
    pal[i] =
        ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b | 0xff << 24;
  }
}

void I_UpdateNoBlit(void) {}

void I_FinishUpdate(void) {
  /* Copy from RAM buffer to frame buffer with 2x scaling */
  uint32_t *framebuffer = (uint32_t *)VID_BASE;
  
  // Scale 320x200 to 640x400 (2x scaling)
  // Center vertically in 640x480 window (40 pixel borders top/bottom)
  int fb_width = 640;
  int y_offset = 40; // Center vertically: (480 - 400) / 2
  
  for (int y = 0; y < SCREENHEIGHT; y++) {
    for (int x = 0; x < SCREENWIDTH; x++) {
      uint32_t color = pal[((uint8_t *)screens[0])[y * SCREENWIDTH + x]];
      
      // Write 2x2 block for each pixel
      int fb_y = (y * 2) + y_offset;
      int fb_x = x * 2;
      int fb_base = fb_y * fb_width + fb_x;
      
      // Top-left pixel
      framebuffer[fb_base] = color;
      // Top-right pixel  
      framebuffer[fb_base + 1] = color;
      // Bottom-left pixel
      framebuffer[fb_base + fb_width] = color;
      // Bottom-right pixel
      framebuffer[fb_base + fb_width + 1] = color;
    }
  }
  
  // Clear top and bottom borders (40 pixels each)
  // Top border
  for (int i = 0; i < fb_width * y_offset; i++) {
    framebuffer[i] = 0xFF000000; // Black
  }
  // Bottom border  
  for (int i = fb_width * (400 + y_offset); i < fb_width * 480; i++) {
    framebuffer[i] = 0xFF000000; // Black
  }

  /* Very crude FPS measure (time to render 100 frames */
#if 1
  static int frame_cnt = 0;
  static int tick_prev = 0;

  if (++frame_cnt == 100) {
    int tick_now = I_GetTime();
    printf("FPS: %d\n", tick_now - tick_prev);
    tick_prev = tick_now;
    frame_cnt = 0;
  }
#endif
}

void I_WaitVBL(int count) {
  return;
  /* Buys-Wait for VBL status bit */
  static volatile uint32_t *const video_state = (void *)(VID_CTRL_BASE);
  while (!(video_state[0] & (1 << 16)))
    ;
}

void I_ReadScreen(byte *scr) {
  /* FIXME: Would have though reading from VID_BASE be better ...
   *        but it seems buggy. Not sure if the problem is in the
   *        gateware
   */
  memcpy(scr, screens[0], SCREENHEIGHT * SCREENWIDTH);
}

#if 0 /* WTF ? Not used ... */
void
I_BeginRead(void)
{
}

void
I_EndRead(void)
{
}
#endif
