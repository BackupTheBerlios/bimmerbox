/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: vbrfix.c,v 1.1 2007/09/21 18:43:40 duke4d Exp $
 *
 * Copyright (C) 2004 Linus Nielsen Feltzing
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "plugin.h"

static struct plugin_api* rb;

static char *audiobuf;
static int audiobuflen;

static void xingupdate(int percent)
{
    char buf[32];

    rb->snprintf(buf, 32, "%d%%", percent);
    rb->lcd_puts(0, 1, buf);
#ifdef HAVE_LCD_BITMAP
    rb->lcd_update();
#endif
}

static int insert_data_in_file(char *fname, int fpos, char *buf, int num_bytes)
{
    int readlen;
    int rc;
    int orig_fd, fd;
    char tmpname[MAX_PATH];
    
    rb->snprintf(tmpname, MAX_PATH, "%s.tmp", fname);

    orig_fd = rb->open(fname, O_RDONLY);
    if(orig_fd < 0) {
        return 10*orig_fd - 1;
    }

    fd = rb->creat(tmpname, O_WRONLY);
    if(fd < 0) {
        rb->close(orig_fd);
        return 10*fd - 2;
    }

    /* First, copy the initial portion (the ID3 tag) */
    if(fpos) {
        readlen = rb->read(orig_fd, audiobuf, fpos);
        if(readlen < 0) {
            rb->close(fd);
            rb->close(orig_fd);
            return 10*readlen - 3;
        }
        
        rc = rb->write(fd, audiobuf, readlen);
        if(rc < 0) {
            rb->close(fd);
            rb->close(orig_fd);
            return 10*rc - 4;
        }
    }
    
    /* Now insert the data into the file */
    rc = rb->write(fd, buf, num_bytes);
    if(rc < 0) {
        rb->close(orig_fd);
        rb->close(fd);
        return 10*rc - 5;
    }

    /* Copy the file */
    do {
        readlen = rb->read(orig_fd, audiobuf, audiobuflen);
        if(readlen < 0) {
            rb->close(fd);
            rb->close(orig_fd);
            return 10*readlen - 7;
        }

        rc = rb->write(fd, audiobuf, readlen);
        if(rc < 0) {
            rb->close(fd);
            rb->close(orig_fd);
            return 10*rc - 8;
        }
    } while(readlen > 0);
    
    rb->close(fd);
    rb->close(orig_fd);

    /* Remove the old file */
    rc = rb->remove(fname);
    if(rc < 0) {
        return 10*rc - 9;
    }

    /* Replace the old file with the new */
    rc = rb->rename(tmpname, fname);
    if(rc < 0) {
        return 10*rc - 9;
    }
    
    return 0;
}

static void fileerror(int rc)
{
    rb->splash(HZ*2, true, "File error: %d", rc);
}

static const unsigned char empty_id3_header[] =
{
    'I', 'D', '3', 0x04, 0x00, 0x00,
    0x00, 0x00, 0x1f, 0x76 /* Size is 4096 minus 10 bytes for the header */
};

static bool vbr_fix(char *selected_file)
{
    unsigned char xingbuf[1500];
    struct mp3entry entry;
    int fd;
    int rc;
    int flen;
    int num_frames;
    int numbytes;
    int framelen;
    int unused_space;

    rb->lcd_clear_display();
    rb->lcd_puts_scroll(0, 0, selected_file);
#ifdef HAVE_LCD_BITMAP
    rb->lcd_update();
#endif

    xingupdate(0);

    rc = rb->mp3info(&entry, selected_file, false);
    if(rc < 0) {
        fileerror(rc);
        return true;
    }
    
    fd = rb->open(selected_file, O_RDWR);
    if(fd < 0) {
        fileerror(fd);
        return true;
    }

    flen = rb->lseek(fd, 0, SEEK_END);

    xingupdate(0);

    num_frames = rb->count_mp3_frames(fd, entry.first_frame_offset,
                                      flen, xingupdate);

    if(num_frames) {
        /* Note: We don't need to pass a template header because it will be
           taken from the mpeg stream */
        framelen = rb->create_xing_header(fd, entry.first_frame_offset,
                                          flen, xingbuf, num_frames, 0,
                                          0, xingupdate, true);
        
        /* Try to fit the Xing header first in the stream. Replace the existing
           VBR header if there is one, else see if there is room between the
           ID3 tag and the first MP3 frame. */
        if(entry.first_frame_offset - entry.id3v2len >=
           (unsigned int)framelen) {
            DEBUGF("Using existing space between ID3 and first frame\n");

            /* Seek to the beginning of the unused space */
            rc = rb->lseek(fd, entry.id3v2len, SEEK_SET);
            if(rc < 0) {
                rb->close(fd);
                fileerror(rc);
                return true;
            }

            unused_space =
                entry.first_frame_offset - entry.id3v2len - framelen;
            
            /* Fill the unused space with 0's (using the MP3 buffer)
               and write it to the file */
            if(unused_space)
            {
                rb->memset(audiobuf, 0, unused_space);
                rc = rb->write(fd, audiobuf, unused_space);
                if(rc < 0) {
                    rb->close(fd);
                    fileerror(rc);
                    return true;
                }
            }

            /* Then write the Xing header */
            rc = rb->write(fd, xingbuf, framelen);
            if(rc < 0) {
                rb->close(fd);
                fileerror(rc);
                return true;
            }
            
            rb->close(fd);
        } else {
            /* If not, insert some space. If there is an ID3 tag in the
               file we only insert just enough to squeeze the Xing header
               in. If not, we insert an additional empty ID3 tag of 4K. */
            
            rb->close(fd);
            
            /* Nasty trick alert! The insert_data_in_file() function
               uses the MP3 buffer when copying the data. We assume
               that the ID3 tag isn't longer than 1MB so the xing
               buffer won't be overwritten. */
            
            if(entry.first_frame_offset) {
                DEBUGF("Inserting %d bytes\n", framelen);
                numbytes = framelen;
            } else {
                DEBUGF("Inserting 4096+%d bytes\n", framelen);
                numbytes = 4096 + framelen;
                
                rb->memset(audiobuf + 0x100000, 0, numbytes);
                
                /* Insert the ID3 header */
                rb->memcpy(audiobuf + 0x100000, empty_id3_header,
                           sizeof(empty_id3_header));
            }
            
            /* Copy the Xing header */
            rb->memcpy(audiobuf + 0x100000 + numbytes - framelen,
                       xingbuf, framelen);
            
            rc = insert_data_in_file(selected_file,
                                     entry.first_frame_offset,
                                     audiobuf + 0x100000, numbytes);
            
            if(rc < 0) {
                fileerror(rc);
                return true;
            }
        }
        
        xingupdate(100);
    }
    else
    {
        /* Not a VBR file */
        DEBUGF("Not a VBR file\n");
        rb->splash(HZ*2, true, "Not a VBR file");
    }

    return false;
}

enum plugin_status plugin_start(struct plugin_api* api, void *parameter)
{
    TEST_PLUGIN_API(api);

    rb = api;

    if (!parameter)
        return PLUGIN_ERROR;

    audiobuf = rb->plugin_get_audio_buffer(&audiobuflen);
    
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(true);
#endif

    vbr_fix(parameter);

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(false);
#endif
    return PLUGIN_OK;
}
