/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: chessclock.c,v 1.1 2007/09/21 18:43:40 duke4d Exp $
 *
 * Copyright (C) 2002 Kjell Ericson
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "plugin.h"

/* variable button definitions */
#if CONFIG_KEYPAD == RECORDER_PAD
#define CHC_QUIT BUTTON_OFF
#define CHC_STARTSTOP BUTTON_PLAY
#define CHC_RESET BUTTON_LEFT
#define CHC_MENU BUTTON_F1
#define CHC_SETTINGS_INC BUTTON_UP
#define CHC_SETTINGS_DEC BUTTON_DOWN
#define CHC_SETTINGS_OK BUTTON_PLAY
#define CHC_SETTINGS_OK2 BUTTON_LEFT
#define CHC_SETTINGS_CANCEL BUTTON_OFF

#elif CONFIG_KEYPAD == ONDIO_PAD
#define CHC_QUIT BUTTON_OFF
#define CHC_STARTSTOP BUTTON_RIGHT
#define CHC_RESET BUTTON_LEFT
#define CHC_MENU BUTTON_MENU
#define CHC_SETTINGS_INC BUTTON_UP
#define CHC_SETTINGS_DEC BUTTON_DOWN
#define CHC_SETTINGS_OK BUTTON_RIGHT
#define CHC_SETTINGS_OK2 BUTTON_LEFT
#define CHC_SETTINGS_CANCEL BUTTON_MENU

#elif CONFIG_KEYPAD == PLAYER_PAD
#define CHC_QUIT BUTTON_ON
#define CHC_STARTSTOP BUTTON_PLAY
#define CHC_RESET BUTTON_STOP
#define CHC_MENU BUTTON_MENU
#define CHC_SETTINGS_INC BUTTON_RIGHT
#define CHC_SETTINGS_DEC BUTTON_LEFT
#define CHC_SETTINGS_OK BUTTON_PLAY
#define CHC_SETTINGS_CANCEL BUTTON_STOP
#define CHC_SETTINGS_CANCEL2 BUTTON_MENU

#elif (CONFIG_KEYPAD == IRIVER_H100_PAD) || \
      (CONFIG_KEYPAD == IRIVER_H300_PAD)
#define CHC_QUIT BUTTON_SELECT
#define CHC_STARTSTOP BUTTON_ON
#define CHC_RESET BUTTON_OFF
#define CHC_MENU BUTTON_REC
#define CHC_SETTINGS_INC BUTTON_RIGHT
#define CHC_SETTINGS_DEC BUTTON_LEFT
#define CHC_SETTINGS_OK BUTTON_ON
#define CHC_SETTINGS_CANCEL BUTTON_OFF
#define CHC_SETTINGS_CANCEL2 BUTTON_REC

#endif



/* leave first line blank on bitmap display, for pause icon */
#ifdef HAVE_LCD_BITMAP
#define FIRST_LINE 1
#else
#define FIRST_LINE 0
#endif

/* here is a global api struct pointer. while not strictly necessary,
   it's nice not to have to pass the api pointer in all function calls
   in the plugin */
static struct plugin_api* rb;
#define MAX_PLAYERS 10

static struct {
    int nr_timers;
    int total_time;
    int round_time;
} settings;

static struct {
    int total_time;
    int used_time;
    bool hidden;
} timer_holder[MAX_PLAYERS];

static int run_timer(int nr);
static int chessclock_set_int(char* string, 
                              int* variable,
                              int step,
                              int min,
                              int max,
                              int flags);
#define FLAGS_SET_INT_SECONDS 1

static char * show_time(int secs);
static int simple_menu(int nr, char **strarr);

static bool pause;

#define MAX_TIME 7200

/* this is the plugin entry point */
enum plugin_status plugin_start(struct plugin_api* api, void* parameter)
{
    int i;
    bool done;
    int nr;
    
    TEST_PLUGIN_API(api);

    (void)parameter;
    rb=api;

    rb->memset(&settings, 0, sizeof(settings));
    
    /* now go ahead and have fun! */
    rb->splash(HZ, true, "Chess Clock");

    rb->lcd_clear_display();
    i=0;
    while (i>=0) {
        int res;
        switch (i) {
            case 0:
                res=chessclock_set_int("Number of players",
                                       &settings.nr_timers, 1, 1,
                                       MAX_PLAYERS, 0);
                break;
            case 1:
                res=chessclock_set_int("Total time",
                                       &settings.total_time, 10, 0, MAX_TIME,
                                       FLAGS_SET_INT_SECONDS);
                settings.round_time=settings.total_time;
                break;
            case 2:
                res=chessclock_set_int("Max round time", &settings.round_time,
                                       10, 0, settings.round_time,
                                       FLAGS_SET_INT_SECONDS);
                break;
            default:
                i=-1; /* done */
                res=-2;
                break;
        }
        if (res==-1) {
            return PLUGIN_USB_CONNECTED;
        }
        if (res==0) {
            i--;
            if (i<0) {
                return PLUGIN_OK;
            }
        }
        if (res>0) {
            i++;
        }
    }
    for (i=0; i<settings.nr_timers; i++) {
        timer_holder[i].total_time=settings.total_time;
        timer_holder[i].used_time=0;
        timer_holder[i].hidden=false;
    }
    
    pause=true; /* We start paused */

    nr=0;
    do {
        int ret=0;
        done=true;
        for (i=0; done && i<settings.nr_timers; i++) {
            if (!timer_holder[i].hidden)
                done=false;
        }
        if (done) {
            return PLUGIN_OK;
        }
        if (!timer_holder[nr].hidden) {
            done=false;
            ret=run_timer(nr);
        }
        switch (ret) {
            case -1: /* exit */
                done=true;
                break;
            case 3:
                return PLUGIN_USB_CONNECTED;
            case 1:
                nr++;
                if (nr>=settings.nr_timers)
                    nr=0;
                break;
            case 2:
                nr--;
                if (nr<0)
                    nr=settings.nr_timers-1;
                break;
        }
    } while (!done);
    return PLUGIN_OK;
}

#ifdef HAVE_LCD_BITMAP
static void show_pause_mode(bool enabled)
{
    static const char pause_icon[] = {0x00,0x7f,0x7f,0x00,0x7f,0x7f,0x00};
    
    if (enabled)
        rb->lcd_mono_bitmap(pause_icon, 52, 0, 7, 8);
    else
    {
        rb->lcd_set_drawmode(DRMODE_SOLID|DRMODE_INVERSEVID);
        rb->lcd_fillrect(52, 0, 7, 8);
        rb->lcd_set_drawmode(DRMODE_SOLID);
    }
}
#endif

/*
  -1= exit
  1 = next player
  2 = prev player
  3 = usb connected
*/
static int run_timer(int nr)
{
    char buf[40];
    char player_info[13];
    long last_tick;
    bool done=false;
    int retval=0;
    long max_ticks=timer_holder[nr].total_time*HZ-timer_holder[nr].used_time;
    long ticks=0;
    bool round_time=false;

#ifdef HAVE_LCD_CHARCELLS
    rb->lcd_icon(ICON_PAUSE, pause);
#else
    show_pause_mode(pause);
#endif

    if (settings.round_time*HZ<max_ticks) {
        max_ticks=settings.round_time*HZ;
        round_time=true;
    }
    rb->snprintf(player_info, sizeof(player_info), "Player %d", nr+1);
    rb->lcd_puts(0, FIRST_LINE, player_info);
    last_tick=*rb->current_tick;

    while (!done) {
        int button;
        long now;
        if (ticks>max_ticks) {
            if (round_time) 
                rb->lcd_puts(0, FIRST_LINE+1, "ROUND UP!");
            else
                rb->lcd_puts(0, FIRST_LINE+1, "TIME OUT!");
            rb->backlight_on();
        } else {
            /*
            if (((int)(rb->current_tick - start_ticks)/HZ)&1) {
                rb->lcd_puts(0, FIRST_LINE, player_info);
            } else {
                rb->lcd_puts(0, FIRST_LINE, player_info);
            }
            */
            rb->lcd_puts(0, FIRST_LINE, player_info);
            now=*rb->current_tick;
            if (!pause) {
                ticks+=now-last_tick;
                if ((max_ticks-ticks)/HZ == 10) {
                     /* Backlight on if 10 seconds remain */
                    rb->backlight_on();
                }
            }
            last_tick=now;
            if (round_time) {
                rb->snprintf(buf, sizeof(buf), "%s/",
                             show_time((max_ticks-ticks+HZ-1)/HZ));
                /* Append total time */
                rb->strcpy(&buf[rb->strlen(buf)],
                           show_time((timer_holder[nr].total_time*HZ-
                                      timer_holder[nr].used_time-
                                      ticks+HZ-1)/HZ));
                rb->lcd_puts(0, FIRST_LINE+1, buf);
            } else {
                rb->lcd_puts(0, FIRST_LINE+1, show_time((max_ticks-ticks+HZ-1)/HZ));
            }
        }
#ifdef HAVE_LCD_BITMAP
        rb->lcd_update();
#endif
        button = rb->button_get(false);
        switch (button) {
            /* OFF/ON key to exit */
            case CHC_QUIT:
                return -1; /* Indicate exit */

            /* PLAY = Stop/Start toggle */
            case CHC_STARTSTOP:
                pause=!pause;
#ifdef HAVE_LCD_CHARCELLS
                rb->lcd_icon(ICON_PAUSE, pause);
#else
                show_pause_mode(pause);
#endif
                break;

            /* LEFT = Reset timer */
            case CHC_RESET:
                ticks=0;
                break;

                /* MENU  */
            case CHC_MENU:
            {
                int ret;
                char *menu[]={"Delete player", "Restart round",
                              "Set round time", "Set total time"};
                ret=simple_menu(4, menu);
                if (ret==-1) {
                    retval = 3;
                    done=true;
                } else if (ret==-2) {
                } else if (ret==0) {
                     /* delete timer */
                    timer_holder[nr].hidden=true;
                    retval=1;
                    done=true;
                    break;
                } else if (ret==1) {
                    /* restart */
                    ticks=0;
                    break;
                } else if (ret==2) {
                    /* set round time */
                    int res;
                    int val=(max_ticks-ticks)/HZ;
                    res=chessclock_set_int("Round time",
                                           &val,
                                           10, 0, MAX_TIME,
                                           FLAGS_SET_INT_SECONDS);
                    if (res==-1) { /*usb*/
                        retval = 3;
                        done=true;
                    } else if (res==1) {
                        ticks=max_ticks-val*HZ;
                    }
                } else if (ret==3) {
                    /* set total time */
                    int res;
                    int val=timer_holder[nr].total_time;
                    res=chessclock_set_int("Total time",
                                           &val,
                                           10, 0, MAX_TIME,
                                           FLAGS_SET_INT_SECONDS);
                    if (res==-1) { /*usb*/
                        retval = 3;
                        done=true;
                    } else if (res==1) {
                        timer_holder[nr].total_time=val;
                    }
                }
            }
            break;

            /* UP (RIGHT/+) = Scroll Lap timer up */
            case CHC_SETTINGS_INC:
                retval = 1;
                done = true;
                break;

            /* DOWN (LEFT/-) = Scroll Lap timer down */
            case CHC_SETTINGS_DEC:
                retval = 2;
                done = true;
                break;

            default:
                if (rb->default_event_handler(button) == SYS_USB_CONNECTED) {
                    retval = 3; /* been in usb mode */
                    done = true;
                }
                break;
        }
        rb->sleep(HZ/4); /* Sleep 1/4 of a second */
    }

    timer_holder[nr].used_time+=ticks;

    return retval;
}

static int chessclock_set_int(char* string, 
             int* variable,
             int step,
             int min,
             int max,
             int flags)
{
    bool done = false;
    int button;

    rb->lcd_clear_display();
    rb->lcd_puts_scroll(0, FIRST_LINE, string);

    while (!done) {
        char str[32];
        if (flags & FLAGS_SET_INT_SECONDS)
            rb->snprintf(str, sizeof str,"%s (m:s)", show_time(*variable));
        else
            rb->snprintf(str, sizeof str,"%d", *variable);
        rb->lcd_puts(0, FIRST_LINE+1, str);
#ifdef HAVE_LCD_BITMAP
        rb->lcd_update();
#endif
        button = rb->button_get(true);
        switch(button) {
            case CHC_SETTINGS_INC:
            case CHC_SETTINGS_INC | BUTTON_REPEAT:
                *variable += step;
                break;

            case CHC_SETTINGS_DEC:
            case CHC_SETTINGS_DEC | BUTTON_REPEAT:
                *variable -= step;
                break;

            case CHC_SETTINGS_OK:
#ifdef CHC_SETTINGS_OK2
            case CHC_SETTINGS_OK2:
#endif
                done = true;
                break;

            case CHC_SETTINGS_CANCEL:
#ifdef CHC_SETTINGS_CANCEL2
            case CHC_SETTINGS_CANCEL2:
#endif
                return 0; /* cancel */
                break;

            default:
                if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                    return -1; /* been in usb mode */
                break;

        }
        if(*variable > max )
            *variable = max;

        if(*variable < min )
            *variable = min;

    }
    rb->lcd_stop_scroll();

    return 1;
}

static char * show_time(int seconds)
{
    static char buf[]="00:00";
    rb->snprintf(buf, sizeof(buf), "%02d:%02d", seconds/60, seconds%60);
    return buf;
}

/* -1 = USB
   -2 = cancel
*/
static int simple_menu(int nr, char **strarr)
{
    int show=0;
    int button;
    rb->lcd_clear_display();

    while (1) {
        if (show>=nr)
            show=0;
        if (show<0)
            show=nr-1;
        rb->lcd_puts_scroll(0, FIRST_LINE, strarr[show]);
#ifdef HAVE_LCD_BITMAP
        rb->lcd_update();
#endif
        button = rb->button_get(true);
        switch(button) {
            case CHC_SETTINGS_INC:
            case CHC_SETTINGS_INC | BUTTON_REPEAT:
                show++;
                break;

            case CHC_SETTINGS_DEC:
            case CHC_SETTINGS_DEC | BUTTON_REPEAT:
                show--;
                break;

            case CHC_SETTINGS_OK:
#ifdef CHC_SETTINGS_OK2
            case CHC_SETTINGS_OK2:
#endif
                return show;
                break;

            case CHC_SETTINGS_CANCEL:
#ifdef CHC_SETTINGS_CANCEL2
            case CHC_SETTINGS_CANCEL2:
#endif
                return -2; /* cancel */
                break;

            default:
                if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                    return -1; /* been in usb mode */
                break;
        }
    }
}

