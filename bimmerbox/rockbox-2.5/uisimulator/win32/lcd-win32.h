/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: lcd-win32.h,v 1.1 2007/09/21 18:43:43 duke4d Exp $
 *
 * Copyright (C) 2002 by Felix Arends
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#ifndef __LCDWIN32_H__
#define __LCDWIN32_H__

#include "uisw32.h"
#include "lcd.h"

// BITMAPINFO256
typedef struct
{
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD bmiColors[256];
} BITMAPINFO256;

extern char            bitmap[LCD_HEIGHT][LCD_WIDTH]; // the ui display
extern BITMAPINFO256   bmi; // bitmap information

#ifdef HAVE_REMOTE_LCD
extern char            remote_bitmap[LCD_REMOTE_HEIGHT][LCD_REMOTE_WIDTH];
extern BITMAPINFO256   remote_bmi; // bitmap information
#endif

void simlcdinit(void);

#endif // #ifndef __LCDWIN32_H__
