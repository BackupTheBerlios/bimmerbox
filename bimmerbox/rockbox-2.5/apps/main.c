/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: main.c,v 1.2 2007/09/21 22:53:24 duke4d Exp $
 *
 * Copyright (C) 2002 by Bj�rn Stenberg
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "config.h"

#include "ata.h"
#include "disk.h"
#include "fat.h"
#include "lcd.h"
#include "rtc.h"
#include "debug.h"
#include "led.h"
#include "kernel.h"
#include "button.h"
#include "tree.h"
#include "panic.h"
#include "menu.h"
#include "system.h"
#include "usb.h"
#include "powermgmt.h"
#include "adc.h"
#include "i2c.h"
#ifndef DEBUG
#include "serial.h"
#endif
#include "audio.h"
#include "mp3_playback.h"
#include "thread.h"
#include "settings.h"
#include "backlight.h"
#include "status.h"
#include "debug_menu.h"
#include "version.h"
#include "sprintf.h"
#include "font.h"
#include "language.h"
#include "wps-display.h"
#include "playlist.h"
#include "buffer.h"
#include "rolo.h"
#include "screens.h"
#include "power.h"
#include "talk.h"
#include "plugin.h"
#include "misc.h"
#include "database.h"

#if (CONFIG_CODEC == SWCODEC)
#include "pcmbuf.h"
#else
#define pcmbuf_init()
#endif
#if defined(IRIVER_H100_SERIES) && !defined(SIMULATOR)
#include "pcm_record.h"
#endif

#ifdef CONFIG_TUNER
#include "radio.h"
#endif
#ifdef HAVE_MMC
#include "ata_mmc.h"
#endif

#ifdef HAVE_REMOTE_LCD
#include "lcd-remote.h"
#endif

/*#define AUTOROCK*/ /* define this to check for "autostart.rock" on boot */

const char appsversion[]=APPSVERSION;

void init(void);

void emu_main(void);

void app_main(void)
{
    init();
    emu_main();
    browse_root();
}

#ifdef SIMULATOR

void init(void)
{
    init_threads();
    buffer_init();
    lcd_init();
#ifdef HAVE_REMOTE_LCD
    lcd_remote_init();
#endif
    font_init();
    show_logo();
    lang_init();
    audio_init();
    settings_reset();
    settings_calc_config_sector();
    settings_load(SETTINGS_ALL);
    settings_apply();
    sleep(HZ/2);
    tree_init();
    playlist_init();
    mp3_init( global_settings.volume,
              global_settings.bass,
              global_settings.treble,
              global_settings.balance,
              global_settings.loudness,
              global_settings.avc,
              global_settings.channel_config,
              global_settings.stereo_width,
              global_settings.mdb_strength,
              global_settings.mdb_harmonics,
              global_settings.mdb_center,
              global_settings.mdb_shape,
              global_settings.mdb_enable,
              global_settings.superbass);
    button_clear_queue(); /* Empty the keyboard buffer */
#if CONFIG_CODEC == SWCODEC
    talk_init();
#endif
}

#else

void init(void)
{
    int rc;
    bool mounted = false;
#if defined(HAVE_CHARGING) && (CONFIG_CPU == SH7034)
    /* if nobody initialized ATA before, I consider this a cold start */
    bool coldstart = (PACR2 & 0x4000) != 0; /* starting from Flash */
#endif
    system_init();
    kernel_init();

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    set_cpu_frequency(CPUFREQ_NORMAL);
#endif
    
    buffer_init();

    settings_reset();
    
    lcd_init();
#ifdef HAVE_REMOTE_LCD
    lcd_remote_init();
#endif
    font_init();
    show_logo();
    lang_init();

    set_irq_level(0);
#ifdef DEBUG
    debug_init();
#else
#ifndef HAVE_MMC  /* FIXME: This is also necessary for debug builds
                   * (do debug builds on the Ondio make sense?) */
    serial_setup();
#endif
#endif

    i2c_init();

#ifdef HAVE_RTC
    rtc_init();
    settings_load(SETTINGS_RTC); /* early load parts of global_settings */
#endif

    adc_init();
    
    usb_init();
    
    backlight_init();

    button_init();

    powermgmt_init();

#ifdef CONFIG_TUNER
    radio_init();
#endif

#if defined(HAVE_CHARGING) && (CONFIG_CPU == SH7034)
    if (coldstart && charger_inserted()
        && !global_settings.car_adapter_mode
#ifdef ATA_POWER_PLAYERSTYLE
        && !ide_powered() /* relies on probing result from bootloader */
#endif
        )
    {
        rc = charging_screen(); /* display a "charging" screen */
        if (rc == 1)            /* charger removed */
            power_off();
        /* "On" pressed or USB connected: proceed */
        show_logo();  /* again, to provide better visual feedback */
    }
#endif

    rc = ata_init();
    if(rc)
    {
#ifdef HAVE_LCD_BITMAP
        char str[32];
        lcd_clear_display();
        snprintf(str, 31, "ATA error: %d", rc);
        lcd_puts(0, 1, str);
        lcd_puts(0, 3, "Press ON to debug");
        lcd_update();
        while(!(button_get(true) & BUTTON_REL));
        dbg_ports();
#endif
        panicf("ata: %d", rc);
    }

    usb_start_monitoring();
    while (usb_detect())
    {   /* enter USB mode early, before trying to mount */
        if (button_get_w_tmo(HZ/10) == SYS_USB_CONNECTED)
#ifdef HAVE_MMC
            if (!mmc_touched() || (mmc_remove_request() == SYS_MMC_EXTRACTED))
#endif
            {
                usb_screen();
                mounted = true; /* mounting done @ end of USB mode */
            }
#ifdef HAVE_USB_POWER
        if (usb_powered())      /* avoid deadlock */
            break;
#endif
    }

    if (!mounted)
    {
        rc = disk_mount_all();
        if (rc<=0)
        {
            lcd_clear_display();
            lcd_puts(0, 0, "No partition");
            lcd_puts(0, 1, "found.");
#ifdef HAVE_LCD_BITMAP
            lcd_puts(0, 2, "Insert USB cable");
            lcd_puts(0, 3, "and fix it.");
            lcd_update();
#endif
            while(button_get(true) != SYS_USB_CONNECTED) {};
            usb_screen();
            system_reboot();
        }
    }

    /* On software codec platforms we have to init audio before
       calling audio_set_buffer_margin(). */
#if (CONFIG_CODEC == SWCODEC)
    audio_init();
#endif
    settings_calc_config_sector();
    settings_load(SETTINGS_ALL);
    settings_apply();

    status_init();
    playlist_init();
    tree_init();
        
    /* No buffer allocation (see buffer.c) may take place after the call to
       audio_init() since the mpeg thread takes the rest of the buffer space */
    mp3_init( global_settings.volume,
              global_settings.bass,
              global_settings.treble,
              global_settings.balance,
              global_settings.loudness,
              global_settings.avc,
              global_settings.channel_config,
              global_settings.stereo_width,
              global_settings.mdb_strength,
              global_settings.mdb_harmonics,
              global_settings.mdb_center,
              global_settings.mdb_shape,
              global_settings.mdb_enable,
              global_settings.superbass);
#if (CONFIG_CODEC == SWCODEC)
    sound_settings_apply();
#else
    audio_init();
#endif
#if defined(IRIVER_H100_SERIES) && !defined(SIMULATOR)
    pcm_init_recording();
#endif
    talk_init();
    /* runtime database has to be initialized after audio_init() */
    rundb_init();


#ifdef AUTOROCK
    {
        int fd;
        static const char filename[] = PLUGIN_DIR "/autostart.rock"; 

        fd = open(filename, O_RDONLY);
        if(fd >= 0) /* no complaint if it doesn't exist */
        {
            close(fd);
            plugin_load((char*)filename, NULL); /* start if it does */
        }
    }
#endif /* #ifdef AUTOROCK */

#ifdef HAVE_CHARGING
    car_adapter_mode_init();
#endif
}

int main(void)
{
    app_main();

    while(1) {
#if CONFIG_LED == LED_REAL
        led(true); sleep(HZ/10);
        led(false); sleep(HZ/10);
#endif
    }
    return 0;
}
#endif

