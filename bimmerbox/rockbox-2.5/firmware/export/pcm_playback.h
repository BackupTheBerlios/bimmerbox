/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: pcm_playback.h,v 1.1 2007/09/21 18:43:42 duke4d Exp $
 *
 * Copyright (C) 2005 by Linus Nielsen Feltzing
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#ifndef PCM_PLAYBACK_H
#define PCM_PLAYBACK_H

void pcm_init(void);
void pcm_set_frequency(unsigned int frequency);

/* This is for playing "raw" PCM data */
void pcm_play_data(void (*get_more)(unsigned char** start, long* size));

void pcm_calculate_peaks(int *left, int *right);
long pcm_get_bytes_waiting(void);

void pcm_play_stop(void);
void pcm_play_pause(bool play);
bool pcm_is_paused(void);
bool pcm_is_playing(void);

#endif
