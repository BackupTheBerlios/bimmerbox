/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: replaygain.c,v 1.1 2007/09/21 18:43:41 duke4d Exp $
 *
 * Copyright (C) 2005 Magnus Holmgren
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system.h>
#include "id3.h"
#include "debug.h"

/* The fixed point math routines (with the exception of fp_atof) are based
 * on oMathFP by Dan Carter (http://orbisstudios.com).
 */

/* 12 bits of precision gives fairly accurate result, but still allows a
 * compact implementation. The math code supports up to 13...
 */

#define FP_BITS         (12)
#define FP_MASK         ((1 << FP_BITS) - 1)
#define FP_ONE          (1 << FP_BITS)
#define FP_TWO          (2 << FP_BITS)
#define FP_HALF         (1 << (FP_BITS - 1))
#define FP_LN2          ( 45426 >> (16 - FP_BITS))
#define FP_LN2_INV      ( 94548 >> (16 - FP_BITS))
#define FP_EXP_ZERO     ( 10922 >> (16 - FP_BITS))
#define FP_EXP_ONE      (  -182 >> (16 - FP_BITS))
#define FP_EXP_TWO      (     4 >> (16 - FP_BITS))
#define FP_INF          (0x7fffffff)
#define FP_LN10         (150902 >> (16 - FP_BITS))

#define FP_MAX_DIGITS       (4)
#define FP_MAX_DIGITS_INT   (10000)

#define FP_FAST_MUL_DIV

#ifdef FP_FAST_MUL_DIV

/* These macros can easily overflow, but they are good enough for our uses,
 * and saves some code.
 */
#define fp_mul(x, y) (((x) * (y)) >> FP_BITS)
#define fp_div(x, y) (((x) << FP_BITS) / (y))

#else

static long fp_mul(long x, long y)
{
    long x_neg = 0;
    long y_neg = 0;
    long rc;

    if ((x == 0) || (y == 0))
    {
        return 0;
    }

    if (x < 0)
    {
        x_neg = 1;
        x = -x;
    }

    if (y < 0)
    {
        y_neg = 1;
        y = -y;
    }

    rc = (((x >> FP_BITS) * (y >> FP_BITS)) << FP_BITS)
        + (((x & FP_MASK) * (y & FP_MASK)) >> FP_BITS)
        + ((x & FP_MASK) * (y >> FP_BITS))
        + ((x >> FP_BITS) * (y & FP_MASK));

    if ((x_neg ^ y_neg) == 1)
    {
        rc = -rc;
    }

    return rc;
}

static long fp_div(long x, long y)
{
    long x_neg = 0;
    long y_neg = 0;
    long shifty;
    long rc;
    int msb = 0;
    int lsb = 0;

    if (x == 0)
    {
        return 0;
    }

    if (y == 0)
    {
        return (x < 0) ? -FP_INF : FP_INF;
    }

    if (x < 0)
    {
        x_neg = 1;
        x = -x;
    }

    if (y < 0)
    {
        y_neg = 1;
        y = -y;
    }

    while ((x & (1 << (30 - msb))) == 0)
    {
        msb++;
    }

    while ((y & (1 << lsb)) == 0)
    {
        lsb++;
    }

    shifty = FP_BITS - (msb + lsb);
    rc = ((x << msb) / (y >> lsb));

    if (shifty > 0)
    {
        rc <<= shifty;
    }
    else
    {
        rc >>= -shifty;
    }

    if ((x_neg ^ y_neg) == 1)
    {
        rc = -rc;
    }

    return rc;
}

#endif /* FP_FAST_MUL_DIV */

static long fp_exp(long x)
{
    long k;
    long z;
    long R;
    long xp;

    if (x == 0)
    {
        return FP_ONE;
    }

    k = (fp_mul(abs(x), FP_LN2_INV) + FP_HALF) & ~FP_MASK;

    if (x < 0)
    {
        k = -k;
    }

    x -= fp_mul(k, FP_LN2);
    z = fp_mul(x, x);
    R = FP_TWO + fp_mul(z, FP_EXP_ZERO + fp_mul(z, FP_EXP_ONE
        + fp_mul(z, FP_EXP_TWO)));
    xp = FP_ONE + fp_div(fp_mul(FP_TWO, x), R - x);

    if (k < 0)
    {
        k = FP_ONE >> (-k >> FP_BITS);
    }
    else
    {
        k = FP_ONE << (k >> FP_BITS);
    }

    return fp_mul(k, xp);
}

static long fp_exp10(long x)
{
    if (x == 0)
    {
        return FP_ONE;
    }

    return fp_exp(fp_mul(FP_LN10, x));
}

static long fp_atof(const char* s, int precision)
{
    long int_part = 0;
    long int_one = 1 << precision;
    long frac_part = 0;
    long frac_count = 0;
    long frac_max = ((precision * 4) + 12) / 13;
    long frac_max_int = 1;
    long sign = 1;
    bool point = false;

    while ((*s != '\0') && isspace(*s))
    {
        s++;
    }

    if (*s == '-')
    {
        sign = -1;
        s++;
    }
    else if (*s == '+')
    {
        s++;
    }

    while (*s != '\0')
    {
        if (*s == '.')
        {
            if (point)
            {
                break;
            }

            point = true;
        }
        else if (isdigit(*s))
        {
            if (point)
            {
                if (frac_count < frac_max)
                {
                    frac_part = frac_part * 10 + (*s - '0');
                    frac_count++;
                    frac_max_int *= 10;
                }
            }
            else
            {
                int_part = int_part * 10 + (*s - '0');
            }
        }
        else
        {
            break;
        }

        s++;
    }

    while (frac_count < frac_max)
    {
      frac_part *= 10;
      frac_count++;
      frac_max_int *= 10;
    }

    return sign * ((int_part * int_one) 
        + (((int64_t) frac_part * int_one) / frac_max_int));
}

static long convert_gain(long gain)
{
    if (gain != 0)
    {
        /* Don't allow unreasonably low or high gain changes. 
         * Our math code can't handle it properly anyway. :)
         */
        if (gain < (-23 * FP_ONE))
        {
            gain = -23 * FP_ONE;
        }
    
        if (gain > (17 * FP_ONE))
        {
            gain = 17 * FP_ONE;
        }

        gain = fp_exp10(gain / 20) << (24 - FP_BITS);
    }
    
    return gain;
}

long get_replaygain_int(long int_gain)
{
    long gain = 0;
    
    if (int_gain)
    {
        gain = convert_gain(int_gain * FP_ONE / 100);
    }
    
    return gain;
}

long get_replaygain(const char* str)
{
    long gain = 0;
    
    if (str)
    {
        gain = fp_atof(str, FP_BITS);
        gain = convert_gain(gain);
    }
    
    return gain;
}

long get_replaypeak(const char* str)
{
    long peak = 0;
    
    if (str)
    {
        peak = fp_atof(str, 24);
    }
    
    return peak;
}

/* Compare two strings, ignoring case, up to the end nil or another end of
 * string character. E.g., if eos is '=', "a=" would equal "a". Returns 
 * true for a match, false otherwise.
 * TODO: This should be placed somewhere else, as it could be useful in 
 *       other places too.
 */
static bool str_equal(const char* s1, const char* s2, char eos)
{
    char c1 = 0;
    char c2 = 0;

    while (*s1 && *s2 && (*s1 != eos) && (*s2 != eos))
    {
        if ((c1 = toupper(*s1)) != (c2 = toupper(*s2)))
        {
            return false;
        }
        
        s1++;
        s2++;
    }
    
    if (c1 == eos)
    {
        c1 = '\0';
    }
    
    if (c2 == eos)
    {
        c2 = '\0';
    }

    return c1 == c2;
}

/* Check for a ReplayGain tag conforming to the "VorbisGain standard". If 
 * found, set the mp3entry accordingly. If value is NULL, key is expected 
 * to be on the "key=value" format, and the comparion/extraction is done 
 * accordingly. buffer is where to store the text contents of the gain tags;
 * up to length bytes (including end nil) can be written.
 * Returns number of bytes written to the tag text buffer, or zero if
 * no ReplayGain tag was found (or nothing was copied to the buffer for
 * other reasons).
 */
long parse_replaygain(const char* key, const char* value, 
    struct mp3entry* entry, char* buffer, int length)
{
    const char* val = value;
    char **p = NULL;
    char eos = '\0';
    
    if (!val)
    {
        if (!(val = strchr(key, '=')))
        {
            return 0;
        }
        
        val++;
        eos = '=';
    }

    if (str_equal(key, "replaygain_track_gain", eos) 
        || (str_equal(key, "rg_radio", eos) && !entry->track_gain))
    {
        entry->track_gain = get_replaygain(val);
        p = &(entry->track_gain_string);
    } 
    else if (str_equal(key, "replaygain_album_gain", eos)
        || (str_equal(key, "rg_audiophile", eos) && !entry->album_gain))
    {
        entry->album_gain = get_replaygain(val);
        p = &(entry->album_gain_string);
    }
    else if (str_equal(key, "replaygain_track_peak", eos) 
        || (str_equal(key, "rg_peak", eos) && !entry->track_peak))
    {
        entry->track_peak = get_replaypeak(val);
    } 
    else if (str_equal(key, "replaygain_album_peak", eos))
    {
        entry->album_peak = get_replaypeak(val);
    }

    if (p)
    {
        int len = strlen(val);
        
        len = MIN(len, length - 1);

        /* A few characters just isn't interesting... */
        if (len > 1)
        {
            strncpy(buffer, val, len);
            buffer[len] = 0;
            *p = buffer;
            return len + 1;
        }
    }

    return 0;
}
