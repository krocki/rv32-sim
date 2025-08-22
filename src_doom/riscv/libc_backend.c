/*
 * libc_backend.c
 *
 * Minimal implementation of libc backend to support what DOOM uses
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


#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>

#include "config.h"
#include "console.h"


// #define LIBC_DEBUG


// HEAP handling
// -------------

extern uint8_t _heap_start;

// Debug output
#define UART_BASE 0x10000000
static volatile unsigned char *const uart = (void *)(UART_BASE);
static void debug_puts(const char *str) {
    while (*str) {
        *uart = *str++;
    }
}
static void debug_hex(unsigned int val) {
    char hex[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        *uart = hex[(val >> (i * 4)) & 0xF];
    }
}

void *
_sbrk(intptr_t increment)
{
    static void* heap_end = NULL;
    void *rv;
    
    // Initialize heap_end on first call
    if (heap_end == NULL) {
        heap_end = &_heap_start;
    }
    
    rv = heap_end;
    
    // Only log significant allocations
    if (increment > 0x1000) {
        debug_puts("_sbrk: large alloc ");
        debug_hex((unsigned int)increment);
        debug_puts(" bytes\n");
    }
    
    heap_end += increment;
#ifdef LIBC_DEBUG
    console_printf("Heap extended to %08x\n", (uint32_t)heap_end);
#endif
    return rv;
}


// File handling
// -------------

#define NUM_FDS		16

/* Flash "filesystem" */
static struct {
    const char *name;	/* Filename */
    size_t      len;	/* Length */
    void *      addr;	/* Address in flash */
} fs[2];  // Will be initialized at runtime

static struct {
    enum {
        FD_NONE  = 0,
        FD_STDIO = 1,
        FD_FLASH = 2,
    } type;
    size_t offset;
    size_t len;
    void   *data;
} fds[NUM_FDS] = {
    [0] = {
        .type = FD_STDIO,
    },
    [1] = {
        .type = FD_STDIO,
    },
    [2] = {
        .type = FD_STDIO,
    },
};

static int fs_initialized = 0;

// External symbols for the embedded WAD file
extern unsigned char _binary_doom1_real_wad_start;
extern unsigned char _binary_doom1_real_wad_end;
extern unsigned int _binary_doom1_real_wad_size;

static void init_fs(void) {
    if (!fs_initialized) {
        debug_puts("init_fs: Using REAL DOOM1.WAD\n");
        
        // Use the linker-generated symbols for the WAD location
        unsigned char *wad_start = &_binary_doom1_real_wad_start;
        unsigned char *wad_end = &_binary_doom1_real_wad_end;
        uint32_t wad_size = (uint32_t)(&_binary_doom1_real_wad_size);
        
        debug_puts("init_fs: WAD linked at ");
        debug_hex((unsigned int)wad_start);
        debug_puts(", size = ");
        debug_hex(wad_size);
        debug_puts(" bytes\n");
        
        // Verify it's actually there
        if (wad_start[0] == 'I' && wad_start[1] == 'W' && wad_start[2] == 'A' && wad_start[3] == 'D') {
            debug_puts("init_fs: Found valid IWAD at linked location!\n");
        } else {
            debug_puts("init_fs: ERROR - WAD at linked location is not valid!\n");
            debug_puts("init_fs: First bytes: ");
            for (int i = 0; i < 4; i++) {
                debug_hex(wad_start[i]);
                debug_puts(" ");
            }
            debug_puts("\n");
        }
        
        // Setup the filesystem entry for the WAD
        fs[0].name = "doom1.wad";
        fs[0].len = wad_size;
        fs[0].addr = (void*)wad_start;
        fs[1].name = NULL;
        
        // Verify WAD header
        debug_puts("WAD header: ");
        for (int i = 0; i < 12; i++) {
            debug_hex(wad_start[i]);
            debug_puts(" ");
        }
        debug_puts("\n");
        
        // Check for IWAD magic
        if (wad_start[0] == 'I' && wad_start[1] == 'W' && 
            wad_start[2] == 'A' && wad_start[3] == 'D') {
            debug_puts("init_fs: Valid IWAD detected!\n");
        } else {
            debug_puts("init_fs: WARNING - Invalid WAD magic!\n");
        }
        
        // Initialize FDs 3-15 to FD_NONE (0)
        for (int i = 3; i < NUM_FDS; i++) {
            fds[i].type = 0;  // FD_NONE
        }
        
        debug_puts("init_fs: File descriptors initialized\n");
        fs_initialized = 1;
        debug_puts("init_fs: COMPLETE - Real WAD loaded\n");
    }
}

int
_open(const char *pathname, int flags)
{
    int fn, fd;

    // Initialize filesystem on first use
    init_fs();

    /* Try to find file */
    for (fn=0; fs[fn].name; fn++) {
        if (!strcmp(pathname, fs[fn].name))
            break;
    }

    if (!fs[fn].name) {
        debug_puts("_open: file not found\n");
        errno = ENOENT;
        return -1;
    }

    /* Find free FD */
    for (fd=3; (fd<NUM_FDS) && (fds[fd].type != FD_NONE); fd++);
    if (fd == NUM_FDS) {
        debug_puts("No free FDs!\n");
        errno = ENOMEM;
        return -1;
    }

    /* "Open" file */
    fds[fd].type   = FD_FLASH;
    fds[fd].offset = 0;
    fds[fd].len    = fs[fn].len;
    fds[fd].data   = fs[fn].addr;

    return fd;
}

// Track total bytes read
static unsigned int total_bytes_read = 0;
static unsigned int read_count = 0;

ssize_t
_read(int fd, void *buf, size_t nbyte)
{
    // Only log first read
    if (read_count == 1) {
        debug_puts("_read called: fd=");
        debug_hex(fd);
        debug_puts(" nbyte=");
        debug_hex(nbyte);
        debug_puts("\n");
    }
    
    if ((fd < 0) || (fd >= NUM_FDS) || (fds[fd].type != FD_FLASH)) {
        debug_puts("_read: invalid fd ");
        debug_hex(fd);
        debug_puts("\n");
        errno = EINVAL;
        return -1;
    }

    if ((fds[fd].offset + nbyte) > fds[fd].len)
        nbyte = fds[fd].len - fds[fd].offset;

    // Track reads
    total_bytes_read += nbyte;
    read_count++;
    
    // Only report major milestones
    if (read_count == 1 || read_count % 500 == 0) {
        debug_puts("_read #");
        debug_hex(read_count);
        debug_puts(": ");
        debug_hex(nbyte);
        debug_puts(" bytes\n");
    }

    // For first read, dump the data being read
    if (read_count == 1) {
        debug_puts("First read data (hex bytes): ");
        unsigned char* data = (unsigned char*)(fds[fd].data + fds[fd].offset);
        for (int i = 0; i < (nbyte < 16 ? nbyte : 16); i++) {
            char hex[] = "0123456789ABCDEF";
            *uart = hex[(data[i] >> 4) & 0xF];
            *uart = hex[data[i] & 0xF];
            *uart = ' ';
        }
        debug_puts("\n");
        
        // Also show as characters
        debug_puts("First read data (chars): ");
        for (int i = 0; i < (nbyte < 12 ? nbyte : 12); i++) {
            if (data[i] >= 32 && data[i] < 127) {
                *uart = data[i];
            } else {
                *uart = '.';
            }
        }
        debug_puts("\n");
        
        // Show first 3 32-bit values from WAD header
        if (nbyte >= 12) {
            debug_puts("WAD header parsed: ID=");
            for (int i = 0; i < 4; i++) {
                *uart = data[i];
            }
            
            // Read numlumps (little-endian)
            uint32_t numlumps = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
            debug_puts(", numlumps=");
            debug_hex(numlumps);
            
            // Read infotableofs (little-endian)
            uint32_t infotableofs = data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24);
            debug_puts(", infotableofs=");
            debug_hex(infotableofs);
            debug_puts("\n");
        }
        
        // Show address of data
        debug_puts("Reading from address: ");
        debug_hex((unsigned int)(fds[fd].data + fds[fd].offset));
        debug_puts("\n");
    }

    memcpy(buf, fds[fd].data + fds[fd].offset, nbyte);
    fds[fd].offset += nbyte;

    return nbyte;
}

ssize_t
_write(int fd, const void *buf, size_t nbyte)
{
    const unsigned char *c = buf;
    for (int i=0; i<nbyte; i++)
        console_putchar(*c++);
    return nbyte;
}

int
_close(int fd)
{
    if ((fd < 0) || (fd >= NUM_FDS)) {
        errno = EINVAL;
        return -1;
    }

    fds[fd].type = FD_NONE;

    return 0;
}

off_t
_lseek(int fd, off_t offset, int whence)
{
    size_t new_offset;

    if ((fd < 0) || (fd >= NUM_FDS) || (fds[fd].type != FD_FLASH)) {
        errno = EINVAL;
        return -1;
    }

    switch (whence) {
    case SEEK_SET:
        new_offset = offset;
        break;
    case SEEK_CUR:
        new_offset = fds[fd].offset + offset;
        break;
    case SEEK_END:
        new_offset = fds[fd].len - offset;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    if ((new_offset < 0) || (new_offset > fds[fd].len)) {
        errno = EINVAL;
        return -1;
    }

    fds[fd].offset = new_offset;

    return new_offset;
}

int
_stat(const char *filename, struct stat *statbuf)
{
    /* Not implemented */
#ifdef LIBC_DEBUG
    console_printf("[1] Unimplemented _stat(filename=\"%s\")\n", filename);
#endif

    return -1;
}

int
_fstat(int fd, struct stat *statbuf)
{
    /* Not implemented */
#ifdef LIBC_DEBUG
    console_printf("[1] Unimplemented _fstat(fd=%d)\n", fd);
#endif

    return -1;
}

int
_isatty(int fd)
{
    /* Only stdout and stderr are TTY */
    errno = 0;
    return (fd == 1) || (fd == 2);
}

int
access(const char *pathname, int mode)
{
    int fn;

    // Initialize filesystem on first use
    init_fs();

    /* Try to find file */
    for (fn=0; fs[fn].name; fn++)
        if (!strcmp(pathname, fs[fn].name))
            break;

    if (!fs[fn].name) {
        errno = ENOENT;
        return -1;
    }

    /* Check requested access */
    if (mode & ~(R_OK | F_OK)) {
        errno = EACCES;
        return -1;
    }

    return 0;
}

// Provide errno for bare-metal
static int errno_val = 0;
int *__errno(void) {
    return &errno_val;
}

// Forward declaration of mini_vsnprintf (provided by mini-printf.c)
int mini_vsnprintf(char *buf, unsigned int size, const char *fmt, va_list ap);

// Wrapper for vsnprintf -> mini_vsnprintf
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    return mini_vsnprintf(buf, size, fmt, ap);
}

// sprintf wrapper
int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = mini_vsnprintf(buf, 1024, fmt, args);
    va_end(args);
    return len;
}

// Minimal memset implementation
void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

// _impure_ptr is provided by libc_nano, don't define it here

// Override printf to use our UART directly

int printf(const char *fmt, ...) {
    static char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = mini_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    for (int i = 0; i < len && buf[i]; i++) {
        *uart = buf[i];
    }
    
    return len;
}

// FILE operations are not needed for DOOM to run
// These functions are only used for config file handling which we skip
