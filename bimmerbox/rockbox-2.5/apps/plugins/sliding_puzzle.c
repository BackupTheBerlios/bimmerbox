/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: sliding_puzzle.c,v 1.1 2007/09/21 18:43:40 duke4d Exp $
 *
 * Copyright (C) 2002 Vicentini Martin
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "plugin.h"
#ifdef HAVE_LCD_BITMAP

/* variable button definitions */
#if CONFIG_KEYPAD == RECORDER_PAD
#define PUZZLE_QUIT BUTTON_OFF
#define PUZZLE_SHUFFLE BUTTON_F1
#define PUZZLE_PICTURE BUTTON_F2

#elif CONFIG_KEYPAD == ONDIO_PAD
#define PUZZLE_QUIT BUTTON_OFF
#define PUZZLE_SHUFFLE_PICTURE_PRE BUTTON_MENU
#define PUZZLE_SHUFFLE (BUTTON_MENU | BUTTON_REPEAT)
#define PUZZLE_PICTURE (BUTTON_MENU | BUTTON_REL)

#elif (CONFIG_KEYPAD == IRIVER_H100_PAD) || \
      (CONFIG_KEYPAD == IRIVER_H300_PAD)
#define PUZZLE_QUIT BUTTON_OFF
#define PUZZLE_SHUFFLE BUTTON_SELECT
#define PUZZLE_PICTURE BUTTON_ON
#endif

static struct plugin_api* rb;
static int spots[20];
static int hole = 19, moves;
static char s[5];
static bool pic = true;
static unsigned char picture[20][32] = {
    { 0x00, 0x00, 0x00, 0x00, 0x18, 0x38, 0xf8, 0xd9,
      0x10, 0xb0, 0x60, 0xc0, 0x80, 0x00, 0x30, 0x78,
      0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x01, 0x07,
      0xbf, 0xf8, 0x43, 0x1c, 0x61, 0x5e, 0xfc, 0xfc },
    
    { 0x68, 0xc8, 0x48, 0x08, 0x98, 0x90, 0xb0, 0xa4,
      0xa0, 0xc0, 0xc0, 0x88, 0x14, 0x08, 0x00, 0x00,
      0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x03,
      0x00, 0x00, 0x30, 0x48, 0x79, 0x33, 0x06, 0x1c },
    
    { 0x20, 0x00, 0x06, 0x09, 0x09, 0x06, 0x00, 0x80,
      0x40, 0x80, 0x00, 0x08, 0x14, 0x08, 0x20, 0x00,
      0x60, 0x30, 0x18, 0xf9, 0x70, 0x00, 0x00, 0x00,
      0xf9, 0x18, 0x30, 0x60, 0x30, 0x18, 0x06, 0x32 },
    
    { 0x00, 0x80, 0x42, 0xa0, 0x50, 0x90, 0x88, 0x88,
      0x84, 0xa4, 0xa4, 0x64, 0x24, 0x18, 0x00, 0x40,
      0x79, 0x4a, 0x31, 0x02, 0x05, 0x2a, 0xd5, 0xaa,
      0x55, 0xaa, 0x55, 0xab, 0x56, 0xac, 0x58, 0xb0 },
    
    { 0x04, 0x0a, 0x04, 0x00, 0x80, 0x80, 0xc0, 0xc8,
      0x40, 0xc2, 0xc0, 0xc0, 0x00, 0x00, 0x00, 0x00,
      0x60, 0x38, 0x9c, 0xe7, 0x59, 0x0c, 0xc4, 0xfc,
      0x3f, 0x07, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00 },
    
    { 0x00, 0x04, 0x00, 0x00, 0x80, 0xe0, 0x78, 0x1e,
      0xa7, 0xd9, 0xcc, 0x76, 0x3b, 0x0d, 0x1f, 0xff,
      0x00, 0x00, 0x00, 0x00, 0x03, 0x13, 0x03, 0x03,
      0x11, 0x29, 0x10, 0x00, 0x04, 0x2a, 0x0b, 0x0b },
    
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0,
      0xc0, 0x8e, 0x10, 0x00, 0x80, 0x80, 0x60, 0x38,
      0x0a, 0x08, 0x05, 0x05, 0x07, 0x03, 0x03, 0x03,
      0x03, 0x03, 0x01, 0x11, 0x01, 0x00, 0x00, 0x80 },
    
    { 0x0e, 0x18, 0x31, 0x3e, 0x1c, 0x00, 0x00, 0x1c,
      0x3e, 0x31, 0x18, 0x0c, 0x0c, 0x18, 0x30, 0x20,
      0x00, 0x00, 0x08, 0x14, 0x08, 0x00, 0x02, 0x80,
      0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00 },
    
    { 0x60, 0xc0, 0x48, 0xc7, 0x60, 0xb8, 0x57, 0xaa,
      0x55, 0xaa, 0x55, 0xaa, 0xd5, 0xaa, 0x75, 0x3a,
      0x00, 0x00, 0x01, 0x23, 0x05, 0x05, 0x09, 0x0a,
      0x0b, 0x0a, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00 },
    
    { 0xe5, 0x8d, 0x18, 0x30, 0x41, 0xc1, 0x8f, 0xfe,
      0xf0, 0x81, 0x0a, 0x01, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x01, 0x03, 0x02, 0x06, 0x06, 0x04, 0x04,
      0x04, 0x05, 0x07, 0x06, 0x00, 0x00, 0x00, 0x00 },
    
    { 0x00, 0x00, 0x00, 0x10, 0x28, 0x10, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
      0xc0, 0xb0, 0x18, 0xcc, 0x24, 0x86, 0x42, 0x22,
      0x31, 0x69, 0xd1, 0xa9, 0x51, 0xa1, 0x41, 0x02 },
    
    { 0x00, 0x00, 0x00, 0x00, 0x40, 0xa1, 0x40, 0x00,
      0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x12, 0x16, 0x5c, 0x58, 0xf0, 0xc0, 0x00, 0x00,
      0x06, 0x09, 0x09, 0x86, 0x60, 0x18, 0xc4, 0xf2 },
    
    { 0x00, 0x00, 0x00, 0x80, 0x80, 0x40, 0x40, 0x40,
      0x40, 0x40, 0x40, 0x40, 0xc0, 0x80, 0x8c, 0x12,
      0x6a, 0xa5, 0x95, 0xd2, 0xca, 0xc9, 0xe9, 0xe9,
      0xe1, 0xe9, 0xe1, 0xe1, 0x62, 0x62, 0xc5, 0xc5 },
    
    { 0x12, 0x0c, 0x00, 0x80, 0x80, 0x40, 0x40, 0x40,
      0x40, 0x40, 0x40, 0x82, 0x85, 0x02, 0x00, 0x00,
      0x8a, 0x32, 0x6f, 0xd6, 0xaa, 0x55, 0x83, 0x01,
      0x81, 0x01, 0x81, 0x82, 0x82, 0x0d, 0xbe, 0xdc },
    
    { 0x00, 0x00, 0x00, 0x82, 0x00, 0x00, 0x00, 0x00,
      0x41, 0x00, 0x00, 0x00, 0x02, 0x05, 0x02, 0x00,
      0x00, 0x00, 0x01, 0x02, 0x01, 0x04, 0x00, 0x60,
      0x90, 0x90, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00 },
    
    { 0x3f, 0x6a, 0xc0, 0x9f, 0x20, 0x27, 0x48, 0x4b,
      0x47, 0x22, 0x50, 0x0f, 0x95, 0x4a, 0x25, 0x90,
      0x07, 0x0f, 0x17, 0x23, 0x43, 0x42, 0x44, 0x40,
      0x44, 0x40, 0x42, 0x40, 0x60, 0x30, 0x3c, 0x1f },
    
    { 0xd0, 0x50, 0x11, 0xb9, 0xef, 0x07, 0x80, 0x40,
      0x30, 0x98, 0x9c, 0xbf, 0x60, 0xc7, 0x0d, 0x36,
      0x59, 0x11, 0x21, 0x23, 0x22, 0x21, 0x30, 0x10,
      0x0d, 0x42, 0x01, 0x80, 0x00, 0x40, 0x03, 0x0c },
    
    { 0xc3, 0x81, 0x81, 0x00, 0x00, 0x01, 0x03, 0x03,
      0x03, 0x03, 0x02, 0x01, 0x00, 0x00, 0x80, 0x80,
      0x10, 0x91, 0x23, 0x23, 0x67, 0xe6, 0xe6, 0xa6,
      0xa6, 0xa6, 0xa6, 0xa6, 0xb3, 0x93, 0x59, 0x49 },
    
    { 0xc1, 0x71, 0xff, 0x54, 0x0a, 0x02, 0x06, 0x0e,
      0x0e, 0x0a, 0x06, 0x02, 0x02, 0xc5, 0x7b, 0x17,
      0x24, 0x10, 0x3b, 0x7f, 0x92, 0xa6, 0xa4, 0xa4,
      0xa4, 0xa4, 0xa4, 0x66, 0x23, 0x11, 0x12, 0x0c },
    
    { 0x55, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0xaa,
      0x55, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80,
      0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xaa }
};

/* draws a spot at the coordinates (x,y), range of p is 1-20 */
static void draw_spot(int p, int x, int y)
{
    if (pic || p==20) {
        rb->lcd_mono_bitmap (picture[p-1], x, y, 16, 16);
    } else {
        rb->lcd_drawrect(x, y, 16, 16);
        rb->lcd_set_drawmode(DRMODE_SOLID|DRMODE_INVERSEVID);
        rb->lcd_fillrect(x+1, y+1, 14, 14);
        rb->lcd_set_drawmode(DRMODE_SOLID);
        rb->snprintf(s, sizeof(s), "%d", p);
        rb->lcd_putsxy(x+2, y+4, s);
    }
}
    
/* check if the puzzle is solved */
static bool puzzle_finished(void)
{
    int i;
    for (i=0; i<20; i++)
    if (spots[i] != (i+1))
        return false;
    return true;
}
    
/* move a piece in any direction */
static void move_spot(int x, int y)
{
    int i;
    spots[hole] = spots[hole-x-5*y];
    hole -= (x+5*y);
    moves++;
    rb->snprintf(s, sizeof(s), "%d", moves);
    rb->lcd_putsxy(85, 20, s);

    for (i=4; i<=16; i+=4) {
        draw_spot(20, (hole%5)*16, (hole/5)*16);
        draw_spot(spots[hole], (hole%5)*16 + x*i, (hole/5)*16 + y*i);
        rb->lcd_update();
    }
    spots[hole] = 20;
}
    
/* initializes the puzzle */
static void puzzle_init(void)
{
    int i, r, temp, tsp[20];
    moves = 0;
    rb->lcd_clear_display();
    rb->lcd_drawrect(80, 0, 32, 64);
    rb->lcd_putsxy(81, 10, "Moves");
    rb->snprintf(s, sizeof(s), "%d", moves);
    rb->lcd_putsxy(85, 20, s);
    
    /* shuffle spots */
    for (i=19; i>=0; i--) {
        r = (rb->rand() % (i+1));
    
        temp = spots[r];
        spots[r] = spots[i];
        spots[i] = temp;
    
        if (spots[i]==20)
            hole = i;
    }
    
    /* test if the puzzle is solvable */
    for (i=0; i<20; i++)
        tsp[i] = spots[i];
    r=0;

    /* First, check if the problem has even or odd parity,
       depending on where the empty square is */
    if (((4-hole%5) + (3-hole/5))%2 == 1)
        ++r;

    /* Now check how many swaps we need to solve it */
    for (i=0; i<19; i++) {
        while (tsp[i] != (i+1)) {
            temp = tsp[i];
            tsp[i] = tsp[temp-1];
            tsp[temp-1] = temp;
            ++r;
        }
    }
    
    /* if the random puzzle isn't solvable just change two spots */
    if (r%2 == 1) {
        if (spots[0]!=20 && spots[1]!=20) {
            temp = spots[0];
            spots[0] = spots[1];
            spots[1] = temp;
        } else {
            temp = spots[2];
            spots[2] = spots[3];
            spots[3] = temp;
        }
    }
    
    /* draw spots to the lcd */
    for (i=0; i<20; i++)
        draw_spot(spots[i], (i%5)*16, (i/5)*16);
    rb->lcd_update();
}

/* the main game loop */
static int puzzle_loop(void)
{
    int button;
    int lastbutton = BUTTON_NONE;
    int i;
    puzzle_init();
    while(true) {
        button = rb->button_get(true);
        switch (button) {
            case PUZZLE_QUIT:
                /* get out of here */
                return PLUGIN_OK;

            case PUZZLE_SHUFFLE:
#ifdef PUZZLE_SHUFFLE_PICTURE_PRE
                if (lastbutton != PUZZLE_SHUFFLE_PICTURE_PRE)
                    break;
#endif
                /* mix up the pieces */
                puzzle_init();
                break;
    
            case PUZZLE_PICTURE:
#ifdef PUZZLE_SHUFFLE_PICTURE_PRE
                if (lastbutton != PUZZLE_SHUFFLE_PICTURE_PRE)
                    break;
#endif
                /* change picture */
                pic = (pic==true?false:true);
                for (i=0; i<20; i++)
                    draw_spot(spots[i], (i%5)*16, (i/5)*16);
                rb->lcd_update();
                break;
    
            case BUTTON_LEFT:
                if ((hole%5)<4 && !puzzle_finished())
                    move_spot(-1, 0);
                break;
    
            case BUTTON_RIGHT:
                if ((hole%5)>0 && !puzzle_finished())
                    move_spot(1, 0);
                break;
    
            case BUTTON_UP:
                if ((hole/5)<3 && !puzzle_finished())
                    move_spot(0, -1);
                break;
    
            case BUTTON_DOWN:
                if ((hole/5)>0 && !puzzle_finished())
                    move_spot(0, 1);
                break;
    
            default:
                if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                    return PLUGIN_USB_CONNECTED;
                break;
        }
        if (button != BUTTON_NONE)
            lastbutton = button;
    }
}
    
enum plugin_status plugin_start(struct plugin_api* api, void* parameter)
{
    int i, w, h;

    TEST_PLUGIN_API(api);
    (void)parameter;
    rb = api;
    
    /* print title */
    rb->lcd_getstringsize("Sliding Puzzle", &w, &h);
    w = (w+1)/2;
    h = (h+1)/2;
    rb->lcd_clear_display();
    rb->lcd_putsxy(LCD_WIDTH/2-w, (LCD_HEIGHT/2)-h, "Sliding Puzzle");
    rb->lcd_update();
    rb->sleep(HZ);

    /* print instructions */
    rb->lcd_clear_display();
    rb->lcd_setfont(FONT_SYSFIXED);
#if CONFIG_KEYPAD == RECORDER_PAD
    rb->lcd_putsxy(3, 18, "[OFF] to stop");
    rb->lcd_putsxy(3, 28, "[F1] shuffle");
    rb->lcd_putsxy(3, 38, "[F2] change pic");
#elif CONFIG_KEYPAD == ONDIO_PAD
    rb->lcd_putsxy(0, 18, "[OFF] to stop");
    rb->lcd_putsxy(0, 28, "[MODE..] shuffle");
    rb->lcd_putsxy(0, 38, "[MODE] change pic");
#endif
    rb->lcd_update();
    rb->sleep(HZ*2);
    
    rb->lcd_clear_display();
    rb->lcd_drawrect(80, 0, 32, 64);
    rb->lcd_putsxy(81, 10, "Moves");
    for (i=0; i<20; i++) {
        spots[i]=(i+1);
        draw_spot(spots[i], (i%5)*16, (i/5)*16);
    }
    hole = 19;
    pic = true;
    rb->lcd_update();
    rb->sleep(HZ*2);
    
    return puzzle_loop();
}

#endif
