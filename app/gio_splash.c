/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Ian Guinan */
/* GIO-1's start-up screen: the word GIO in bold rounded letters with chrome-blue shading, wiped in left to right,
 * then a four-point star that pops in on the O. Drawn from rectangles: no bitmap in flash.
 *
 *   gio_splash_begin()      the background, and the shading's table
 *   gio_splash_step(ms)     the picture as it is ms after the start; 1 once it is all there.
 *                           The intro (intro.c) calls it every frame with the chime's own clock, so the star lands on
 *                           the chord however fast the screen draws
 *   gio_splash_draw()       the finished picture, at once
 *
 * The design grid: the word is 198 x 64 units, stroke 18, centred on the 240 x 240 screen. GS_SCALE is in eighths:
 * 8 = 1:1. Every shape is drawn a row at a time, so each row takes its colour from a 64-entry chrome table and every
 * shape can be clipped: that is how the wipe works. Each change is drawn whole into fm1-base's canvas (gfx.c: the
 * word's box and the star's room above it, 198 x 72 of its 240 x 144) and sent in one blit: one transfer on the
 * device, and what the browser's emulator sends on to its screen (it follows blits, not fills).
 *
 * Timing: the wipe is GIO_WIPE_FRAMES frames of GIO_FRAME_MS, then GIO_STAR_PAUSE, then the star's steps
 * GIO_FRAME_MS x 2 apart. The wipe and the pause (20 x 16 + 150 = 470 ms) must equal the chime's rise,
 * GIO_CHIME_RISE_MS (gio_chime.c). Change one, change the other. */

#define GIO_WIPE_FRAMES 20
#define GIO_FRAME_MS 16
#define GIO_STAR_PAUSE 150
#define GIO_STAR_MS (GIO_WIPE_FRAMES * GIO_FRAME_MS + GIO_STAR_PAUSE)   /* 470: the chord lands */

#define GS_W 240
#define GS_H 240
#define GS_TOP 8                                       /* the canvas's rows above the word: the star's room */
#define GS_SCALE 8                                     /* /8 */
#define GS_RGB(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))   /* (lcd_fill swaps) */
#define GIO_BG GS_RGB(0x0B, 0x0B, 0x10)
#define GS_WHITE 0xFFFFu
#define GS_SHADED 0x0001u                              /* the chrome table's colour for this shape */
#define GS(v) (((v) * GS_SCALE) / 8)
#define GS_WORD_W 198
#define GS_WORD_H 64

/* the star: on the O's upper-left rim, radius in design units, arms a quarter of it thick. Its size per step, in
 * tenths: it grows, overshoots and settles */
#define GS_STAR_X 133
#define GS_STAR_Y 8
#define GS_STAR_R 10
static const uint8_t gs_star_steps[] = {3, 6, 9, 12, 13, 11, 10};
#define GS_NSTAR (int)sizeof gs_star_steps
#define GIO_SPLASH_MS (GIO_STAR_MS + (GS_NSTAR - 1) * GIO_FRAME_MS * 2)   /* 662: the last step */

/* the chrome: pale sky at the top, deep blue down to a sharp white horizon just above the middle, then light blue
 * fading to navy (row, r, g, b) */
static const uint8_t gs_stops[][4] = {
    {0, 0xCF, 0xE9, 0xFF},
    {28, 0x1E, 0x5F, 0xD0},
    {29, 0xE8, 0xF6, 0xFF},
    {32, 0x6F, 0xB4, 0xFF},
    {63, 0x0A, 0x2A, 0x7A},
};
#define GS_NSTOPS (sizeof gs_stops / sizeof gs_stops[0])

static uint16_t gs_chrome[GS_WORD_H];
static int gs_ox, gs_oy;                               /* the word's top left on the screen */
static int gs_cx0, gs_cy0, gs_cx1, gs_cy1;             /* the clip, screen pixels, end-exclusive */
static int gs_wiped, gs_star;                          /* the wipe's frames shown, the star's steps shown */

static void gs_build_chrome(void)
{
    unsigned i;
    for (i = 0; i + 1 < GS_NSTOPS; i++) {
        int y0 = gs_stops[i][0], y1 = gs_stops[i + 1][0], n = y1 - y0, y;
        for (y = y0; y <= y1; y++) {
            int t = y - y0;
            int r = gs_stops[i][1] + (gs_stops[i + 1][1] - gs_stops[i][1]) * t / n;
            int g = gs_stops[i][2] + (gs_stops[i + 1][2] - gs_stops[i][2]) * t / n;
            int b = gs_stops[i][3] + (gs_stops[i + 1][3] - gs_stops[i][3]) * t / n;
            gs_chrome[y] = GS_RGB(r, g, b);
        }
    }
}

static void gs_clip(int x0, int y0, int x1, int y1)
{
    gs_cx0 = x0;
    gs_cy0 = y0;
    gs_cx1 = x1;
    gs_cy1 = y1;
}

static int gs_isqrt(int v)
{
    int s = 0;
    while ((s + 1) * (s + 1) <= v)
        s++;
    return s;
}

static uint16_t gs_row_colour(int py, uint16_t c)
{
    int dy;
    if (c != GS_SHADED)
        return c;
    dy = (py - gs_oy) * 8 / GS_SCALE;                  /* back to design rows */
    if (dy < 0)
        dy = 0;
    if (dy >= GS_WORD_H)
        dy = GS_WORD_H - 1;
    return gs_chrome[dy];
}

static void gs_span(int x0, int x1, int py, uint16_t c)   /* one clipped row, screen pixels: into the canvas */
{
    if (py < gs_cy0 || py >= gs_cy1)
        return;
    if (x0 < gs_cx0)
        x0 = gs_cx0;
    if (x1 > gs_cx1)
        x1 = gs_cx1;
    if (x1 > x0)
        cv_rect(x0 - gs_ox, py - (gs_oy - GS_TOP), x1 - x0, 1, gs_row_colour(py, c));
}

static void gs_rrect(int x, int y, int w, int h, int r, uint16_t c)   /* a rounded rectangle: design units in */
{
    int j;
    x = gs_ox + GS(x);
    y = gs_oy + GS(y);
    w = GS(w);
    h = GS(h);
    r = GS(r);
    for (j = 0; j < h; j++) {
        int inset = 0;
        if (r) {
            int d = 0;                                 /* rows into the corner */
            if (j < r)
                d = r - j;
            else if (j >= h - r)
                d = j - (h - r) + 1;
            if (d) {
                int t2 = (2 * d - 1) * (2 * d - 1);    /* sampled at the pixel's centre */
                inset = r - gs_isqrt((4 * r * r - t2) / 4);
            }
        }
        gs_span(x + inset, x + w - inset, y + j, c);
    }
}

static void gs_word(void)                              /* the word, through the clip */
{
    /* G: the O's ring and counter, its upper right opened, a squared foot */
    gs_rrect(0, 0, 76, 64, 32, GS_SHADED);
    gs_rrect(18, 18, 40, 28, 14, GIO_BG);
    gs_rrect(46, 0, 30, 32, 0, GIO_BG);
    gs_rrect(58, 32, 18, 32, 0, GS_SHADED);
    /* I */
    gs_rrect(90, 0, 18, 64, 9, GS_SHADED);
    /* O */
    gs_rrect(122, 0, 76, 64, 32, GS_SHADED);
    gs_rrect(140, 18, 40, 28, 14, GIO_BG);
}

static void gs_draw_star(int r)                        /* a tall thin diamond and a wide one: a row each */
{
    int cx = gs_ox + GS(GS_STAR_X), cy = gs_oy + GS(GS_STAR_Y);
    int t = r / 4 > 0 ? r / 4 : 1, dy;
    for (dy = -r; dy <= r; dy++) {
        int a = dy < 0 ? -dy : dy;
        int hv = (r - a) * t / r;                      /* the vertical arm */
        int hh = a < t ? r * (t - a) / t : 0;          /* the horizontal arm */
        int h = hv > hh ? hv : hh;
        gs_span(cx - h, cx + h + 1, cy + dy, GS_WHITE);
    }
}

static void gs_show(int wiped, int star)               /* the word wiped in this far, the star at this step */
{
    cv_begin((uint32_t)GS(GS_WORD_W), (uint32_t)(GS(GS_WORD_H) + GS_TOP), GIO_BG);
    gs_clip(gs_ox, 0, gs_ox + GS(GS_WORD_W) * wiped / GIO_WIPE_FRAMES, GS_H);
    gs_word();
    gs_clip(0, 0, GS_W, GS_H);
    if (star)
        gs_draw_star(GS(GS_STAR_R) * gs_star_steps[star - 1] / 10);
    cv_blit((uint32_t)gs_ox, (uint32_t)(gs_oy - GS_TOP));
    gs_wiped = wiped;
    gs_star = star;
}

void gio_splash_begin(void)
{
    gs_build_chrome();
    gs_ox = (GS_W - GS(GS_WORD_W)) / 2;
    gs_oy = (GS_H - GS(GS_WORD_H)) / 2;
    gs_clip(0, 0, GS_W, GS_H);
    gs_wiped = gs_star = 0;
    lcd_fill(0, 0, GS_W, GS_H, GIO_BG);
}

void gio_splash_draw(void)
{
    gio_splash_begin();
    gs_show(GIO_WIPE_FRAMES, GS_NSTAR);
}

int gio_splash_step(uint32_t ms)
{
    int f = (int)(ms / GIO_FRAME_MS) + 1, s = 0;       /* the wipe's frames due (frame 1 at 0 ms); the star's step */
    if (f > GIO_WIPE_FRAMES)
        f = GIO_WIPE_FRAMES;
    if (ms >= GIO_STAR_MS) {
        s = (int)((ms - GIO_STAR_MS) / (GIO_FRAME_MS * 2)) + 1;
        if (s > GS_NSTAR)
            s = GS_NSTAR;
    }
    if (f != gs_wiped || s != gs_star)
        gs_show(f, s);                                 /* (a late frame: straight to where it should be) */
    return gs_wiped == GIO_WIPE_FRAMES && gs_star == GS_NSTAR;
}
