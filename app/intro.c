/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Ian Guinan */
/* GIO-1's intro, once a power-on: the start-up screen (gio_splash.c) wipes in while the chime (gio_chime.c) rises,
 * the star lands with the chord, and INTRO_SONG_DELAY_MS later the instrument starts as it always has (ui_init: the
 * screen, then the song from the top), the chime ringing out under the song's first bars.
 *
 * Safety: nothing here runs before the platform is up. fm1-base's own start-up screen (app_splash, also shown in
 * safe mode) only paints the background; the animation runs in the main loop, a few rows a frame, so USB, the
 * watchdog and the boot guard carry on as before; the keys held at power-on (OCT- + OCT+, REC) are read before it.
 * The screen's clock is the chime's sample count (so the star lands on the chord), or the wall clock if the audio
 * ever falls behind it; however things go, the intro ends by INTRO_MAX_MS.
 *
 * Any control touched skips it: the chime rings out under the song, and the instrument starts once every control is
 * let go (GIO-1's input acts on releases too), or after INTRO_RELEASE_MS (a key stuck down). */

#define INTRO_SONG_DELAY_MS 1000u            /* the star's landing to the song: a breath (1.47 s from the start) */
#define INTRO_MAX_MS 4000u                   /* the intro's end, whatever happens */
#define INTRO_RELEASE_MS 1500u               /* a skip: the controls let go, or this long */

static uint8_t intro_on;                     /* the intro is on the screen */
static uint8_t intro_seen;                   /* it has run since power-on (an update's return: not again) */
static uint8_t intro_skipped;
static uint8_t intro_disabled;               /* (the simulator's scenarios: "intro off") */
static uint32_t intro_t0, intro_skip_t;

static void intro_finish(void)
{
    intro_on = 0;
    ui_init();                               /* the screen drawn, the song from the top */
}

static void intro_start(void)
{
    intro_seen = 1;
    intro_on = 1;
    intro_skipped = 0;
    intro_t0 = plat_ms();
    gio_splash_begin();
    gio_chime_start();
}

static int intro_touched(void)               /* any button, key or encoder (turns drained: they belong to the intro) */
{
    int touched = (plat_buttons() | plat_keys()) != 0, r;
    for (r = 0; r < NE; r++)
        if (plat_enc(r))
            touched = 1;
    return touched;
}

static void intro_frame(void)
{
    uint32_t now = plat_ms(), wall = now - intro_t0;
    uint32_t ms = gio_chime_pos() * 1000u / GIO_CHIME_SR;   /* (under 4 s of samples: no overflow) */
    int drawn;
    ann_tick(now);
    if (wall > ms + 250u)
        ms = wall - 250u;                    /* the audio fell behind (or never started): the wall clock */
    drawn = gio_splash_step(ms);
    if (intro_touched()) {
        if (!intro_skipped) {
            intro_skipped = 1;
            intro_skip_t = now;
        }
    } else if (intro_skipped) {
        intro_finish();
        return;
    }
    if (intro_skipped ? now - intro_skip_t >= INTRO_RELEASE_MS : drawn && ms >= GIO_STAR_MS + INTRO_SONG_DELAY_MS)
        intro_finish();
    else if (wall >= INTRO_MAX_MS) {
        gio_chime_stop();
        intro_finish();
    }
}

void app_splash(void) { gio_splash_begin(); }   /* fm1-base's start-up screen: the intro's background, at once */
