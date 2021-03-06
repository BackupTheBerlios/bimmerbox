/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: cpu.h,v 1.1 2007/09/21 18:43:41 duke4d Exp $
 *
 * Copyright (C) 2004 by Linus Nielsen Feltzing
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "config.h"

#if CONFIG_CPU == SH7034
#include "sh7034.h"
#endif
#if CONFIG_CPU == MCF5249
#include "mcf5249.h"
#endif
#if CONFIG_CPU == MCF5250
#include "mcf5250.h"
#endif
#if CONFIG_CPU == TCC730
#include "tcc730.h"
#endif
