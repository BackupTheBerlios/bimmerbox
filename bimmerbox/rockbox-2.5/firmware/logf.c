/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: logf.c,v 1.1 2007/09/21 18:43:41 duke4d Exp $
 *
 * Copyright (C) 2005 by Daniel Stenberg
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

/*
 * logf() logs MAX_LOGF_ENTRY (21) bytes per entry in a circular buffer. Each
 * logged string is space- padded for easier and faster output on screen. Just
 * output MAX_LOGF_ENTRY characters on each line. MAX_LOGF_ENTRY bytes fit
 * nicely on the iRiver remote LCD (128 pixels with an 8x6 pixels font).
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sprintf.h>
#include "config.h"
#include "lcd-remote.h"
#include "logf.h"

/* Only provide all this if asked to */
#ifdef ROCKBOX_HAS_LOGF

unsigned char logfbuffer[MAX_LOGF_LINES][MAX_LOGF_ENTRY];
int logfindex;
bool logfwrap;

#ifdef HAVE_REMOTE_LCD
static void displayremote(void)
{
    /* TODO: we should have a debug option that enables/disables this! */
    int w, h;
    int lines;
    int columns;
    int i;
    int index;

    lcd_remote_getstringsize("A", &w, &h);
    lines = LCD_REMOTE_HEIGHT/h;
    columns = LCD_REMOTE_WIDTH/w;
    lcd_remote_setmargins(0, 0);
    lcd_remote_clear_display();
    
    index = logfindex;
    for(i = lines-1; i>=0; i--) {
        unsigned char buffer[columns+1];

        if(--index < 0) {
            if(logfwrap)
                index = MAX_LOGF_LINES-1;
            else
                break; /* done */
        }
        
        memcpy(buffer, logfbuffer[index], columns);
        buffer[columns]=0;
        lcd_remote_puts(0, i, buffer);
    }
    lcd_remote_update();   
}
#else
#define displayremote()
#endif

void logf(const char *format, ...)
{
    int len;
    unsigned char *ptr;
    va_list ap;
    va_start(ap, format);

    if(logfindex >= MAX_LOGF_LINES) {
        /* wrap */
        logfwrap = true;
        logfindex = 0;
    }
    ptr = logfbuffer[logfindex];
    len = vsnprintf(ptr, MAX_LOGF_ENTRY, format, ap);
    va_end(ap);
    if(len < MAX_LOGF_ENTRY)
        /* pad with spaces up to the MAX_LOGF_ENTRY byte border */
        memset(ptr+len, ' ', MAX_LOGF_ENTRY-len);

    logfindex++; /* leave it where we write the next time */

    displayremote();
}

#endif
