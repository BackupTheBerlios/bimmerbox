/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: sprintf.h,v 1.1 2007/09/21 18:43:42 duke4d Exp $
 *
 * Copyright (C) 2002 by Felix Arends
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#ifndef __SPRINTF_H__
#define __SPRINTF_H__

#include <stddef.h>
#include <stdarg.h>
#include <_ansi.h>

int snprintf (char *buf, size_t size, const char *fmt, ...)
    ATTRIBUTE_PRINTF(3, 4);

int vsnprintf (char *buf, int size, const char *fmt, va_list ap);
int fdprintf (int fd, const char *fmt, ...)
    ATTRIBUTE_PRINTF(2, 3);

#endif /* __SPRINTF_H__ */
