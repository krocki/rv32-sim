// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// $Log:$
//
// DESCRIPTION:
//      Handles WAD file header, directory, lump I/O.
//
//-----------------------------------------------------------------------------

static const char __attribute__((unused))
rcsid[] = "$Id: w_wad.c,v 1.5 1997/02/03 16:47:57 b1 Exp $";


#ifdef NORMALUNIX
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <alloca.h>
#define O_BINARY                0
#endif

#include "doomtype.h"
#include "m_swap.h"
#include "i_system.h"
#include "z_zone.h"

#ifdef __GNUG__
#pragma implementation "w_wad.h"
#endif
#include "w_wad.h"






//
// GLOBALS
//

// Location of each lump on disk.
lumpinfo_t*             lumpinfo;
int                     numlumps;

void**                  lumpcache;


#define strcmpi strcasecmp

char* strupr (char* str)
{
    char *s = str;
    while (*s) { *s = toupper(*s); s++; }
    return str;
}

int filelength (int handle)
{
    struct stat fileinfo;

    if (fstat (handle,&fileinfo) == -1)
        I_Error ("Error fstating");

    return fileinfo.st_size;
}


void
ExtractFileBase
( char*         path,
  char*         dest )
{
    char*       src;
    int         length;

    src = path + strlen(path) - 1;

    // back up until a \ or the start
    while (src != path
           && *(src-1) != '\\'
           && *(src-1) != '/')
    {
        src--;
    }

    // copy up to eight characters
    memset (dest,0,8);
    length = 0;

    while (*src && *src != '.')
    {
        if (++length == 9)
            I_Error ("Filename base of %s >8 chars",path);

        *dest++ = toupper((int)*src++);
    }
}





//
// LUMP BASED ROUTINES.
//

//
// W_AddFile
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
//
// If filename starts with a tilde, the file is handled
//  specially to allow map reloads.
// But: the reload feature is a fragile hack...

int                     reloadlump;
char*                   reloadname;


void W_AddFile (char *filename)
{
    wadinfo_t           header;
    lumpinfo_t*         lump_p;
    unsigned            i;
    int                 handle;
    int                 length;
    int                 startlump;
    filelump_t*         fileinfo;
    filelump_t          singleinfo;
    int                 storehandle;

    // open the file and add to directory

    // handle reload indicator.
    if (filename[0] == '~')
    {
        filename++;
        reloadname = filename;
        reloadlump = numlumps;
    }

    if ( (handle = open (filename,O_RDONLY | O_BINARY)) == -1)
    {
        printf (" couldn't open %s\n",filename);
        return;
    }

    printf (" adding %s\n",filename);
    startlump = numlumps;

    if (strcmpi (filename+strlen(filename)-3 , "wad" ) )
    {
        // single lump file
        fileinfo = &singleinfo;
        singleinfo.filepos = 0;
        singleinfo.size = LONG(filelength(handle));
        ExtractFileBase (filename, singleinfo.name);
        numlumps++;
    }
    else
    {
        // WAD file
        printf(" Reading WAD header (12 bytes)...\n");
        int bytes_read = read (handle, &header, sizeof(header));
        printf(" Read %d bytes for header\n", bytes_read);
        printf(" Header ID: %.4s\n", header.identification);
        printf(" Raw numlumps: 0x%x\n", header.numlumps);
        printf(" Raw infotableofs: 0x%x\n", header.infotableofs);
        if (strncmp(header.identification,"IWAD",4))
        {
            // Homebrew levels?
            if (strncmp(header.identification,"PWAD",4))
            {
                I_Error ("Wad file %s doesn't have IWAD "
                         "or PWAD id\n", filename);
            }

            // ???modifiedgame = true;
        }
        header.numlumps = LONG(header.numlumps);
        header.infotableofs = LONG(header.infotableofs);
        printf(" WAD has %d lumps, table at 0x%x\n", header.numlumps, header.infotableofs);
        length = header.numlumps*sizeof(filelump_t);
        printf(" Allocating %d bytes for lump table (stack alloca)\n", length);
        // Use malloc instead of alloca for large allocations
        if (length > 8192) {
            printf(" Using malloc instead of alloca for large lump table\n");
            fileinfo = malloc(length);
            if (!fileinfo) {
                I_Error("Out of memory allocating lump table");
            }
        } else {
            fileinfo = alloca (length);
        }
        printf(" Seeking to lump table...\n");
        lseek (handle, header.infotableofs, SEEK_SET);
        printf(" Reading %d bytes of lump info...\n", length);
        read (handle, fileinfo, length);
        numlumps += header.numlumps;
        printf(" Total lumps now: %d\n", numlumps);
    }


    // Fill in lumpinfo
    printf(" Reallocating lumpinfo for %d lumps\n", numlumps);
    lumpinfo = realloc (lumpinfo, numlumps*sizeof(lumpinfo_t));

    if (!lumpinfo)
        I_Error ("Couldn't realloc lumpinfo");

    lump_p = &lumpinfo[startlump];

    storehandle = reloadname ? -1 : handle;

    printf(" Processing lumps %d to %d\n", startlump, numlumps-1);
    for (i=startlump ; i<numlumps ; i++,lump_p++, fileinfo++)
    {
        lump_p->handle = storehandle;
        lump_p->position = LONG(fileinfo->filepos);
        lump_p->size = LONG(fileinfo->size);
        strncpy (lump_p->name, fileinfo->name, 8);
        
        // Show progress every 500 lumps
        if ((i - startlump) % 500 == 0) {
            printf("  Processed %d/%d lumps\n", i - startlump, numlumps - startlump);
        }
    }
    printf(" All lumps processed\n");

    if (reloadname)
        close (handle);
}




//
// W_Reload
// Flushes any of the reloadable lumps in memory
//  and reloads the directory.
//
void W_Reload (void)
{
    wadinfo_t           header;
    int                 lumpcount;
    lumpinfo_t*         lump_p;
    unsigned            i;
    int                 handle;
    int                 length;
    filelump_t*         fileinfo;

    if (!reloadname)
        return;

    if ( (handle = open (reloadname,O_RDONLY | O_BINARY)) == -1)
        I_Error ("W_Reload: couldn't open %s",reloadname);

    read (handle, &header, sizeof(header));
    lumpcount = LONG(header.numlumps);
    header.infotableofs = LONG(header.infotableofs);
    length = lumpcount*sizeof(filelump_t);
    fileinfo = alloca (length);
    lseek (handle, header.infotableofs, SEEK_SET);
    read (handle, fileinfo, length);

    // Fill in lumpinfo
    lump_p = &lumpinfo[reloadlump];

    for (i=reloadlump ;
         i<reloadlump+lumpcount ;
         i++,lump_p++, fileinfo++)
    {
        if (lumpcache[i])
            Z_Free (lumpcache[i]);

        lump_p->position = LONG(fileinfo->filepos);
        lump_p->size = LONG(fileinfo->size);
    }

    close (handle);
}



//
// W_InitMultipleFiles
// Pass a null terminated list of files to use.
// All files are optional, but at least one file
//  must be found.
// Files with a .wad extension are idlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
// Lump names can appear multiple times.
// The name searcher looks backwards, so a later file
//  does override all earlier ones.
//
void W_InitMultipleFiles (char** filenames)
{
    int         size;

    printf("W_InitMultipleFiles: Starting\n");

    // open all the files, load headers, and count lumps
    numlumps = 0;

    // will be realloced as lumps are added
    lumpinfo = malloc(1);

    for ( ; *filenames ; filenames++) {
        printf("W_InitMultipleFiles: Processing file %s\n", *filenames);
        W_AddFile (*filenames);
    }

    if (!numlumps)
        I_Error ("W_InitFiles: no files found");

    printf("W_InitMultipleFiles: Total lumps loaded: %d\n", numlumps);

    // set up caching
    size = numlumps * sizeof(*lumpcache);
    printf("W_InitMultipleFiles: Allocating %d bytes for lump cache\n", size);
    lumpcache = malloc (size);

    if (!lumpcache)
        I_Error ("Couldn't allocate lumpcache");

    memset (lumpcache,0, size);
    printf("W_InitMultipleFiles: Complete!\n");
}




//
// W_InitFile
// Just initialize from a single file.
//
void W_InitFile (char* filename)
{
    char*       names[2];

    names[0] = filename;
    names[1] = NULL;
    W_InitMultipleFiles (names);
}



//
// W_NumLumps
//
int W_NumLumps (void)
{
    return numlumps;
}



//
// W_CheckNumForName
// Returns -1 if name not found.
//

int W_CheckNumForName (char* name)
{
    union {
        char    s[9];
        int     x[2];

    } name8;

    int         v1;
    int         v2;
    lumpinfo_t* lump_p;

    // make the name into two integers for easy compares
    strncpy (name8.s,name,8);

    // in case the name was a fill 8 chars
    name8.s[8] = 0;

    // case insensitive
    strupr (name8.s);

    v1 = name8.x[0];
    v2 = name8.x[1];


    // scan backwards so patch lump files take precedence
    lump_p = lumpinfo + numlumps;

    while (lump_p-- != lumpinfo)
    {
        if ( *(int *)lump_p->name == v1
             && *(int *)&lump_p->name[4] == v2)
        {
            return lump_p - lumpinfo;
        }
    }

    // TFB. Not found.
    return -1;
}




//
// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//
int W_GetNumForName (char* name)
{
    int i;

    i = W_CheckNumForName (name);

    if (i == -1)
      I_Error ("W_GetNumForName: %s not found!", name);

    return i;
}


//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//
int W_LumpLength (int lump)
{
    if (lump >= numlumps)
        I_Error ("W_LumpLength: %i >= numlumps",lump);

    return lumpinfo[lump].size;
}



//
// W_ReadLump
// Loads the lump into the given buffer,
//  which must be >= W_LumpLength().
//
void
W_ReadLump
( int           lump,
  void*         dest )
{
    int         c;
    lumpinfo_t* l;
    int         handle;

    if (lump >= numlumps)
        I_Error ("W_ReadLump: %i >= numlumps",lump);

    l = lumpinfo+lump;

    // ??? I_BeginRead ();

    if (l->handle == -1)
    {
        // reloadable file, so use open / read / close
        if ( (handle = open (reloadname,O_RDONLY | O_BINARY)) == -1)
            I_Error ("W_ReadLump: couldn't open %s",reloadname);
    }
    else
        handle = l->handle;

    lseek (handle, l->position, SEEK_SET);
    c = read (handle, dest, l->size);

    if (c < l->size)
        I_Error ("W_ReadLump: only read %i of %i on lump %i",
                 c,l->size,lump);

    if (l->handle == -1)
        close (handle);

    // ??? I_EndRead ();
}




//
// W_CacheLumpNum
//
void*
W_CacheLumpNum
( int           lump,
  int           tag )
{
    if ((unsigned)lump >= numlumps)
        I_Error ("W_CacheLumpNum: %i >= numlumps",lump);

    if (!lumpcache[lump])
    {
        // read the lump in

        //printf ("cache miss on lump %i\n",lump);
        Z_Malloc (W_LumpLength (lump), tag, &lumpcache[lump]);
        W_ReadLump (lump, lumpcache[lump]);
    }
    else
    {
        //printf ("cache hit on lump %i\n",lump);
        Z_ChangeTag (lumpcache[lump],tag);
    }

    return lumpcache[lump];
}



//
// W_CacheLumpName
//
void*
W_CacheLumpName
( char*         name,
  int           tag )
{
    return W_CacheLumpNum (W_GetNumForName(name), tag);
}


//
// W_Profile
//
int             info[2500][10];
int             profilecount;

void W_Profile (void)
{
    int         i;
    memblock_t* block;
    void*       ptr;
    char        ch;
    FILE*       f;
    int         j;
    char        name[9];


    for (i=0 ; i<numlumps ; i++)
    {
        ptr = lumpcache[i];
        if (!ptr)
        {
            ch = ' ';
            continue;
        }
        else
        {
            block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
            if (block->tag < PU_PURGELEVEL)
                ch = 'S';
            else
                ch = 'P';
        }
        info[i][profilecount] = ch;
    }
    profilecount++;

    f = fopen ("waddump.txt","w");
    name[8] = 0;

    for (i=0 ; i<numlumps ; i++)
    {
        memcpy (name,lumpinfo[i].name,8);

        for (j=0 ; j<8 ; j++)
            if (!name[j])
                break;

        for ( ; j<8 ; j++)
            name[j] = ' ';

        fprintf (f,"%s ",name);

        for (j=0 ; j<profilecount ; j++)
            fprintf (f,"    %c",info[i][j]);

        fprintf (f,"\n");
    }
    fclose (f);
}


