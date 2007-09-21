/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: filetree.h,v 1.1 2007/09/21 18:43:36 duke4d Exp $
 *
 * Copyright (C) 2005 by Bj�rn Stenberg
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#ifndef FILETREE_H
#define FILETREE_H
#include "tree.h"

int ft_load(struct tree_context* c, const char* tempdir);
int ft_play_filenumber(int pos, int attr);
int ft_play_dirname(int start_index);
void ft_play_filename(char *dir, char *file);
int ft_enter(struct tree_context* c);
int ft_exit(struct tree_context* c);
int ft_build_playlist(struct tree_context* c, int start_index);

#endif