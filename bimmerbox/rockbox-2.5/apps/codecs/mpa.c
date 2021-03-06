/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: mpa.c,v 1.1 2007/09/21 18:43:37 duke4d Exp $
 *
 * Copyright (C) 2005 Dave Chapman
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "codec.h"

#include <codecs/libmad/mad.h>

#include "playback.h"
#include "dsp.h"
#include "lib/codeclib.h"
#include "system.h"
#include <inttypes.h>

struct mad_stream Stream IDATA_ATTR;
struct mad_frame Frame IDATA_ATTR;
struct mad_synth Synth IDATA_ATTR;
mad_timer_t Timer;

/* The following function is used inside libmad - let's hope it's never
   called. 
*/

void abort(void) {
}


#define INPUT_CHUNK_SIZE   8192

mad_fixed_t mad_frame_overlap[2][32][18] IDATA_ATTR;
unsigned char mad_main_data[MAD_BUFFER_MDLEN] IDATA_ATTR;
/* TODO: what latency does layer 1 have? */
int mpeg_latency[3] = { 0, 481, 529 };
#ifdef USE_IRAM
extern char iramcopy[];
extern char iramstart[];
extern char iramend[];
#endif

struct codec_api *ci;
int64_t samplecount;
int64_t samplesdone;
int stop_skip, start_skip;
int current_stereo_mode = -1;
unsigned int current_frequency = 0;

void recalc_samplecount(void)
{
    /* NOTE: currently this doesn't work, the below calculated samples_count
       seems to be right, but sometimes we just don't have all the data we
       need... */
    if (ci->id3->frame_count) {
        /* TODO: 1152 is the frame size in samples for MPEG1 layer 2 and layer 3,
           it's probably not correct at all for MPEG2 and layer 1 */
        samplecount = ((int64_t) ci->id3->frame_count) * 1152;
    } else {
        samplecount = ((int64_t) ci->id3->length) * current_frequency / 1000;
    }
    
    samplecount -= start_skip + stop_skip;
}

/* this is the codec entry point */
enum codec_status codec_start(struct codec_api* api)
{
    int Status = 0;
    size_t size;
    int file_end;
    int frame_skip;
    char *InputBuffer;

    ci = api;
    
    /* Generic codec inititialisation */
    TEST_CODEC_API(api);

#ifdef USE_IRAM
    ci->memcpy(iramstart, iramcopy, iramend - iramstart);
#endif

    /* This function sets up the buffers and reads the file into RAM */
  
    if (codec_init(api)) {
        return CODEC_ERROR;
    }

    /* Create a decoder instance */

    ci->configure(CODEC_SET_FILEBUF_LIMIT, (int *)(1024*1024*2));
    ci->configure(CODEC_SET_FILEBUF_CHUNKSIZE, (int *)(1024*16));
    ci->configure(DSP_SET_CLIP_MIN, (int *)-MAD_F_ONE);
    ci->configure(DSP_SET_CLIP_MAX, (int *)(MAD_F_ONE - 1));
    ci->configure(DSP_SET_SAMPLE_DEPTH, (int *)(MAD_F_FRACBITS));
    ci->configure(DSP_DITHER, (bool *)false);
    ci->configure(CODEC_DSP_ENABLE, (bool *)true);
    
    ci->memset(&Stream, 0, sizeof(struct mad_stream));
    ci->memset(&Frame, 0, sizeof(struct mad_frame));
    ci->memset(&Synth, 0, sizeof(struct mad_synth));
    ci->memset(&Timer, 0, sizeof(mad_timer_t));
    
    mad_stream_init(&Stream);
    mad_frame_init(&Frame);
    mad_synth_init(&Synth);
    mad_timer_reset(&Timer);

    /* We do this so libmad doesn't try to call codec_calloc() */
    memset(mad_frame_overlap, 0, sizeof(mad_frame_overlap));
    Frame.overlap = &mad_frame_overlap;
    Stream.main_data = &mad_main_data;
    /* This label might need to be moved above all the init code, but I don't
       think reiniting the codec is necessary for MPEG. It might even be unwanted
       for gapless playback */
  next_track:
  
    file_end = 0;
    
    while (!*ci->taginfo_ready && !ci->stop_codec)
        ci->sleep(1);
  
    ci->configure(DSP_SET_FREQUENCY, (int *)ci->id3->frequency);
    current_frequency = ci->id3->frequency;
    codec_set_replaygain(ci->id3);
    
    ci->request_buffer(&size, ci->id3->first_frame_offset);
    ci->advance_buffer(size);

    if (ci->id3->lead_trim >= 0 && ci->id3->tail_trim >= 0) {
        stop_skip = ci->id3->tail_trim - mpeg_latency[ci->id3->layer];
        if (stop_skip < 0) stop_skip = 0;
        start_skip = ci->id3->lead_trim + mpeg_latency[ci->id3->layer];
    } else {
        stop_skip = 0;
        /* We want to skip this amount anyway */
        start_skip = mpeg_latency[ci->id3->layer];
    }

    samplesdone = ((int64_t) ci->id3->elapsed) * current_frequency / 1000;
    frame_skip = start_skip;
    recalc_samplecount();
    
    /* This is the decoding loop. */
    while (1) {
        int framelength;

        ci->yield();
        if (ci->stop_codec || ci->reload_codec) {
            break ;
        }
    
        if (ci->seek_time) {
            int newpos;
        
            samplesdone = ((int64_t) (ci->seek_time - 1)) 
                * current_frequency / 1000;
            newpos = ci->mp3_get_filepos(ci->seek_time-1) +
                ci->id3->first_frame_offset;

            if (!ci->seek_buffer(newpos)) {
                goto next_track;
            }
            ci->seek_time = 0;
            if (newpos == 0)
                frame_skip = start_skip;
            /* Optional but good thing to do. */
            ci->seek_complete();
        }

        /* Lock buffers */
        if (Stream.error == 0) {
            InputBuffer = ci->request_buffer(&size, INPUT_CHUNK_SIZE);
            if (size == 0 || InputBuffer == NULL)
                break ;
            mad_stream_buffer(&Stream, InputBuffer, size);
        }
    
        if(mad_frame_decode(&Frame,&Stream))
        {
            if (Stream.error == MAD_FLAG_INCOMPLETE || Stream.error == MAD_ERROR_BUFLEN) {
                // ci->splash(HZ*1, true, "Incomplete");
                /* This makes the codec to support partially corrupted files too. */
                if (file_end == 30)
                    break ;
        
                /* Fill the buffer */
                Stream.error = 0;
                file_end++;
                continue ;
            }
            else if(MAD_RECOVERABLE(Stream.error))
            {
                if(Stream.error!=MAD_ERROR_LOSTSYNC)
                {
                    // rb->splash(HZ*1, true, "Recoverable...!");
                }
                continue;
            }
            else if(Stream.error==MAD_ERROR_BUFLEN) {
                //rb->splash(HZ*1, true, "Buflen error");
                break ;
            } else {
                //rb->splash(HZ*1, true, "Unrecoverable error");
                Status=1;
                break;
            }
            break ;
        }
        
        file_end = false;
        /* ?? Do we need the timer module? */
        // mad_timer_add(&Timer,Frame.header.duration);

        mad_synth_frame(&Synth,&Frame);
        framelength = Synth.pcm.length - frame_skip;
    
        /* Convert MAD's numbers to an array of 16-bit LE signed integers */
        /* We skip frame_skip number of samples here, this should only happen for
           very first frame in the stream. */
        /* TODO: possible for frame_skip to exceed one frames worth of samples? */

        if(Frame.header.samplerate != current_frequency) {
            current_frequency = Frame.header.samplerate;
            ci->configure(DSP_SWITCH_FREQUENCY,
                          (int *)current_frequency);
            recalc_samplecount();
        }
        
        if (stop_skip > 0)
        {
            int64_t max = samplecount - samplesdone;
            
            if (max < 0) max = 0;
            if (max < framelength) framelength = (int) max;
            if (framelength == 0) break;
        }

        if (MAD_NCHANNELS(&Frame.header) == 2) {
            if (current_stereo_mode != STEREO_NONINTERLEAVED) {
                ci->configure(DSP_SET_STEREO_MODE, (int *)STEREO_NONINTERLEAVED);
                current_stereo_mode = STEREO_NONINTERLEAVED;
            }
            ci->pcmbuf_insert_split(&Synth.pcm.samples[0][frame_skip],
                                    &Synth.pcm.samples[1][frame_skip],
                                    framelength * 4);
        } else {
            if (current_stereo_mode != STEREO_MONO) {
                ci->configure(DSP_SET_STEREO_MODE, (int *)STEREO_MONO);
                current_stereo_mode = STEREO_MONO;
            }
            ci->pcmbuf_insert((char *)&Synth.pcm.samples[0][frame_skip],
                              framelength * 4);
        }
        
        frame_skip = 0;
        
        if (Stream.next_frame)
            ci->advance_buffer_loc((void *)Stream.next_frame);
        else
            ci->advance_buffer(size);

        samplesdone += framelength;
        ci->set_elapsed(samplesdone / (current_frequency / 1000));
    }
  
    Stream.error = 0;
  
    if (ci->request_next_track())
        goto next_track;
    return CODEC_OK;
}
