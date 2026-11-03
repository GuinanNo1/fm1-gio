/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Ian Guinan */
/* The start-up chime (app/gio_chime.c), off the device:
 *   - built at 48 kHz (-DGIO_CHIME_SR=48000), it is the design's own render, gio_chime.wav, sample for sample until it
 *     stops itself: its samples' FNV-1a matches that file's (the file is not kept here: 250 KB);
 *   - built at the FM-1's 44.1 kHz, gio_chime_mix adds into the render's buffer at MASTER's gain, clips nothing, rises
 *     for 470 ms, lands, and is over in under 3 s; gio_chime_stop silences it at once.
 * tests/run_tests.sh builds it both ways. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../app/gio_chime.c"

static int fails;
#define CHECK(c, ...) do { if (c) printf("  ok   " __VA_ARGS__); else { printf("  FAIL " __VA_ARGS__); fails++; } \
                           printf("\n"); } while (0)

int main(void)
{
#if GIO_CHIME_SR == 48000
    int16_t b[2 * 256];
    uint64_t h = 0xcbf29ce484222325u;
    uint32_t n = 0, i;
    gio_chime_start();
    while (gio_chime_active()) {
        uint32_t before = ch_t, got;
        gio_chime_render(b, 256);
        got = ch_t - before;                     /* the frames it played (the rest of the block: silence) */
        for (i = 0; i < 2u * got; i++) {
            uint16_t v = (uint16_t)b[i];
            h = (h ^ (v & 0xFFu)) * 0x100000001b3u;
            h = (h ^ (v >> 8)) * 0x100000001b3u;
        }
        n += got;
    }
    CHECK(n == 127535u, "48 kHz: %u frames (gio_chime.wav: 127535 before it stops)", (unsigned)n);
    CHECK(h == 0xce2bc3ccc704d605u, "48 kHz: the samples are gio_chime.wav's (FNV-1a %016llx)", (unsigned long long)h);
#else
    static int32_t lr[2 * 256];
    int32_t peak = 0, peak_rise = 0;
    uint32_t n = 0, i, blocks = 0;
    gio_chime_start();
    while (gio_chime_mix(memset(lr, 0, sizeof lr), 256, 4096u)) {
        for (i = 0; i < 512u; i++) {
            int32_t v = lr[i] < 0 ? -lr[i] : lr[i];
            if (v > peak)
                peak = v;
            if (n < 2048u && v > peak_rise)       /* (the first 46 ms) */
                peak_rise = v;
        }
        n += 256;
        if (++blocks > 1000u)
            break;
    }
    CHECK(n > 2400u * 441u / 10u && n < 3000u * 441u / 10u, "44.1 kHz: over after %u ms", (unsigned)(n * 10u / 441u));
    CHECK(peak < 8000000 && peak > 2000000, "MASTER full: peak %d of 8388607 (no clipping)", (int)peak);
    CHECK(peak_rise < peak / 4, "it rises: %d in its first 46 ms", (int)peak_rise);
    CHECK(CH_RISE == 20727u, "the rise: %u samples (470 ms, the screen's wipe and pause)", (unsigned)CH_RISE);
    gio_chime_start();
    for (i = 0; i < 40u; i++)
        gio_chime_mix(memset(lr, 0, sizeof lr), 256, 4096u);
    CHECK(gio_chime_pos() == 40u * 256u, "its clock: %u samples after 40 blocks", (unsigned)gio_chime_pos());
    gio_chime_stop();
    memset(lr, 0, sizeof lr);
    CHECK(gio_chime_mix(lr, 256, 4096u) == 0 && lr[0] == 0 && lr[511] == 0, "stopped: nothing added");
    lr[0] = 8388000;                             /* a full buffer and the chime: kept in range */
    gio_chime_start();
    for (i = 0; i < 100u; i++) {
        int k;
        for (k = 0; k < 512; k++)
            lr[k] = k & 1 ? -8388000 : 8388000;
        gio_chime_mix(lr, 256, 4096u);
        for (k = 0; k < 512; k++)
            if (lr[k] > 8388607 || lr[k] < -8388608)
                fails++;
    }
    CHECK(1, "mixed into a full buffer: in range");
#endif
    printf(fails ? "CHIME TESTS FAILED\n" : "chime ok\n");
    return fails != 0;
}
