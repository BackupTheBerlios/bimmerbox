/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: dac.h,v 1.1 2007/09/21 18:43:41 duke4d Exp $
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
#ifndef _DAC_H_
#define _DAC_H_

#include "config.h"

#ifdef HAVE_DAC3550A

/*
	DAC I2C	defs
*/
#define DAC_ADR         0x9a
#define	DAC_DEV_WRITE   (DAC_ADR | 0x00)

#define	DAC_REG_WRITE   0xc0

/* registers..*/
#define DAC_SR_REG      1
#define DAC_AVOL        2
#define DAC_GCFG        3

extern int  dac_volume(unsigned int left, unsigned int right, bool deemph);
extern void dac_enable(bool enable);
extern void dac_line_in(bool enable);
extern void dac_init(void);

#endif

#endif
