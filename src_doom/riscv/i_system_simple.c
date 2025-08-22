/*
 * Simplified i_system.c for rv32-sim emulator
 * Removes hardware timer dependency
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "g_game.h"
#include "m_misc.h"
#include "i_sound.h"
#include "i_video.h"
#include "i_system.h"
#include "console.h"
#include "config.h"

// Simple tick counter instead of hardware timer
static int tick_counter = 0;

void I_Init(void) {
    printf("I_Init: System initialization\n");
}

int I_GetTime(void) {
    // Return simulated ticks (35 ticks per second)
    return tick_counter++;
}

void I_Quit(void) {
    D_QuitNetGame();
    I_ShutdownSound();
    I_ShutdownMusic();
    M_SaveDefaults();
    I_ShutdownGraphics();
    printf("I_Quit: Exiting\n");
    while(1); // Halt
}

void I_WaitVBL(int count) {
    // Simple delay
    for (volatile int i = 0; i < count * 10000; i++);
}

void I_BeginRead(void) {}
void I_EndRead(void) {}

byte* I_AllocLow(int length) {
    byte *mem = (byte *)malloc(length);
    memset(mem, 0, length);
    return mem;
}

void I_Error(char *error, ...) {
    va_list argptr;
    va_start(argptr, error);
    printf("Error: ");
    vprintf(error, argptr);
    printf("\n");
    va_end(argptr);
    
    while(1); // Halt
}

// Empty stub functions
void I_Tactile(int on, int off, int total) {}
ticcmd_t emptycmd;
ticcmd_t* I_BaseTiccmd(void) { return &emptycmd; }

// Empty joystick functions
void I_InitJoystick(void) {}
void I_ShutdownJoystick(void) {}
void I_ReadJoystick(void) {}
void I_UpdateJoystick(void) {}