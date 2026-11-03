/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Ian Guinan */
/* GIO-1's start-up chime: a supersaw chord that rises an octave in step with the start-up screen's wipe (intro.c),
 * lands as the star appears, with a bright ping, then rings out. Synthesised in code: no sample in flash.
 *
 *   gio_chime_start()                  arm it (the main loop; the render picks it up at its next block)
 *   gio_chime_mix(lr, n, gain_q12)     the render: adds n stereo frames into lr (int32, 24-bit full scale, as
 *                                      app_render's), MASTER's gain applied; 0 once it has finished
 *   gio_chime_pos()                    the samples played so far: the start-up screen's clock
 *   gio_chime_stop()                   silence it at once (a control touched during the intro)
 *   gio_chime_render(out, n)           the design's own output: n frames of interleaved int16 (the tests' check
 *                                      against gio_chime.wav)
 *
 * The rise lasts GIO_CHIME_RISE_MS, which must equal the wipe plus the pause before the star (intro.c:
 * GIO_WIPE_FRAMES x GIO_FRAME_MS + GIO_STAR_PAUSE = 20 x 16 + 150 = 470 ms). Change one, change the other.
 *
 * The sound: C major add 9 (C3 G3 C4 E4 D5), three saws each, about 13 cents apart (left, centre, right); a triangle
 * sub an octave below; a two-pole low-pass that opens with the rise; a triangle ping (C6 + G6) on landing.
 *
 * Integer maths only, and 32-bit only (nothing in fm1-base's device code needs 64-bit arithmetic; each step below
 * gives exactly the result of the design's 64-bit one). At 48 kHz it matches gio_chime.wav sample for sample until it
 * stops itself (the design rendered the rest of that block, below -60 dB; this is silent): tests/host/chime_test.c. */
#include <stdint.h>

#ifndef GIO_CHIME_SR
#define GIO_CHIME_SR 44100
#endif
#define GIO_CHIME_RISE_MS 470
#define GIO_CHIME_HOLD_MS 250             /* full level after landing */

#define CH_MS(ms) ((uint32_t)(ms) * GIO_CHIME_SR / 1000u)
#define CH_INC(f) ((uint32_t)((f) * 4294967296.0 / GIO_CHIME_SR))   /* (folded at compile time) */
#define CH_RISE CH_MS(GIO_CHIME_RISE_MS)
#define CH_HOLD CH_MS(GIO_CHIME_HOLD_MS)
#define CH_NV 5
#define CH_BLOCK 32u                      /* the glide, level and filter: every 32 samples */

/* the filter's coefficient, Q15: closed at the start, open on landing, then settling */
#define CUT_START 600
#define CUT_PEAK 12000
#define CUT_REST 5000

static const uint32_t ch_note_inc[CH_NV] = {
    CH_INC(130.81), CH_INC(196.00), CH_INC(261.63), CH_INC(329.63), CH_INC(587.33)
};
static const uint32_t ch_sub_target = CH_INC(65.41);
static const uint32_t ch_spark_inc[2] = {CH_INC(1046.50), CH_INC(1567.98)};

static uint32_t ch_ph[CH_NV][3], ch_inc[CH_NV][3];
static uint32_t ch_sub_ph, ch_sub_inc, ch_sp_ph[2];
static int32_t ch_l1, ch_l2, ch_r1, ch_r2, ch_cut;
static int32_t ch_amp, ch_sp_amp;         /* Q24 */
static uint32_t ch_t;
static volatile uint32_t ch_pos;          /* ch_t as the main loop sees it */
static volatile uint8_t ch_active;        /* set last by gio_chime_start, so the render never sees half a start */

static int32_t ch_tri(uint32_t p)         /* a triangle, +-32767 */
{
    int32_t s = (int32_t)(p >> 16) - 32768;
    return (s < 0 ? -s : s) * 2 - 32768;
}

static int16_t ch_sat(int32_t v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)v; }

static uint32_t ch_scale(uint32_t inc, uint32_t r)   /* inc x r >> 16 (r <= 65536), exactly, in 32 bits */
{
    return (inc >> 16) * r + (((inc & 0xFFFFu) * r) >> 16);
}

void gio_chime_start(void)
{
    int v, k;
    ch_active = 0;
    for (v = 0; v < CH_NV; v++)
        for (k = 0; k < 3; k++)           /* the start phases spread, for width */
            ch_ph[v][k] = (uint32_t)v * 0x9E3779B9u + (uint32_t)k * 0x7F4A7C15u;
    ch_sub_ph = ch_sp_ph[0] = ch_sp_ph[1] = 0;
    ch_l1 = ch_l2 = ch_r1 = ch_r2 = 0;
    ch_cut = CUT_START;
    ch_amp = ch_sp_amp = 0;
    ch_t = 0;
    ch_pos = 0;
    ch_active = 1;
}

void gio_chime_stop(void) { ch_active = 0; }
int gio_chime_active(void) { return ch_active; }
uint32_t gio_chime_pos(void) { return ch_pos; }

/* every CH_BLOCK samples: the glide (an octave, easing out), the level and the filter */
static void ch_control(void)
{
    uint32_t r;                           /* the pitch's ratio, Q16: 0.5 .. 1.0 */
    int v;
    if (ch_t < CH_RISE) {
        uint32_t p = (ch_t << 16) / CH_RISE;
        uint32_t q = 65536u - p;
        uint32_t e = 65536u - (q == 65536u ? 65536u : (q * q) >> 16);   /* ease-out (q x q fits but at q = 1.0) */
        r = 32768u + (e >> 1);
        ch_amp = (int32_t)((1u << 22) + e * 192u);                /* 0.25 -> 1 */
        ch_cut = CUT_START + (int32_t)((e * (uint32_t)(CUT_PEAK - CUT_START)) >> 16);
    } else {
        r = 65536u;
        ch_cut -= (ch_cut - CUT_REST) >> 6;
        if (ch_sp_amp == 0 && ch_t < CH_RISE + CH_BLOCK)
            ch_sp_amp = 1 << 24;          /* the landing's ping */
    }
    for (v = 0; v < CH_NV; v++) {
        uint32_t base = ch_scale(ch_note_inc[v], r);
        ch_inc[v][0] = base - (base >> 7);   /* about 13 cents flat */
        ch_inc[v][1] = base;
        ch_inc[v][2] = base + (base >> 7);   /* about 13 cents sharp */
    }
    ch_sub_inc = ch_scale(ch_sub_target, r);
}

/* one stereo frame, int16 (the design's output); clears ch_active when it has rung out */
static void ch_frame(int16_t *o)
{
    int32_t L = 0, R = 0, sub, a, oL, oR;
    int v;
    if ((ch_t & (CH_BLOCK - 1u)) == 0)
        ch_control();
    for (v = 0; v < CH_NV; v++) {
        int32_t s0 = (int32_t)(ch_ph[v][0] >> 16) - 32768;
        int32_t s1 = (int32_t)(ch_ph[v][1] >> 16) - 32768;
        int32_t s2 = (int32_t)(ch_ph[v][2] >> 16) - 32768;
        ch_ph[v][0] += ch_inc[v][0];
        ch_ph[v][1] += ch_inc[v][1];
        ch_ph[v][2] += ch_inc[v][2];
        L += s0 + s1;
        R += s1 + s2;
    }
    L >>= 4;
    R >>= 4;
    ch_l1 += ((L - ch_l1) * ch_cut) >> 15;
    ch_l2 += ((ch_l1 - ch_l2) * ch_cut) >> 15;
    ch_r1 += ((R - ch_r1) * ch_cut) >> 15;
    ch_r2 += ((ch_r1 - ch_r2) * ch_cut) >> 15;
    sub = ch_tri(ch_sub_ph) >> 2;
    ch_sub_ph += ch_sub_inc;
    if (ch_t >= CH_RISE + CH_HOLD)
        ch_amp -= ch_amp >> 14;           /* about 2 s ring-out */
    a = ch_amp >> 9;                      /* Q15 */
    oL = ((ch_l2 + sub) * a) >> 15;
    oR = ((ch_r2 + sub) * a) >> 15;
    if (ch_sp_amp) {
        int32_t s = ch_sp_amp >> 9;
        int32_t p0 = (ch_tri(ch_sp_ph[0]) >> 2) * s >> 15;
        int32_t p1 = (ch_tri(ch_sp_ph[1]) >> 2) * s >> 15;
        ch_sp_ph[0] += ch_spark_inc[0];
        ch_sp_ph[1] += ch_spark_inc[1];
        ch_sp_amp -= ch_sp_amp >> 12;     /* a quick ping */
        oL += p0 + (p1 >> 1);
        oR += (p0 >> 1) + p1;
    }
    o[0] = ch_sat(oL);
    o[1] = ch_sat(oR);
    if (ch_t > CH_RISE + CH_HOLD && ch_amp < (1 << 16))
        ch_active = 0;
    ch_t++;
}

int gio_chime_render(int16_t *out, int frames)
{
    int i;
    for (i = 0; i < frames; i++) {
        if (ch_active)
            ch_frame(out + 2 * i);
        else
            out[2 * i] = out[2 * i + 1] = 0;
    }
    ch_pos = ch_t;
    return ch_active;
}

int gio_chime_mix(int32_t *lr, uint32_t n, uint32_t gain_q12)
{
    uint32_t i;
    if (!ch_active)
        return 0;
    for (i = 0; i < n && ch_active; i++) {
        int16_t o[2];
        int k;
        ch_frame(o);
        for (k = 0; k < 2; k++) {         /* int16 to 24-bit (<< 8), x 0.6875 (its chord a few dB above the songs'
                                           * own level, measured in the simulator), MASTER's gain (Q12) */
            int32_t v = lr[2 * i + k] + (((int32_t)o[k] * (int32_t)gain_q12 * 11) >> 8);
            lr[2 * i + k] = v > 8388607 ? 8388607 : v < -8388608 ? -8388608 : v;
        }
    }
    ch_pos = ch_t;
    return ch_active;
}
