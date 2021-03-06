/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: keyboard.c,v 1.1 2007/09/21 18:43:40 duke4d Exp $
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
#include "lcd.h"
#include "button.h"
#include "kernel.h"
#include "system.h"
#include "version.h"
#include "debug_menu.h"
#include "sprintf.h"
#include <string.h>
#include "font.h"
#include "screens.h"
#include "status.h"
#include "talk.h"
#include "settings.h"
#include "misc.h"

#define KEYBOARD_LINES 4
#define KEYBOARD_PAGES 3
#define KEYBOARD_MARGIN 3

#if (CONFIG_KEYPAD == IRIVER_H100_PAD) || \
    (CONFIG_KEYPAD == IRIVER_H300_PAD)
#define KBD_CURSOR_RIGHT (BUTTON_ON | BUTTON_RIGHT)
#define KBD_CURSOR_LEFT (BUTTON_ON | BUTTON_LEFT)
#define KBD_SELECT BUTTON_SELECT
#define KBD_PAGE_FLIP BUTTON_MODE
#define KBD_DONE_PRE BUTTON_ON
#define KBD_DONE (BUTTON_ON | BUTTON_REL)
#define KBD_ABORT BUTTON_OFF
#define KBD_BACKSPACE BUTTON_REC

#elif CONFIG_KEYPAD == RECORDER_PAD
#define KBD_CURSOR_RIGHT (BUTTON_ON | BUTTON_RIGHT)
#define KBD_CURSOR_LEFT (BUTTON_ON | BUTTON_LEFT)
#define KBD_SELECT BUTTON_PLAY
#define KBD_PAGE_FLIP BUTTON_F1
#define KBD_DONE BUTTON_F2
#define KBD_ABORT BUTTON_OFF
#define KBD_BACKSPACE BUTTON_F3

#elif CONFIG_KEYPAD == ONDIO_PAD /* restricted Ondio keypad */
#define KBD_MODES /* Ondio uses 2 modes, picker and line edit */
#define KBD_SELECT (BUTTON_MENU | BUTTON_REL) /* backspace in line edit */
#define KBD_SELECT_PRE BUTTON_MENU
#define KBD_DONE (BUTTON_MENU | BUTTON_REPEAT)
#define KBD_ABORT BUTTON_OFF

#elif CONFIG_KEYPAD == GMINI100_PAD
#define KBD_CURSOR_RIGHT (BUTTON_MENU | BUTTON_RIGHT)
#define KBD_CURSOR_LEFT (BUTTON_MENU | BUTTON_LEFT)
#define KBD_SELECT (BUTTON_PLAY | BUTTON_REL)
#define KBD_SELECT_PRE BUTTON_PLAY
#define KBD_PAGE_FLIP BUTTON_ON
#define KBD_DONE (BUTTON_PLAY | BUTTON_REPEAT)
#define KBD_ABORT BUTTON_OFF
#define KBD_BACKSPACE (BUTTON_MENU | BUTTON_PLAY)

#endif

static const char * const kbdpages[KEYBOARD_PAGES][KEYBOARD_LINES] = {
    {   "ABCDEFG !?\" @#$%+'", 
        "HIJKLMN 789 &_()-`",
        "OPQRSTU 456 �|{}/<",  
        "VWXYZ.,0123 ~=[]*>" },

    {   "abcdefg ���������",   
        "hijklmn ���������",
        "opqrstu ���������",   
        "vwxyz., ���      "  },

    {   "������� ���� ����",   
        "������� ���� ����",
        "������ ����� ����",
        "������ ����� ����"  },
};

/* helper function to spell a char if voice UI is enabled */
static void kbd_spellchar(char c)
{
    static char spell_char[2] = "\0\0"; /* store char to pass to talk_spell */

    if (global_settings.talk_menu) /* voice UI? */
    {
        spell_char[0] = c; 
        talk_spell(spell_char, false);
    }
}

int kbd_input(char* text, int buflen)
{
    bool done = false;
    int page = 0;

    int font_w = 0, font_h = 0, i;
    int x = 0, y = 0;
    int main_x, main_y, max_chars;
    int status_y1, status_y2;
    int len;
    int editpos, curpos, leftpos;
    bool redraw = true;
    const char * const *line;
#ifdef KBD_MODES
    bool line_edit = false;
#endif

    char outline[256];
    struct font* font = font_get(FONT_SYSFIXED);
    int button, lastbutton = 0;

    lcd_setfont(FONT_SYSFIXED);
    font_w = font->maxwidth;
    font_h = font->height;

    main_y = (KEYBOARD_LINES + 1) * font_h + (2*KEYBOARD_MARGIN);
    main_x = 0;
    status_y1 = LCD_HEIGHT - font_h;
    status_y2 = LCD_HEIGHT;

    editpos = strlen(text);

    max_chars = LCD_WIDTH / font_w - 2; /* leave room for < and > */
    line = kbdpages[0];

    if (global_settings.talk_menu) /* voice UI? */
        talk_spell(text, true); /* spell initial text */ 

    while(!done)
    {
        len = strlen(text);
            
        if(redraw)
        {
            lcd_clear_display();
            
            lcd_setfont(FONT_SYSFIXED);

            /* draw page */
            for (i=0; i < KEYBOARD_LINES; i++)
                lcd_putsxy(0, 8+i * font_h, line[i]);
            
            /* separator */
            lcd_hline(0, LCD_WIDTH - 1, main_y - KEYBOARD_MARGIN);
            
            /* write out the text */
            curpos = MIN(editpos, max_chars - MIN(len - editpos, 2));
            leftpos = editpos - curpos;
            strncpy(outline, text + leftpos, max_chars);
            outline[max_chars] = 0;
            
            lcd_putsxy(font_w, main_y, outline);

            if (leftpos)
                lcd_putsxy(0, main_y, "<");
            if (len - leftpos > max_chars)
                lcd_putsxy(LCD_WIDTH - font_w, main_y, ">");

            /* cursor */
            i = (curpos + 1) * font_w;
            lcd_vline(i, main_y, main_y + font_h);

#if CONFIG_KEYPAD == RECORDER_PAD
            /* draw the status bar */
            buttonbar_set("Shift", "OK", "Del");
            buttonbar_draw();
#endif
            
#ifdef KBD_MODES
            if (!line_edit)
#endif
            {
                /* highlight the key that has focus */
                lcd_set_drawmode(DRMODE_COMPLEMENT);
                lcd_fillrect(font_w * x, 8 + font_h * y, font_w, font_h);
                lcd_set_drawmode(DRMODE_SOLID);
            }

            status_draw(true);
        
            lcd_update();
        }

        /* The default action is to redraw */
        redraw = true;

        button = button_get_w_tmo(HZ/2);
        switch ( button ) {

            case KBD_ABORT:
                lcd_setfont(FONT_UI);
                return -1;
                break;

#ifdef KBD_PAGE_FLIP
            case KBD_PAGE_FLIP:    
                if (++page == KEYBOARD_PAGES)
                    page = 0;
                line = kbdpages[page];
                kbd_spellchar(line[y][x]);
                break;
#endif

            case BUTTON_RIGHT:
            case BUTTON_RIGHT | BUTTON_REPEAT:
#ifdef KBD_MODES
                if (line_edit) /* right doubles as cursor_right in line_edit */
                {
                    if (editpos < len)
                    {
                        editpos++;
                        kbd_spellchar(text[editpos]);
                    }
                }
                else
#endif
                {   
                    if (x < (int)strlen(line[y]) - 1)
                        x++;
                    else
                    {
                        x = 0;
#ifndef KBD_PAGE_FLIP   /* no dedicated flip key - flip page on wrap */
                        if (++page == KEYBOARD_PAGES)
                            page = 0;
                        line = kbdpages[page];
#endif
                    }
                    kbd_spellchar(line[y][x]);
                }
                break;

            case BUTTON_LEFT:
            case BUTTON_LEFT | BUTTON_REPEAT:
#ifdef KBD_MODES
                if (line_edit) /* left doubles as cursor_left in line_edit */
                {
                    if (editpos)
                    {
                        editpos--;
                        kbd_spellchar(text[editpos]);
                    }
                }
                else
#endif
                {   
                    if (x)
                        x--;
                    else
                    {
#ifndef KBD_PAGE_FLIP   /* no dedicated flip key - flip page on wrap */
                        if (--page < 0)
                            page = (KEYBOARD_PAGES-1);
                        line = kbdpages[page];
#endif
                        x = strlen(line[y]) - 1;
                    }
                    kbd_spellchar(line[y][x]);
                }
                break;

            case BUTTON_DOWN:
            case BUTTON_DOWN | BUTTON_REPEAT:
#ifdef KBD_MODES
                if (line_edit)
                {
                    y = 0;
                    line_edit = false;
                }
                else
                {
#endif
                    if (y < KEYBOARD_LINES - 1)
                        y++;
                    else
#ifndef KBD_MODES
                        y=0;
#else
                        line_edit = true;
                }
                if (!line_edit)
#endif
                    kbd_spellchar(line[y][x]);
                break;

            case BUTTON_UP:
            case BUTTON_UP | BUTTON_REPEAT:
#ifdef KBD_MODES
                if (line_edit)
                {
                    y = KEYBOARD_LINES - 1;
                    line_edit = false;
                }
                else
                {
#endif
                    if (y)
                        y--;
                    else
#ifndef KBD_MODES
                        y = KEYBOARD_LINES - 1;
#else
                        line_edit = true;
                }
                if (!line_edit)
#endif
                    kbd_spellchar(line[y][x]);
                break;

            case KBD_DONE:
                /* accepts what was entered and continues */
#ifdef KBD_DONE_PRE
                if (lastbutton != KBD_DONE_PRE)
                    break;
#endif
                done = true;
                break;

            case KBD_SELECT:
                /* inserts the selected char */
#ifdef KBD_SELECT_PRE
                if (lastbutton != KBD_SELECT_PRE)
                    break;
#endif
#ifdef KBD_MODES
                if (line_edit) /* select doubles as backspace in line_edit */
                {
                    if (editpos > 0)
                    {
                        for (i = editpos; i < len; i++)
                            text[i-1] = text[i];
                        text[i-1] = '\0';
                        editpos--;
                    }
                }
                else
#endif
                {
                    if (len + 1 < buflen)
                    {
                        for (i = len ; i > editpos; i--)
                            text[i] = text[i-1];
                        text[len+1] = 0;
                        text[editpos] = line[y][x];
                        editpos++;
                    }
                }
                if (global_settings.talk_menu) /* voice UI? */
                    talk_spell(text, false);   /* speak revised text */
                break;

#ifndef KBD_MODES
            case KBD_BACKSPACE:
            case KBD_BACKSPACE | BUTTON_REPEAT:
                if (editpos > 0)
                {
                    for (i = editpos; i < len; i++)
                        text[i-1] = text[i];
                    text[i-1] = '\0';
                    editpos--;
                }
                if (global_settings.talk_menu) /* voice UI? */
                    talk_spell(text, false);   /* speak revised text */
                break;

            case KBD_CURSOR_RIGHT:
            case KBD_CURSOR_RIGHT | BUTTON_REPEAT:
                if (editpos < len)
                {
                    editpos++;
                    kbd_spellchar(text[editpos]);
                }
                break;

            case KBD_CURSOR_LEFT:
            case KBD_CURSOR_LEFT | BUTTON_REPEAT:
                if (editpos)
                {
                    editpos--;
                    kbd_spellchar(text[editpos]);
                }
                break;
#endif /* !KBD_MODES */

            case BUTTON_NONE:
                status_draw(false);
                redraw = false;
                break;

            default:
                if(default_event_handler(button) == SYS_USB_CONNECTED)
                    lcd_setfont(FONT_SYSFIXED);
                break;

        }
        if (button != BUTTON_NONE)
            lastbutton = button;
    }
    lcd_setfont(FONT_UI);

    return 0;
}
