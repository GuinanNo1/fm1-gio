/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, Ian Guinan */
/* GIO-1's engine alone on the host: a song at its recipe, rendered to a WAV, with its levels, the sections it went
 * through and the engine's time (for listening, and for keeping the CPU in check).
 *
 *   gio_render SONG SECS OUT.wav [SEED] [NAME=V ...]   (NAME: a parameter's index, e.g. 9=2 for the Robot lead)
 *
 *   cc -O2 -ffp-contract=off -DEN_HOST -Iapp/dsp -Ibase/include -o gio_render tests/host/gio_render.c app/dsp/gio.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "gio.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st, (void)d1, (void)d2; }

static void w32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void w16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char **argv)
{
    int song = argc > 1 ? atoi(argv[1]) : 0, i, b, blocks;
    float secs = argc > 2 ? (float)atof(argv[2]) : 60.0f;
    const char *path = argc > 3 ? argv[3] : "out.wav";
    uint32_t seed = argc > 4 ? (uint32_t)atoi(argv[4]) : 1u;
    int16_t par[P_NPARAMS];
    static int32_t buf[512];
    double sum2 = 0.0, peak = 0.0, t0, t1;
    long frames = 0, nan = 0;
    int last_sec = -1;
    FILE *f = fopen(path, "wb");
    struct timespec ts;
    if (!f)
        return 2;
    en_init();
    en_song(song);
    en_song_recipe(song, par);
    for (i = 5; i < argc; i++) {
        int k = atoi(argv[i]);
        const char *e = strchr(argv[i], '=');
        if (e && k >= 0 && k < P_NPARAMS)
            par[k] = (int16_t)atoi(e + 1);
    }
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, par[i]);
    en_seed(seed);
    en_play(1);
    blocks = (int)(secs * 44100.0f / 256.0f);
    fwrite("RIFF", 1, 4, f);
    w32(f, 36u + (uint32_t)blocks * 256u * 4u);
    fwrite("WAVEfmt ", 1, 8, f);
    w32(f, 16);
    w16(f, 1);
    w16(f, 2);
    w32(f, 44100);
    w32(f, 44100 * 4);
    w16(f, 4);
    w16(f, 16);
    fwrite("data", 1, 4, f);
    w32(f, (uint32_t)blocks * 256u * 4u);
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t0 = (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
    double eng = 0.0;
    for (b = 0; b < blocks; b++) {
        int16_t s16[512];
        double a, c;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        a = (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
        en_render(buf, 256, 4096);
        clock_gettime(CLOCK_MONOTONIC, &ts);
        c = (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
        eng += c - a;
        for (i = 0; i < 512; i++) {
            double x = (double)buf[i] / 8388608.0;
            if (x != x)
                nan++;
            if (fabs(x) > peak)
                peak = fabs(x);
            sum2 += x * x;
            s16[i] = (int16_t)(x * 32767.0);
        }
        fwrite(s16, 2, 512, f);
        frames += 256;
        if (getenv("CHORDS")) {                  /* CHORDS=1: each chord as it changes */
            static uint32_t last = 0xFFFFFFFFu;
            int pcs[5], k;
            char nm[16];
            uint32_t sig = en_chord_root;
            for (k = 0; k < EN_NPAD; k++)
                sig = sig * 131u + en_chord[k] % 12u;
            if (sig != last) {
                last = sig;
                pcs[0] = en_chord_root;
                for (k = 0; k < EN_NPAD; k++)
                    pcs[k + 1] = en_chord[k] % 12;
                printf("%s ", en_chord_name(pcs, 5, nm));
                fflush(stdout);
            }
        }
        if (en_sec != last_sec) {
            if (getenv("CHORDS"))
                printf("\n");
            last_sec = en_sec;
            printf("  %6.1f s  section %d %-9s %2d bars  layers 0x%02X  key %s\n", (double)frames / 44100.0, en_sec,
                   en_sec_name(song, en_sec), en_sec_len, en_layers, EN_NOTE_NAME[en_key]);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t1 = (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
    fclose(f);
    (void)t0, (void)t1;
    printf("%s: %.0f s, peak %.1f dBFS, rms %.1f dBFS, nan %ld, engine %.2f%% of real time\n",
           en_song_name(song), (double)secs, 20.0 * log10(peak + 1e-12),
           10.0 * log10(sum2 / (double)(frames * 2) + 1e-24), nan, 100.0 * eng / (double)secs);
    return nan ? 1 : 0;
}
