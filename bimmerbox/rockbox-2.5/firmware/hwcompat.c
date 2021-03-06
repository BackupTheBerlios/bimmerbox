/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: hwcompat.c,v 1.1 2007/09/21 18:43:41 duke4d Exp $
 *
 * Copyright (C) 2002 by Linus Nielsen Feltzing
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include <stdbool.h>
#include "config.h"
#include "debug.h"

int read_rom_version(void)
{
#ifdef GMINI_ARCH
    int ver = 0;
#else
    int ver = *(short *)0x020000fe;
#endif
    
    return ver;
}

int read_hw_mask(void)
{
#if defined(ARCHOS_PLAYER) || defined(GMINI_ARCH)
    int mask = 0; /* Fake value for simplicity */
#else
    int mask = *(short *)0x020000fc;
#endif
    
    return mask;
}

#ifdef ARCHOS_PLAYER
bool is_new_player(void)
{
    int ver = read_rom_version();

    return (ver > 449) || (ver == 116);
}           
#endif
