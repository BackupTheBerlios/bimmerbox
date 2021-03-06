
/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: button.h,v 1.1 2007/09/21 18:43:41 duke4d Exp $
 *
 * Copyright (C) 2002 by Daniel Stenberg
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#ifndef _BUTTON_H_
#define _BUTTON_H_

#include <stdbool.h>
#include "config.h"

extern struct event_queue button_queue;

void button_init (void);
long button_get (bool block);
long button_get_w_tmo(int ticks);
int button_status(void);
void button_clear_queue(void);
#ifdef HAVE_LCD_BITMAP
void button_set_flip(bool flip); /* turn 180 degrees */
#endif

#if (CONFIG_KEYPAD == IRIVER_H100_PAD) || \
    (CONFIG_KEYPAD == IRIVER_H300_PAD)
bool button_hold(void);
bool remote_button_hold(void);
#endif

#define  BUTTON_NONE          0x0000

/* Shared button codes */
#define  BUTTON_LEFT          0x0040
#define  BUTTON_RIGHT         0x0080

/* Button modifiers */
#define  BUTTON_REMOTE        0x2000
#define  BUTTON_REPEAT        0x4000
#define  BUTTON_REL           0x8000

/* remote control buttons */
#define  BUTTON_RC_VOL_UP     (0x0008 | BUTTON_REMOTE)
#define  BUTTON_RC_VOL_DOWN   (0x0800 | BUTTON_REMOTE)
#define  BUTTON_RC_LEFT       (BUTTON_LEFT | BUTTON_REMOTE)
#define  BUTTON_RC_RIGHT      (BUTTON_RIGHT| BUTTON_REMOTE)

#if CONFIG_KEYPAD == IRIVER_H100_PAD

/* iRiver H100 specific button codes */
#define  BUTTON_SELECT        0x0100
#define  BUTTON_MODE          0x0200
#define  BUTTON_REC           0x0400
#define  BUTTON_ON            0x0001
#define  BUTTON_OFF           0x0002
#define  BUTTON_UP            0x0010
#define  BUTTON_DOWN          0x0020
#define  BUTTON_QUICK         BUTTON_ON
#define  BUTTON_QUICK_LONG    (BUTTON_ON | BUTTON_REPEAT)

#define BUTTON_RC_ON          (BUTTON_REMOTE | 0x00010000)
#define BUTTON_RC_STOP        (BUTTON_REMOTE | 0x00020000)
#define BUTTON_RC_MODE        (BUTTON_REMOTE | 0x00040000)
#define BUTTON_RC_BITRATE     (BUTTON_REMOTE | 0x00200000)
#define BUTTON_RC_REC         (BUTTON_REMOTE | 0x00400000)
#define BUTTON_RC_SOURCE      (BUTTON_REMOTE | 0x00800000)
#define BUTTON_RC_MENU        (BUTTON_REMOTE | 0x01000000)
#define BUTTON_RC_FF          (BUTTON_REMOTE | 0x02000000)
#define BUTTON_RC_REW         (BUTTON_REMOTE | 0x04000000)

#elif CONFIG_KEYPAD == IRIVER_H300_PAD

/* iRiver H300 specific button codes */
#define  BUTTON_SELECT        0x0100
#define  BUTTON_MODE          0x0200
#define  BUTTON_REC           0x0400
#define  BUTTON_ON            0x0001
#define  BUTTON_OFF           0x0002
#define  BUTTON_UP            0x0010
#define  BUTTON_DOWN          0x0020

#define BUTTON_RC_ON          (BUTTON_REMOTE | 0x00010000)
#define BUTTON_RC_STOP        (BUTTON_REMOTE | 0x00020000)
#define BUTTON_RC_MODE        (BUTTON_REMOTE | 0x00040000)
#define BUTTON_RC_BITRATE     (BUTTON_REMOTE | 0x00200000)
#define BUTTON_RC_REC         (BUTTON_REMOTE | 0x00400000)
#define BUTTON_RC_SOURCE      (BUTTON_REMOTE | 0x00800000)
#define BUTTON_RC_MENU        (BUTTON_REMOTE | 0x01000000)
#define BUTTON_RC_FF          (BUTTON_REMOTE | 0x02000000)
#define BUTTON_RC_REW         (BUTTON_REMOTE | 0x04000000)

#elif CONFIG_KEYPAD == RECORDER_PAD

/* Recorder specific button codes */
#define  BUTTON_ON            0x0001
#define  BUTTON_OFF           0x0002
#define  BUTTON_PLAY          0x0004
#define  BUTTON_UP            0x0010
#define  BUTTON_DOWN          0x0020
#define  BUTTON_F1            0x0100
#define  BUTTON_F2            0x0200
#define  BUTTON_F3            0x0400

#define  BUTTON_RC_PLAY       (BUTTON_PLAY | BUTTON_REMOTE)
#define  BUTTON_RC_STOP       (BUTTON_OFF | BUTTON_REMOTE)

#elif CONFIG_KEYPAD == PLAYER_PAD

/* Jukebox 6000 and Studio specific button codes */
#define  BUTTON_ON            0x0001
#define  BUTTON_MENU          0x0002
#define  BUTTON_PLAY          0x0010
#define  BUTTON_STOP          0x0020

#define  BUTTON_RC_PLAY       (BUTTON_PLAY | BUTTON_REMOTE)
#define  BUTTON_RC_STOP       (BUTTON_STOP | BUTTON_REMOTE)

#elif CONFIG_KEYPAD == ONDIO_PAD

/* Ondio specific button codes */
#define  BUTTON_OFF           0x0002
#define  BUTTON_UP            0x0010
#define  BUTTON_DOWN          0x0020
#define  BUTTON_MENU          0x0100

#elif CONFIG_KEYPAD == GMINI100_PAD

#define  BUTTON_ON            0x0001
#define  BUTTON_OFF           0x0002
#define  BUTTON_PLAY          0x0004
#define  BUTTON_UP            0x0010
#define  BUTTON_DOWN          0x0020
#define  BUTTON_MENU          0x0100

#endif /* RECORDER/PLAYER/ONDIO/GMINI KEYPAD */

#endif /* _BUTTON_H_ */

