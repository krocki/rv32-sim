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
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
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

static void init_fs(void) {
    if (!fs_initialized) {
        debug_puts("init_fs: Using REAL DOOM1.WAD\n");
        
        // The WAD is embedded at a specific offset in the binary
        // It should be around 0x80048b20 based on the binary layout
        unsigned char *wad_start = (unsigned char *)0x80048b20;
        
        // Verify it's actually there
        debug_puts("init_fs: Checking for IWAD at fixed location 0x80048b20...\n");
        if (wad_start[0] == 'I' && wad_start[1] == 'W' && wad_start[2] == 'A' && wad_start[3] == 'D') {
            debug_puts("init_fs: Found IWAD at expected location!\n");
        } else {
            // If not at expected location, search for it
            debug_puts("init_fs: IWAD not at expected location, searching...\n");
            unsigned char *search_start = (unsigned char *)0x80000000;
            wad_start = NULL;
            
            for (uint32_t offset = 0x40000; offset < 0x500000; offset += 4) {
                unsigned char *test = search_start + offset;
                if (test[0] == 'I' && test[1] == 'W' && test[2] == 'A' && test[3] == 'D') {
                    wad_start = test;
                    debug_puts("init_fs: Found IWAD at ");
                    debug_hex((unsigned int)wad_start);
                    debug_puts("\n");
                    break;
                }
            }
        }
        
        if (!wad_start) {
            debug_puts("init_fs: ERROR - Could not find IWAD magic!\n");
            wad_start = &_binary_doom1_real_wad_start;  // Fallback
        }
        
        // The WAD size is approximately 4MB
        uint32_t wad_size = 0x4006B4;  // From ls -l output
        
        debug_puts("init_fs: WAD at ");
        debug_hex((unsigned int)wad_start);
        debug_puts(", size = ");
        debug_hex(wad_size);
        debug_puts(" bytes\n");
        
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
        debug_puts("First read data: ");
        unsigned char* data = (unsigned char*)(fds[fd].data + fds[fd].offset);
        for (int i = 0; i < (nbyte < 16 ? nbyte : 16); i++) {
            debug_hex(data[i]);
            debug_puts(" ");
        }
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

// Override printf to use our UART directly
int printf(const char *fmt, ...) {
    static char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    for (int i = 0; i < len && buf[i]; i++) {
        *uart = buf[i];
    }
    
    return len;
}

// Stub implementations for stdio FILE operations
FILE *fopen(const char *pathname, const char *mode) {
    // For config files, just return NULL (file not found)
    // This will make DOOM use defaults
    return NULL;
}

int fclose(FILE *stream) {
    return 0;
}

int feof(FILE *stream) {
    return 1; // Always at end
}

int fscanf(FILE *stream, const char *format, ...) {
    return 0; // No items read
}

int fprintf(FILE *stream, const char *format, ...) {
    return 0;
}

char *fgets(char *s, int size, FILE *stream) {
    return NULL;
}
