/***************************************************************************
 *             __________               __   ___.                  
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___  
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /  
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <   
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \  
 *                     \/            \/     \/    \/            \/ 
 * $Id: misc.c,v 1.1 2007/09/21 18:43:37 duke4d Exp $
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
#include <stdlib.h>
#include <ctype.h>
#include "lang.h"
#include "string.h"
#include "config.h"
#include "file.h"
#include "dir.h"
#include "lcd.h"
#include "lcd-remote.h"
#include "sprintf.h"
#include "errno.h"
#include "system.h"
#include "timefuncs.h"
#include "screens.h"
#include "talk.h"
#include "mpeg.h"
#include "audio.h"
#include "mp3_playback.h"
#include "settings.h"
#include "ata.h"
#include "kernel.h"
#include "power.h"
#include "powermgmt.h"
#include "backlight.h"
#include "atoi.h"
#include "version.h"
#include "font.h"
#ifdef HAVE_MMC
#include "ata_mmc.h"
#endif
#include "tree.h"

#ifdef HAVE_LCD_BITMAP
#include "bmp.h"
#include "icons.h"
#endif /* End HAVE_LCD_BITMAP */

/* Format a large-range value for output, using the appropriate unit so that
 * the displayed value is in the range 1 <= display < 1000 (1024 for "binary"
 * units) if possible, and 3 significant digits are shown. If a buffer is
 * given, the result is snprintf()'d into that buffer, otherwise the result is
 * voiced.*/
char *output_dyn_value(char *buf, int buf_size, int value,
                       const unsigned char **units, bool bin_scale)
{
    int scale = bin_scale ? 1024 : 1000;
    int fraction = 0;
    int unit_no = 0;
    int i;
    char tbuf[5];

    while (value >= scale)
    {
        fraction = value % scale;
        value /= scale;
        unit_no++;
    }
    if (bin_scale)
        fraction = fraction * 1000 / 1024;

    if (value >= 100 || !unit_no)
        tbuf[0] = '\0';
    else if (value >= 10)
        snprintf(tbuf, sizeof(tbuf), "%01d", fraction / 100);
    else
        snprintf(tbuf, sizeof(tbuf), "%02d", fraction / 10);
    
    if (buf)
    {
        if (strlen(tbuf))
            snprintf(buf, buf_size, "%d%s%s%s", value, str(LANG_POINT),
                     tbuf, P2STR(units[unit_no]));
        else
            snprintf(buf, buf_size, "%d%s", value, P2STR(units[unit_no]));
    }
    else
    {
        /* strip trailing zeros from the fraction */
        for (i = strlen(tbuf) - 1; (i >= 0) && (tbuf[i] == '0'); i--)
            tbuf[i] = '\0';

        talk_number(value, true);
        if (tbuf[0] != 0)
        {
            talk_id(LANG_POINT, true);
            talk_spell(tbuf, true);
        }
        talk_id(P2ID(units[unit_no]), true);
    }
    return buf;
}

/* Create a filename with a number part in a way that the number is 1
   higher than the highest numbered file matching the same pattern.
   It is allowed that buffer and path point to the same memory location,
   saving a strcpy(). Path must always be given without trailing slash,. */
char *create_numbered_filename(char *buffer, const char *path,
                               const char *prefix, const char *suffix,
                               int numberlen)
{
    DIR *dir;
    struct dirent *entry;
    int max_num = 0;
    int pathlen;
    int prefixlen = strlen(prefix);
    char fmtstring[12];

    if (buffer != path)
        strncpy(buffer, path, MAX_PATH);

    pathlen = strlen(buffer);

    dir = opendir(pathlen ? buffer : "/");
    if (!dir)
        return NULL;

    while ((entry = readdir(dir)))
    {
        int curr_num;

        if (strncasecmp(entry->d_name, prefix, prefixlen)
            || strcasecmp(entry->d_name + prefixlen + numberlen, suffix))
            continue;

        curr_num = atoi(entry->d_name + prefixlen);
        if (curr_num > max_num)
            max_num = curr_num;
    }
    closedir(dir);

    snprintf(fmtstring, sizeof(fmtstring), "/%%s%%0%dd%%s", numberlen);
    snprintf(buffer + pathlen, MAX_PATH - pathlen, fmtstring, prefix,
             max_num + 1, suffix);

    return buffer;
}

#ifdef HAVE_RTC
/* Create a filename with a date+time part.
   It is allowed that buffer and path point to the same memory location,
   saving a strcpy(). Path must always be given without trailing slash. */
char *create_datetime_filename(char *buffer, const char *path,
                               const char *prefix, const char *suffix)
{
    struct tm *tm = get_time();
    int pathlen;

    if (buffer != path)
        strncpy(buffer, path, MAX_PATH);

    pathlen = strlen(buffer);
    snprintf(buffer + pathlen, MAX_PATH - pathlen,
             "/%s%02d%02d%02d-%02d%02d%02d%s", prefix,
             tm->tm_year % 100, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec, suffix);

    return buffer;
}
#endif /* HAVE_RTC */

/* Read (up to) a line of text from fd into buffer and return number of bytes
 * read (which may be larger than the number of bytes stored in buffer). If
 * an error occurs, -1 is returned (and buffer contains whatever could be
 * read). A line is terminated by a LF char. Neither LF nor CR chars are
 * stored in buffer.
 */
int read_line(int fd, char* buffer, int buffer_size)
{
    int count = 0;
    int num_read = 0;
    
    errno = 0;

    while (count < buffer_size)
    {
        unsigned char c;

        if (1 != read(fd, &c, 1))
            break;
        
        num_read++;
            
        if ( c == '\n' )
            break;

        if ( c == '\r' )
            continue;

        buffer[count++] = c;
    }

    buffer[MIN(count, buffer_size - 1)] = 0;

    return errno ? -1 : num_read;
}

#ifdef HAVE_LCD_BITMAP

#if LCD_DEPTH <= 8
#define BMP_NUMCOLORS (1 << LCD_DEPTH)
#else
#define BMP_NUMCOLORS 0
#endif

#if LCD_DEPTH == 1
#define BMP_BPP 1
#define BMP_LINESIZE ((LCD_WIDTH/8 + 3) & ~3)
#elif LCD_DEPTH <= 4
#define BMP_BPP 4
#define BMP_LINESIZE ((LCD_WIDTH/2 + 3) & ~3)
#elif LCD_DEPTH <= 8
#define BMP_BPP 8
#define BMP_LINESIZE ((LCD_WIDTH + 3) & ~3)
#elif LCD_DEPTH <= 16
#define BMP_BPP 16
#define BMP_LINESIZE ((LCD_WIDTH*2 + 3) & ~3)
#else
#define BMP_BPP 24
#define BMP_LINESIZE ((LCD_WIDTH*3 + 3) & ~3)
#endif

#define BMP_HEADERSIZE (54 + 4 * BMP_NUMCOLORS)
#define BMP_DATASIZE   (BMP_LINESIZE * LCD_HEIGHT)
#define BMP_TOTALSIZE  (BMP_HEADERSIZE + BMP_DATASIZE)

#define LE16_CONST(x) (x)&0xff, ((x)>>8)&0xff
#define LE32_CONST(x) (x)&0xff, ((x)>>8)&0xff, ((x)>>16)&0xff, ((x)>>24)&0xff

static const unsigned char bmpheader[] =
{
    0x42, 0x4d,                 /* 'BM' */
    LE32_CONST(BMP_TOTALSIZE),  /* Total file size */
    0x00, 0x00, 0x00, 0x00,     /* Reserved */
    LE32_CONST(BMP_HEADERSIZE), /* Offset to start of pixel data */

    0x28, 0x00, 0x00, 0x00,     /* Size of (2nd) header */
    LE32_CONST(LCD_WIDTH),      /* Width in pixels */
    LE32_CONST(LCD_HEIGHT),     /* Height in pixels */
    0x01, 0x00,                 /* Number of planes (always 1) */
    LE16_CONST(BMP_BPP),        /* Bits per pixel 1/4/8/16/24 */
    0x00, 0x00, 0x00, 0x00,     /* Compression mode, 0 = none */
    LE32_CONST(BMP_DATASIZE),   /* Size of bitmap data */
    0xc4, 0x0e, 0x00, 0x00,     /* Horizontal resolution (pixels/meter) */
    0xc4, 0x0e, 0x00, 0x00,     /* Vertical resolution (pixels/meter) */
    LE32_CONST(BMP_NUMCOLORS),  /* Number of used colours */
    LE32_CONST(BMP_NUMCOLORS),  /* Number of important colours */

#if LCD_DEPTH == 1
    0x90, 0xee, 0x90, 0x00,     /* Colour #0 */
    0x00, 0x00, 0x00, 0x00      /* Colour #1 */
#elif LCD_DEPTH == 2
    0xe6, 0xd8, 0xad, 0x00,     /* Colour #0 */
    0x99, 0x90, 0x73, 0x00,     /* Colour #1 */
    0x4c, 0x48, 0x39, 0x00,     /* Colour #2 */
    0x00, 0x00, 0x00, 0x00      /* Colour #3 */
#endif
};

void screen_dump(void)
{
    int fh;
    int bx, by, iy;
    int src_byte;
    char filename[MAX_PATH];
#if LCD_DEPTH == 1
    int ix, src_mask, dst_mask;
    static unsigned char line_block[8][BMP_LINESIZE];
#elif LCD_DEPTH == 2
    int src_byte2;
    static unsigned char line_block[4][BMP_LINESIZE];
#endif

#ifdef HAVE_RTC
    create_datetime_filename(filename, "", "dump ", ".bmp");
#else
    create_numbered_filename(filename, "", "dump_", ".bmp", 4);
#endif

    fh = creat(filename, O_WRONLY);
    if (fh < 0)
        return;

    write(fh, bmpheader, sizeof(bmpheader));

    /* BMP image goes bottom up */
#if LCD_DEPTH == 1
    for (by = LCD_HEIGHT/8 - 1; by >= 0; by--)
    {
        memset(&line_block[0][0], 0, sizeof(line_block));

        for (bx = 0; bx < LCD_WIDTH/8; bx++)
        {
            dst_mask = 0x01;
            for (ix = 7; ix >= 0; ix--)
            {
                src_byte = lcd_framebuffer[by][8*bx+ix];
                src_mask = 0x01;
                for (iy = 7; iy >= 0; iy--)
                {
                    if (src_byte & src_mask)
                        line_block[iy][bx] |= dst_mask;
                    src_mask <<= 1;
                }
                dst_mask <<= 1;
            }
        }
        
        write(fh, &line_block[0][0], sizeof(line_block));
    }
#elif LCD_DEPTH == 2
    for (by = LCD_HEIGHT/4 - 1; by >= 0; by--)
    {
        memset(&line_block[0][0], 0, sizeof(line_block));

        for (bx = 0; bx < LCD_WIDTH/2; bx++)
        {
            src_byte = lcd_framebuffer[by][2*bx];
            src_byte2 = lcd_framebuffer[by][2*bx+1];
            for (iy = 3; iy >= 0; iy--)
            {
                line_block[iy][bx] = ((src_byte & 3) << 4) | (src_byte2 & 3);
                src_byte >>= 2;
                src_byte2 >>= 2;
            }
        }

        write(fh, &line_block[0][0], sizeof(line_block));
    }
#endif
    close(fh);
}
#endif

/* parse a line from a configuration file. the line format is:

   name: value

   Any whitespace before setting name or value (after ':') is ignored.
   A # as first non-whitespace character discards the whole line.
   Function sets pointers to null-terminated setting name and value.
   Returns false if no valid config entry was found.
*/

bool settings_parseline(char* line, char** name, char** value)
{
    char* ptr;

    while ( isspace(*line) )
        line++;

    if ( *line == '#' )
        return false;

    ptr = strchr(line, ':');
    if ( !ptr )
        return false;

    *name = line;
    *ptr = 0;
    ptr++;
    while (isspace(*ptr))
        ptr++;
    *value = ptr;
    return true;
}

static void system_flush(void)
{
    tree_flush();
}

static void system_restore(void)
{
    tree_restore();
}

static bool clean_shutdown(void (*callback)(void *), void *parameter)
{
#ifdef SIMULATOR
    (void)callback;
    (void)parameter;
    exit(0);
#else
#ifndef HAVE_POWEROFF_WHILE_CHARGING
    if(!charger_inserted())
#endif
    {
        lcd_clear_display();
        splash(0, true, str(LANG_SHUTTINGDOWN));
        if (callback != NULL)
            callback(parameter);

        system_flush();
        
        shutdown_hw();
    }
#endif
    return false;
}

#ifdef HAVE_CHARGING
static bool waiting_to_resume_play = false;
static long play_resume_tick;

static void car_adapter_mode_processing(bool inserted)
{    
    if (global_settings.car_adapter_mode)
    {
        if(inserted)
        { 
            /*
             * Just got plugged in, delay & resume if we were playing
             */
            if (audio_status() & AUDIO_STATUS_PAUSE)
            {
                /* delay resume a bit while the engine is cranking */
                play_resume_tick = current_tick + HZ*5;
                waiting_to_resume_play = true;
            }
        }
        else
        {
            /*
             * Just got unplugged, pause if playing
             */
            if ((audio_status() & AUDIO_STATUS_PLAY) &&
                !(audio_status() & AUDIO_STATUS_PAUSE))
            {
                audio_pause(); 
            }
        }
    }
}

static void car_adapter_tick(void)
{
    if (waiting_to_resume_play)
    {
        if (TIME_AFTER(current_tick, play_resume_tick))
        {
            if (audio_status() & AUDIO_STATUS_PAUSE)
            {
                audio_resume(); 
            }
            waiting_to_resume_play = false;
        }
    }
}

void car_adapter_mode_init(void)
{
    tick_add_task(car_adapter_tick);
}
#endif

long default_event_handler_ex(long event, void (*callback)(void *), void *parameter)
{
    switch(event)
    {
        case SYS_USB_CONNECTED:
            if (callback != NULL)
                callback(parameter);
#ifdef HAVE_MMC
            if (!mmc_touched() || (mmc_remove_request() == SYS_MMC_EXTRACTED))
#endif
            {
                system_flush();
                usb_screen();
                system_restore();
            }
            return SYS_USB_CONNECTED;
        case SYS_POWEROFF:
            if (!clean_shutdown(callback, parameter))
                return SYS_POWEROFF;
            break;
#ifdef HAVE_CHARGING
        case SYS_CHARGER_CONNECTED:
            car_adapter_mode_processing(true);
            return SYS_CHARGER_CONNECTED;
            
        case SYS_CHARGER_DISCONNECTED:
            car_adapter_mode_processing(false);
            return SYS_CHARGER_DISCONNECTED;
#endif
    }
    return 0;
}

long default_event_handler(long event)
{
    return default_event_handler_ex(event, NULL, NULL);
}

int show_logo( void )
{
#ifdef HAVE_LCD_BITMAP
    char version[32];
    int font_h, font_w;

    lcd_clear_display();
#if LCD_WIDTH == 112 || LCD_WIDTH == 128
    lcd_bitmap(rockbox112x37, 0, 10, 112, 37);
#endif
#if LCD_WIDTH >= 160
    lcd_bitmap(rockbox160x53x2, 0, 10, 160, 53);
#endif

#ifdef HAVE_REMOTE_LCD
    lcd_remote_clear_display();
    lcd_remote_bitmap(rockbox112x37,10,14,112,37);
#endif

    snprintf(version, sizeof(version), "Ver. %s", appsversion);
    lcd_setfont(FONT_SYSFIXED);
    lcd_getstringsize("A", &font_w, &font_h);
    lcd_putsxy((LCD_WIDTH/2) - ((strlen(version)*font_w)/2),
               LCD_HEIGHT-font_h, version);
    lcd_update();

#ifdef HAVE_REMOTE_LCD
    lcd_remote_setfont(FONT_SYSFIXED);
    lcd_remote_getstringsize("A", &font_w, &font_h);
    lcd_remote_putsxy((LCD_REMOTE_WIDTH/2) - ((strlen(version)*font_w)/2),
               LCD_REMOTE_HEIGHT-font_h, version);
    lcd_remote_update();
#endif

#else
    char *rockbox = "  ROCKbox!";
    lcd_clear_display();
    lcd_double_height(true);
    lcd_puts(0, 0, rockbox);
    lcd_puts(0, 1, appsversion);
#endif

    return 0;
}
