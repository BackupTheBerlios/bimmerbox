/***************************************************************************
 *
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: search.c,v 1.1 2007/09/21 18:43:40 duke4d Exp $
 *
 * Copyright (C) 2003 Stefan Meyer
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "plugin.h"
#include "ctype.h"

static struct plugin_api* rb;

#define BUFFER_SIZE 16384

static int fd;
static int fdw;

static int file_size;
static int results = 0;

static char buffer[BUFFER_SIZE+1];
static char search_string[60] ;

static int buffer_pos; /* Position of the buffer in the file */

static int line_end;   /* Index of the end of line */

char resultfile[MAX_PATH];
char path[MAX_PATH];

static int strpcasecmp(const char *s1, const char *s2) 	 
{ 	 
    while (*s1 != '\0' && tolower(*s1) == tolower(*s2)) { 	 
        s1++; 	 
        s2++; 	 
    } 	 
    
    return (*s1 == '\0') ; 	 
}

static void fill_buffer(int pos)
{
    int numread;
    int i;
    int found = false ;
    const char crlf = '\n'; 

    if (pos>=file_size-BUFFER_SIZE)
        pos = file_size-BUFFER_SIZE;
    if (pos<0)
        pos = 0;

    rb->lseek(fd, pos, SEEK_SET);
    numread = rb->read(fd, buffer, BUFFER_SIZE);

    buffer[numread] = 0;
    line_end = 0;

    for(i=0;i<numread;i++) {
	switch(buffer[i]) {
	    case '\r':
		buffer[i] = ' ';
		break;
            case '\n':
		buffer[i] = 0;
		buffer_pos = pos + i +1 ;

		if (found)
		{
		    /* write to playlist */
		    rb->write(fdw, &buffer[line_end],
                              rb->strlen( &buffer[line_end] ));
		    rb->write(fdw, &crlf, 1);
		    
		    found = false ;
		    results++ ;
		}
		line_end = i +1 ;
                
		break;
                
            default:
	    	if (!found && tolower(buffer[i]) == tolower(search_string[0]))
		{
                    found = strpcasecmp(&search_string[0],&buffer[i]) ;
		}
		break;
	}
    }
    DEBUGF("\n-------------------\n");
}

static void search_buffer(void)
{
    buffer_pos = 0;
 
    fill_buffer(0);
    while ((buffer_pos+1) < file_size) {
	fill_buffer(buffer_pos);	
    }
}

static bool search_init(char* file)
{
    rb->memset(search_string, 0, sizeof(search_string));
    
    if (!rb->kbd_input(search_string,sizeof search_string))
    {
        rb->lcd_clear_display();
        rb->splash(0, true, "Searching...");	
        fd = rb->open(file, O_RDONLY);
        if (fd==-1)
            return false;

        fdw = rb->creat(resultfile,0);

        if (fdw < 0) {
#ifdef HAVE_LCD_BITMAP
            rb->splash(HZ, true, "Failed to create result file!");
#else
            rb->splash(HZ, true, "File creation failed");
#endif
            return false;
        }
   
        file_size = rb->lseek(fd, 0, SEEK_END);

        return true;
    }
    
    return false ;
}

/* this is the plugin entry point */
enum plugin_status plugin_start(struct plugin_api* api, void* parameter)
{
    int ok;
    char *filename = parameter;
    char *p;

    TEST_PLUGIN_API(api);

    rb = api;

    DEBUGF("%s - %s\n", parameter, &filename[rb->strlen(filename)-4]);
    /* Check the extension. We only allow .m3u files. */
    if(rb->strcasecmp(&filename[rb->strlen(filename)-4], ".m3u")) {
        rb->splash(HZ, true, "Not a .m3u file");
        return PLUGIN_ERROR;
    }

    rb->strcpy(path, filename);
    
    p = rb->strrchr(path, '/');
    if(p)
        *p = 0;

    rb->snprintf(resultfile, MAX_PATH, "%s/search_result.m3u", path, p+1);
    ok = search_init(parameter);
    if (!ok) {
    	return PLUGIN_ERROR;
    }
    search_buffer();

    rb->lcd_clear_display();
    rb->splash(HZ, true, "Done");
    rb->close(fdw);
    rb->close(fd);

    /* We fake a USB connection to force a reload of the file browser */
    return PLUGIN_USB_CONNECTED;
}
