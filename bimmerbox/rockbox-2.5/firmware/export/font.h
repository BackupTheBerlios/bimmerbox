/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: font.h,v 1.1 2007/09/21 18:43:41 duke4d Exp $
 *
 * Copyright (c) 2002 by Greg Haerr <greg@censoft.com>
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#ifndef _FONT_H
#define _FONT_H

/*
 * Incore font and image definitions
 */
#include "config.h"

#if defined(HAVE_LCD_BITMAP) || defined(SIMULATOR)

/* max static loadable font buffer size */
#ifndef MAX_FONT_SIZE
#if LCD_HEIGHT > 64
#define MAX_FONT_SIZE   10000
#else
#define MAX_FONT_SIZE	4000
#endif
#endif

/*
 * Fonts are specified by number, and used for display
 * of menu information as well as mp3 filename data.
 * At system startup, up to MAXFONTS fonts are initialized,
 * either by being compiled-in, or loaded from disk.
 * If the font asked for does not exist, then the
 * system uses the next lower font number.  Font 0
 * must be available at system startup.
 * Fonts are specified in firmware/font.c.
 */
enum {
    FONT_SYSFIXED, /* system fixed pitch font*/
    FONT_UI,       /* system porportional font*/
    MAXFONTS
};

/*
 * .fnt loadable font file format definition
 *
 * format                     len  description
 * -------------------------  ---- ------------------------------
 * UCHAR version[4]              4   magic number and version bytes
 * USHORT maxwidth               2   font max width in pixels
 * USHORT height                 2   font height in pixels
 * USHORT ascent                 2   font ascent (baseline) in pixels
 * USHORT pad                    2   unused, pad to 32-bit boundary
 * ULONG firstchar               4   first character code in font
 * ULONG defaultchar             4   default character code in font
 * ULONG size                    4   # characters in font
 * ULONG nbits                   4   # bytes imagebits data in file
 * ULONG noffset                 4   # longs offset data in file
 * ULONG nwidth                  4   # bytes width data in file
 * MWIMAGEBITS bits          nbits   image bits variable data
 * [MWIMAGEBITS padded to 16-bit boundary]
 * USHORT offset         noffset*2   offset variable data
 * UCHAR width            nwidth*1   width variable data
 */

/* loadable font magic and version #*/
#define VERSION "RB12"

/* builtin C-based proportional/fixed font structure */
/* based on The Microwindows Project http://microwindows.org */
struct font {
    int		maxwidth;	/* max width in pixels*/
    unsigned int height;	/* height in pixels*/
    int		ascent;		/* ascent (baseline) height*/
    int		firstchar;	/* first character in bitmap*/
    int		size;		/* font size in glyphs*/
    const unsigned char *bits;		/* 8-bit column bitmap data*/
    const unsigned short *offset;	/* offsets into bitmap data*/
    const unsigned char *width;	/* character widths or NULL if fixed*/
    int		defaultchar;	/* default char (not glyph index)*/
};

/* font routines*/
void font_init(void);
struct font* font_load(const char *path);
struct font* font_get(int font);
void font_reset(void);
int font_getstringsize(const unsigned char *str, int *w, int *h, int fontnumber);

#else /* HAVE_LCD_BITMAP */

#define font_init()
#define font_load(x)

#endif

#endif
/* -----------------------------------------------------------------
 * vim: et sw=4 ts=8 sts=4 tw=78
 */
