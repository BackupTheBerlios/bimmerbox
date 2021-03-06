/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: logf.h,v 1.1 2007/09/21 18:43:42 duke4d Exp $
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
#ifndef LOGF_H
#define LOGF_H
#include <config.h>
#include <stdbool.h>

#ifdef ROCKBOX_HAS_LOGF

#define MAX_LOGF_LINES 1000
#define MAX_LOGF_ENTRY 30
#define MAX_LOGF_DATASIZE (MAX_LOGF_ENTRY*MAX_LOGF_LINES)

extern unsigned char logfbuffer[MAX_LOGF_LINES][MAX_LOGF_ENTRY];
extern int logfindex;
extern bool logfwrap;

void logf(const char *format, ...);
#else
/* built without logf() support enabled */
#define logf(...)
#endif

#endif /* LOGF_H */
