/***************************************************************************
 *             __________               __   ___.                  
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___  
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /  
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <   
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \  
 *                     \/            \/     \/    \/            \/ 
 * $Id: misc.h,v 1.1 2007/09/21 18:43:37 duke4d Exp $
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
#ifndef MISC_H
#define MISC_H

/* Format a large-range value for output, using the appropriate unit so that
 * the displayed value is in the range 1 <= display < 1000 (1024 for "binary"
 * units) if possible, and 3 significant digits are shown. If a buffer is
 * given, the result is snprintf()'d into that buffer, otherwise the result is
 * voiced.*/
void output_dyn_value(char *buf, int buf_size, int value,
                      const unsigned char **units, bool bin_scale);

char *create_numbered_filename(char *buffer, const char *path, 
                               const char *prefix, const char *suffix,
                               int numberlen);
#ifdef HAVE_RTC
char *create_datetime_filename(char *buffer, const char *path,
                               const char *prefix, const char *suffix);
#endif

/* Read (up to) a line of text from fd into buffer and return number of bytes
 * read (which may be larger than the number of bytes stored in buffer). If 
 * an error occurs, -1 is returned (and buffer contains whatever could be 
 * read). A line is terminated by a LF char. Neither LF nor CR chars are 
 * stored in buffer.
 */
int read_line(int fd, char* buffer, int buffer_size);

#ifdef HAVE_LCD_BITMAP
/* Save a .BMP file containing the current screen contents. */
void screen_dump(void);
#endif

bool settings_parseline(char* line, char** name, char** value);
long default_event_handler_ex(long event, void (*callback)(void *), void *parameter);
long default_event_handler(long event);
void car_adapter_mode_init(void);
extern int show_logo(void);

#endif
