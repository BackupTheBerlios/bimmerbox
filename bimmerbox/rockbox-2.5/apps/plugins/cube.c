/***************************************************************************
*             __________               __   ___.
*   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
*   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
*   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
*   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
*                     \/            \/     \/    \/            \/
* $Id: cube.c,v 1.1 2007/09/21 18:43:40 duke4d Exp $
*
* Copyright (C) 2002 Damien Teney
* modified to use int instead of float math by Andreas Zwirtes
* heavily extended by Jens Arnold
*
* All files in this archive are subject to the GNU General Public License.
* See the file COPYING in the source tree root for full license agreement.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
***************************************************************************/
#include "plugin.h"
#include "playergfx.h"
#include "xlcd.h"

/* Loops that the values are displayed */
#define DISP_TIME 30

/* variable button definitions */
#if CONFIG_KEYPAD == RECORDER_PAD
#define CUBE_QUIT          BUTTON_OFF
#define CUBE_X_INC         BUTTON_RIGHT
#define CUBE_X_DEC         BUTTON_LEFT
#define CUBE_Y_INC         BUTTON_UP
#define CUBE_Y_DEC         BUTTON_DOWN
#define CUBE_Z_INC         BUTTON_F2
#define CUBE_Z_DEC         BUTTON_F1
#define CUBE_MODE          BUTTON_F3
#define CUBE_PAUSE         BUTTON_PLAY
#define CUBE_HIGHSPEED     BUTTON_ON

#elif CONFIG_KEYPAD == PLAYER_PAD
#define CUBE_QUIT          BUTTON_STOP
#define CUBE_X_INC         BUTTON_RIGHT
#define CUBE_X_DEC         BUTTON_LEFT
#define CUBE_Y_INC         (BUTTON_ON | BUTTON_RIGHT)
#define CUBE_Y_DEC         (BUTTON_ON | BUTTON_LEFT)
#define CUBE_Z_INC         (BUTTON_MENU | BUTTON_RIGHT)
#define CUBE_Z_DEC         (BUTTON_MENU | BUTTON_LEFT)
#define CUBE_MODE_PRE      BUTTON_MENU
#define CUBE_MODE          (BUTTON_MENU | BUTTON_REL)
#define CUBE_PAUSE         BUTTON_PLAY
#define CUBE_HIGHSPEED_PRE BUTTON_ON
#define CUBE_HIGHSPEED     (BUTTON_ON | BUTTON_REL)

#elif CONFIG_KEYPAD == ONDIO_PAD
#define CUBE_QUIT          BUTTON_OFF
#define CUBE_X_INC         BUTTON_RIGHT
#define CUBE_X_DEC         BUTTON_LEFT
#define CUBE_Y_INC         BUTTON_UP
#define CUBE_Y_DEC         BUTTON_DOWN
#define CUBE_Z_INC         (BUTTON_MENU | BUTTON_UP)
#define CUBE_Z_DEC         (BUTTON_MENU | BUTTON_DOWN)
#define CUBE_MODE_PRE      BUTTON_MENU
#define CUBE_MODE          (BUTTON_MENU | BUTTON_REL)
#define CUBE_PAUSE         (BUTTON_MENU | BUTTON_LEFT)
#define CUBE_HIGHSPEED     (BUTTON_MENU | BUTTON_RIGHT)

#elif (CONFIG_KEYPAD == IRIVER_H100_PAD) || \
      (CONFIG_KEYPAD == IRIVER_H300_PAD)
#define CUBE_QUIT          BUTTON_OFF
#define CUBE_X_INC         BUTTON_RIGHT
#define CUBE_X_DEC         BUTTON_LEFT
#define CUBE_Y_INC         BUTTON_UP
#define CUBE_Y_DEC         BUTTON_DOWN
#define CUBE_Z_INC         (BUTTON_ON | BUTTON_UP)
#define CUBE_Z_DEC         (BUTTON_ON | BUTTON_DOWN)
#define CUBE_MODE          BUTTON_MODE
#define CUBE_PAUSE_PRE     BUTTON_ON
#define CUBE_PAUSE         (BUTTON_ON | BUTTON_REL)
#define CUBE_HIGHSPEED     BUTTON_SELECT

#endif

#ifdef HAVE_LCD_BITMAP
#define MYLCD(fn) rb->lcd_ ## fn
#define DIST (10*LCD_HEIGHT/16)
static int x_off = LCD_WIDTH/2;
static int y_off = LCD_HEIGHT/2;
#if CONFIG_LCD == LCD_SSD1815
#define ASPECT 320 /* = 1.25 (fixed point 24.8) */
#else
#define ASPECT 256 /* = 1.00 */
#endif
#else /* !LCD_BITMAP */
#define MYLCD(fn) pgfx_ ## fn
#define DIST 9
static int x_off = 10;
static int y_off = 7;
#define ASPECT 300 /* = 1.175 */
#endif /* !LCD_BITMAP */

struct point_3D {
    long x, y, z;
};

struct point_2D {
    long x, y;
};

struct line {
    int start, end;
};

struct face {
    int corner[4];
    int line[4];
};

/* initial, unrotated cube corners */
static const struct point_3D sommet[8] =
{
    {-DIST, -DIST, -DIST},
    { DIST, -DIST, -DIST},
    { DIST,  DIST, -DIST},
    {-DIST,  DIST, -DIST},
    {-DIST, -DIST,  DIST},
    { DIST, -DIST,  DIST},
    { DIST,  DIST,  DIST},
    {-DIST,  DIST,  DIST}
};

/* The 12 lines forming the edges */
static const struct line lines[12] =
{
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 7}, {7, 6}, {6, 5}, {5, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
};

static bool lines_drawn[12];

/* The 6 faces of the cube; points are in clockwise order when viewed
   from the outside */
static const struct face faces[6] =
{
    {{0, 1, 2, 3}, {0, 1, 2, 3}},
    {{4, 7, 6, 5}, {4, 5, 6, 7}},
    {{0, 4, 5, 1}, {8, 7, 9, 0}},
    {{2, 6, 7, 3}, {10, 5, 11, 2}},
    {{0, 3, 7, 4}, {3, 11, 4, 8}},
    {{1, 5, 6, 2}, {9, 6, 10, 1}}
};

#if LCD_DEPTH > 1
#ifdef HAVE_LCD_COLOR
static const struct rgb face_colors[6] =
{
    {LCD_MAX_RED, 0, 0},   {LCD_MAX_RED, 0, 0},  {0, LCD_MAX_GREEN, 0},
    {0, LCD_MAX_GREEN, 0}, {0, 0, LCD_MAX_BLUE}, {0, 0, LCD_MAX_BLUE}
};
#else
static const int face_colors[6] =
{
    2*LCD_MAX_LEVEL/3, 2*LCD_MAX_LEVEL/3, LCD_MAX_LEVEL/3, LCD_MAX_LEVEL/3, 0, 0
};
#endif
#endif

enum {
#if LCD_DEPTH > 1
    SOLID,
#endif
    HIDDEN_LINES,
    WIREFRAME,
    NUM_MODES
};

static int mode = 0;

static struct point_3D point3D[8];
static struct point_2D point2D[8];
static long matrice[3][3];

static const int nb_points = 8;
static long z_off = 600;

/* Precalculated sine and cosine * 16384 (fixed point 18.14) */
static const short sin_table[91] =
{
        0,   285,   571,   857,  1142,  1427,  1712,  1996,  2280,  2563,
     2845,  3126,  3406,  3685,  3963,  4240,  4516,  4790,  5062,  5334,
     5603,  5871,  6137,  6401,  6663,  6924,  7182,  7438,  7691,  7943,
     8191,  8438,  8682,  8923,  9161,  9397,  9630,  9860, 10086, 10310,
    10531, 10748, 10963, 11173, 11381, 11585, 11785, 11982, 12175, 12365,
    12550, 12732, 12910, 13084, 13254, 13420, 13582, 13740, 13894, 14043,
    14188, 14329, 14466, 14598, 14725, 14848, 14967, 15081, 15190, 15295,
    15395, 15491, 15582, 15668, 15749, 15825, 15897, 15964, 16025, 16082,
    16135, 16182, 16224, 16261, 16294, 16321, 16344, 16361, 16374, 16381,
    16384
};

static struct plugin_api* rb;

static long sin(int val)
{
    /* Speed improvement through sukzessive lookup */
    if (val < 181)
    {
        if (val < 91)
        {
            /* phase 0-90 degree */
            return (long)sin_table[val];
        }
        else
        {
            /* phase 91-180 degree */
            return (long)sin_table[180-val];
        }
    }
    else
    {
        if (val < 271)
        {
            /* phase 181-270 degree */
            return -(long)sin_table[val-180];
        }
        else
        {
            /* phase 270-359 degree */
            return -(long)sin_table[360-val];
        }
    }
    return 0;
}

static long cos(int val)
{
    /* Speed improvement through sukzessive lookup */
    if (val < 181)
    {
        if (val < 91)
        {
            /* phase 0-90 degree */
            return (long)sin_table[90-val];
        }
        else
        {
            /* phase 91-180 degree */
            return -(long)sin_table[val-90];
        }
    }
    else
    {
        if (val < 271)
        {
            /* phase 181-270 degree */
            return -(long)sin_table[270-val];
        }
        else
        {
            /* phase 270-359 degree */
            return (long)sin_table[val-270];
        }
    }
    return 0;
}


static void cube_rotate(int xa, int ya, int za)
{
    int i;
    /* Just to prevent unnecessary lookups */
    long sxa, cxa, sya, cya, sza, cza;

    sxa = sin(xa);
    cxa = cos(xa);
    sya = sin(ya);
    cya = cos(ya);
    sza = sin(za);
    cza = cos(za);

    /* calculate overall translation matrix */
    matrice[0][0] = (cza * cya) >> 14;
    matrice[1][0] = (sza * cya) >> 14;
    matrice[2][0] = -sya;

    matrice[0][1] = (((cza * sya) >> 14) * sxa - sza * cxa) >> 14;
    matrice[1][1] = (((sza * sya) >> 14) * sxa + cxa * cza) >> 14;
    matrice[2][1] = (sxa * cya) >> 14;

    matrice[0][2] = (((cza * sya) >> 14) * cxa + sza * sxa) >> 14;
    matrice[1][2] = (((sza * sya) >> 14) * cxa - cza * sxa) >> 14;
    matrice[2][2] = (cxa * cya) >> 14;

    /* apply translation matrix to all points */
    for (i = 0; i < nb_points; i++)
    {
        point3D[i].x = matrice[0][0] * sommet[i].x + matrice[1][0] * sommet[i].y
                     + matrice[2][0] * sommet[i].z;
        
        point3D[i].y = matrice[0][1] * sommet[i].x + matrice[1][1] * sommet[i].y
                     + matrice[2][1] * sommet[i].z;
        
        point3D[i].z = matrice[0][2] * sommet[i].x + matrice[1][2] * sommet[i].y
                     + matrice[2][2] * sommet[i].z;
    }
}

static void cube_viewport(void)
{
    int i;

    /* Do viewport transformation for all points */
    for (i = 0; i < nb_points; i++)
    {
#if ASPECT != 256
        point2D[i].x = (point3D[i].x * ASPECT) / (point3D[i].z + (z_off << 14))
                     + x_off;
#else
        point2D[i].x = (point3D[i].x << 8) / (point3D[i].z + (z_off << 14))
                     + x_off;
#endif
        point2D[i].y = (point3D[i].y << 8) / (point3D[i].z + (z_off << 14))
                     + y_off;
    }
}

static void cube_draw(void)
{
    int i, j, line;

    switch (mode)
    {
#if LCD_DEPTH > 1
      case SOLID:

        for (i = 0; i < 6; i++)
        {
            /* backface culling; if the shape winds counter-clockwise, we are
             * looking at the backface, and the (simplified) cross product
             * is < 0. Do not draw it. */
            if (0 >= (point2D[faces[i].corner[1]].x - point2D[faces[i].corner[0]].x)
                   * (point2D[faces[i].corner[2]].y - point2D[faces[i].corner[1]].y)
                   - (point2D[faces[i].corner[1]].y - point2D[faces[i].corner[0]].y)
                   * (point2D[faces[i].corner[2]].x - point2D[faces[i].corner[1]].x))
                continue;

            rb->lcd_set_foreground(face_colors[i]);
            xlcd_filltriangle(point2D[faces[i].corner[0]].x,
                              point2D[faces[i].corner[0]].y,
                              point2D[faces[i].corner[1]].x,
                              point2D[faces[i].corner[1]].y,
                              point2D[faces[i].corner[2]].x,
                              point2D[faces[i].corner[2]].y);
            xlcd_filltriangle(point2D[faces[i].corner[0]].x,
                              point2D[faces[i].corner[0]].y,
                              point2D[faces[i].corner[2]].x,
                              point2D[faces[i].corner[2]].y,
                              point2D[faces[i].corner[3]].x,
                              point2D[faces[i].corner[3]].y);

        }
        rb->lcd_set_foreground(LCD_BLACK);
        break;
#endif /* LCD_DEPTH > 1 */

      case HIDDEN_LINES:

        rb->memset(lines_drawn, 0, sizeof(lines_drawn));
        for (i = 0; i < 6; i++)
        {
            /* backface culling; if the shape winds counter-clockwise, we are
             * looking at the backface, and the (simplified) cross product
             * is < 0. Do not draw it. */
            if (0 >= (point2D[faces[i].corner[1]].x - point2D[faces[i].corner[0]].x)
                   * (point2D[faces[i].corner[2]].y - point2D[faces[i].corner[1]].y)
                   - (point2D[faces[i].corner[1]].y - point2D[faces[i].corner[0]].y)
                   * (point2D[faces[i].corner[2]].x - point2D[faces[i].corner[1]].x))
                continue;

            for (j = 0; j < 4; j++)
            {
                line = faces[i].line[j];
                if (!lines_drawn[line])
                {
                    lines_drawn[line] = true;
                    MYLCD(drawline)(point2D[lines[line].start].x,
                                    point2D[lines[line].start].y,
                                    point2D[lines[line].end].x,
                                    point2D[lines[line].end].y);
                }
            }
        }
        break;
      
      case WIREFRAME:

        for (i = 0; i < 12; i++)
            MYLCD(drawline)(point2D[lines[i].start].x,
                            point2D[lines[i].start].y,
                            point2D[lines[i].end].x,
                            point2D[lines[i].end].y);
        break;
    }
}


enum plugin_status plugin_start(struct plugin_api* api, void* parameter)
{
    char buffer[30];
    int t_disp = 0;

    int button;
    int lastbutton = BUTTON_NONE;
    int xa = 0;
    int ya = 0;
    int za = 0;
    int xs = 1;
    int ys = 3;
    int zs = 1;
    bool highspeed = false;
    bool paused = false;
    bool redraw = true;
    bool exit = false;

    TEST_PLUGIN_API(api);
    (void)(parameter);
    rb = api;

#ifdef HAVE_LCD_BITMAP
#if LCD_DEPTH > 1
    xlcd_init(rb);
#endif
    rb->lcd_setfont(FONT_SYSFIXED);
#else
    if (!pgfx_init(rb, 4, 2))
    {
        rb->splash(HZ*2, true, "Old LCD :(");
        return PLUGIN_OK;
    }
    pgfx_display(3, 0);
#endif

    while(!exit)
    {
        if (highspeed)
            rb->yield();
        else
            rb->sleep(4);

        if (redraw)
        {
            MYLCD(clear_display)();
            cube_rotate(xa, ya, za);
            cube_viewport();
            cube_draw();
            redraw = false;
        }

#ifdef HAVE_LCD_BITMAP
        if (t_disp > 0)
        {
            t_disp--;
            rb->snprintf(buffer, sizeof(buffer), "x:%d y:%d z:%d h:%d",
                         xs, ys, zs, highspeed);
            rb->lcd_putsxy(0, LCD_HEIGHT-8, buffer);
            if (t_disp == 0)
                redraw = true;
        }
#else
        if (t_disp > 0)
        {
            if (t_disp == DISP_TIME)
            {
                rb->snprintf(buffer, sizeof(buffer), "x%d", xs);
                rb->lcd_puts(0, 0, buffer);
                rb->snprintf(buffer, sizeof(buffer), "y%d", ys);
                rb->lcd_puts(0, 1, buffer);
                pgfx_display(3, 0);
                rb->snprintf(buffer, sizeof(buffer), "z%d", zs);
                rb->lcd_puts(8, 0, buffer);
                rb->snprintf(buffer, sizeof(buffer), "h%d", highspeed);
                rb->lcd_puts(8, 1, buffer);
            }
            t_disp--;
            if (t_disp == 0)
            {
                rb->lcd_clear_display();
                pgfx_display(3, 0);
            }
        }
#endif
        MYLCD(update)();

        if (!paused)
        {
            xa += xs;
            if (xa > 359)
                xa -= 360;
            if (xa < 0)
                xa += 360;
            ya += ys;
            if (ya > 359)
                ya -= 360;
            if (ya < 0)
                ya += 360;
            za += zs;
            if (za > 359)
                za -= 360;
            if (za < 0)
                za += 360;
            redraw = true;
        }

        button = rb->button_get(false);
        switch (button)
        {
            case CUBE_X_INC:
                if (xs < 10)
                    xs++;
                t_disp = DISP_TIME;
                break;

            case CUBE_X_DEC:
                if (xs > -10)
                    xs--;
                t_disp = DISP_TIME;
                break;

            case CUBE_Y_INC:
                if (ys < 10)
                    ys++;
                t_disp = DISP_TIME;
                break;

            case CUBE_Y_DEC:
                if (ys > -10)
                    ys--;
                t_disp = DISP_TIME;
                break;

            case CUBE_Z_INC:
                if (zs < 10)
                    zs++;
                t_disp = DISP_TIME;
                break;

            case CUBE_Z_DEC:
                if (zs > -10)
                    zs--;
                t_disp = DISP_TIME;
                break;

            case CUBE_MODE:
#ifdef CUBE_MODE_PRE
                if (lastbutton != CUBE_MODE_PRE)
                    break;
#endif
                if (++mode >= NUM_MODES)
                    mode = 0;
                redraw = true;
                break;

            case CUBE_PAUSE:
#ifdef CUBE_PAUSE_PRE
                if (lastbutton != CUBE_PAUSE_PRE)
                    break;
#endif
                paused = !paused;
                break;

            case CUBE_HIGHSPEED:
#ifdef CUBE_HIGHSPEED_PRE
                if (lastbutton != CUBE_HIGHSPEED_PRE)
                    break;
#endif
                highspeed = !highspeed;
                t_disp = DISP_TIME;
                break;

            case CUBE_QUIT:
                exit = true;
                break;

            default:
                if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                {
#ifdef HAVE_LCD_CHARCELLS
                    pgfx_release();
#endif
                    return PLUGIN_USB_CONNECTED;
                }
                break;
        }
        if (button != BUTTON_NONE)
            lastbutton = button;
    }
    
#ifdef HAVE_LCD_CHARCELLS
    pgfx_release();
#endif

    return PLUGIN_OK;
}

