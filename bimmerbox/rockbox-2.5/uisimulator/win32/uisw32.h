/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: uisw32.h,v 1.1 2007/09/21 18:43:43 duke4d Exp $
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

#ifndef __UISW32_H__
#define __UISW32_H__

#ifdef _MSC_VER
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif
#include <windows.h>
#include "lcd-win32.h"

#if defined(ARCHOS_RECORDER)
#define UI_TITLE                    "Jukebox Recorder"
#define UI_WIDTH                    270 // width of GUI window
#define UI_HEIGHT                   406 // height of GUI window
#define UI_LCD_BGCOLOR              90, 145, 90 // bkgnd color of LCD (no backlight)
#define UI_LCD_BGCOLORLIGHT         126, 229, 126 // bkgnd color of LCD (backlight)
#define UI_LCD_BLACK                0, 0, 0 // black
#define UI_LCD_POSX                 80 // x position of lcd
#define UI_LCD_POSY                 104 // y position of lcd (96 for real aspect)
#define UI_LCD_WIDTH                112
#define UI_LCD_HEIGHT               64 // (80 for real aspect)

#elif defined(ARCHOS_PLAYER)
#define UI_TITLE                    "Jukebox Player"
#define UI_WIDTH                    284 // width of GUI window
#define UI_HEIGHT                   420 // height of GUI window
#define UI_LCD_BGCOLOR              90, 145, 90 // bkgnd color of LCD (no backlight)
#define UI_LCD_BGCOLORLIGHT         126, 229, 126 // bkgnd color of LCD (backlight)
#define UI_LCD_BLACK                0, 0, 0 // black
#define UI_LCD_POSX                 75 // x position of lcd
#define UI_LCD_POSY                 111 // y position of lcd
#define UI_LCD_WIDTH                132
#define UI_LCD_HEIGHT               75

#elif defined(ARCHOS_FMRECORDER) || defined(ARCHOS_RECORDERV2)
#define UI_TITLE                    "Jukebox FM Recorder"
#define UI_WIDTH                    285 // width of GUI window
#define UI_HEIGHT                   414 // height of GUI window
#define UI_LCD_BGCOLOR              90, 145, 90 // bkgnd color of LCD (no backlight)
#define UI_LCD_BGCOLORLIGHT         126, 229, 126 // bkgnd color of LCD (backlight)
#define UI_LCD_BLACK                0, 0, 0 // black
#define UI_LCD_POSX                 87 // x position of lcd
#define UI_LCD_POSY                 77 // y position of lcd (69 for real aspect)
#define UI_LCD_WIDTH                112
#define UI_LCD_HEIGHT               64 // (80 for real aspect)

#elif defined(ARCHOS_ONDIOSP) ||  defined(ARCHOS_ONDIOFM)
#define UI_TITLE                    "Ondio"
#define UI_WIDTH                    155 // width of GUI window
#define UI_HEIGHT                   334 // height of GUI window
#define UI_LCD_BGCOLOR              90, 145, 90 // bkgnd color of LCD (no backlight)
#define UI_LCD_BGCOLORLIGHT         90, 145, 90 // bkgnd color of LCD (backlight)
#define UI_LCD_BLACK                0, 0, 0 // black
#define UI_LCD_POSX                 21 // x position of lcd
#define UI_LCD_POSY                 82 // y position of lcd (74 for real aspect)
#define UI_LCD_WIDTH                112
#define UI_LCD_HEIGHT               64 // (80 for real aspect)

#elif defined(IRIVER_H100_SERIES)
#define UI_TITLE                    "iriver H1x0"
#define UI_WIDTH                    379 // width of GUI window
#define UI_HEIGHT                   508 // height of GUI window
#define UI_LCD_BGCOLOR              90, 145, 90 // bkgnd color of LCD (no backlight)
#define UI_LCD_BGCOLORLIGHT         230, 216, 173 // bkgnd color of LCD (backlight)
#define UI_LCD_BLACK                0, 0, 0 // black
#define UI_LCD_POSX                 109 // x position of lcd
#define UI_LCD_POSY                 23 // y position of lcd
#define UI_LCD_WIDTH                160
#define UI_LCD_HEIGHT               128
#define UI_REMOTE_BGCOLOR           90, 145, 90 // bkgnd of remote lcd (no bklight)
#define UI_REMOTE_BGCOLORLIGHT      250, 180, 130 // bkgnd of remote lcd (bklight)
#define UI_REMOTE_POSX              50  // x position of remote lcd
#define UI_REMOTE_POSY              403 // y position of remote lcd
#define UI_REMOTE_WIDTH             128
#define UI_REMOTE_HEIGHT            64

#elif defined(ARCHOS_GMINI120)
#define UI_TITLE                    "Gmini 120"
#define UI_WIDTH                    370 // width of GUI window
#define UI_HEIGHT                   264 // height of GUI window
#define UI_LCD_BGCOLOR              90, 145, 90 // bkgnd color of LCD (no backlight)
#define UI_LCD_BGCOLORLIGHT         230, 160, 60 // bkgnd color of LCD (backlight)
#define UI_LCD_BLACK                0, 0, 0 // black
#define UI_LCD_POSX                 85 // x position of lcd
#define UI_LCD_POSY                 61 // y position of lcd (74 for real aspect)
#define UI_LCD_WIDTH                192 // * 1.5
#define UI_LCD_HEIGHT               96  // * 1.5

#endif

#define TIMER_EVENT                 0x34928340

extern HWND                         hGUIWnd; // the GUI window handle
extern unsigned int                 uThreadID; // id of mod thread
extern bool                         bActive;

// typedefs
typedef unsigned char               uchar;
typedef unsigned int                uint32;

#endif // #ifndef __UISW32_H__
