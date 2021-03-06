/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: highscore.c,v 1.1 2007/09/21 18:43:40 duke4d Exp $
 *
 * Copyright (C) 2005 Linus Nielsen Feltzing
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "plugin.h"
#include "highscore.h"

static struct plugin_api *rb;

void highscore_init(struct plugin_api* newrb)
{
    rb = newrb;
}

int highscore_save(char *filename, struct highscore *scores, int num_scores)
{
    int i;
    int fd;
    int rc;
    char buf[80];

    fd = rb->open(filename, O_WRONLY|O_CREAT);
    if(fd < 0)
        return -1;
    
    for(i = 0;i < num_scores;i++)
    {
        rb->snprintf(buf, sizeof(buf)-1, "%s:%d:%d\n",
                     scores[i].name, scores[i].score, scores[i].level);
        rc = rb->write(fd, buf, rb->strlen(buf));
        if(rc < 0)
        {
            rb->close(fd);
            return -2;
        }
    }
    rb->close(fd);
    return 0;
}

int highscore_load(char *filename, struct highscore *scores, int num_scores)
{
    int i;
    int fd;
    char buf[80];
    char *name, *score, *level;
    char *ptr;

    fd = rb->open(filename, O_RDONLY);
    if(fd < 0)
        return -1;

    rb->memset(scores, 0, sizeof(struct highscore)*num_scores);

    i = -1;
    while(rb->read_line(fd, buf, sizeof(buf)-1) && i < num_scores)
    {
        i++;
        
        DEBUGF("%s\n", buf);
        name = buf;
        ptr = rb->strchr(buf, ':');
        if ( !ptr )
            continue;
        *ptr = 0;
        ptr++;
        
        rb->strncpy(scores[i].name, name, sizeof(scores[i].name));
        
        DEBUGF("%s\n", scores[i].name);
        score = ptr;
        
        ptr = rb->strchr(ptr, ':');
        if ( !ptr )
            continue;
        *ptr = 0;
        ptr++;
        
        scores[i].score = rb->atoi(score);
        
        level = ptr;
        scores[i].level = rb->atoi(level);
    }    
    return 0;
}
