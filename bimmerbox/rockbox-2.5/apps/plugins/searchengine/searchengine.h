/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: searchengine.h,v 1.1 2007/09/21 18:43:40 duke4d Exp $
 *
 * Copyright (C) 2005 by Michiel van der Kolk 
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#ifndef SEARCHENGINE_H
#define SEARCHENGINE_H
#include <plugin.h>
#include <database.h>
#include <autoconf.h>

extern int w, h, y;
#ifdef HAVE_LCD_BITMAP
#define PUTS(str) do { \
      rb->lcd_putsxy(1, y, str); \
      rb->lcd_getstringsize(str, &w, &h); \
      y += h + 1; \
} while (0); \
rb->lcd_update()
#else
#define PUTS(str) do { \
      rb->lcd_puts(0, y, str); \
      y = (y + 1) % 2; \
} while (0);
#endif

extern struct plugin_api* rb;

void *my_malloc(size_t size);
void setmallocpos(void *pointer);

#endif
