/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: debug_menu.h,v 1.1 2007/09/21 18:43:36 duke4d Exp $
 *
 * Copyright (C) 2002 Heikki Hannikainen
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#ifndef _DEBUG_MENU_H
#define _DEBUG_MENU_H

bool debug_menu(void);

#ifndef SIMULATOR
extern bool dbg_ports(void);
#ifdef HAVE_RTC
extern bool dbg_rtc(void);
#endif
#endif
extern bool dbg_partitions(void);

#endif
