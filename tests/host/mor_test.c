/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, Ian Guinan */
/* The instrument on the host: the pure parts (chords, voice leading, names), the songs' tables, and the render
 * (sound, no NaN, bounded, every section reached, the form loops, Hold and Next, the key, the pattern, the drums'
 * four on the floor, the same seed the same music). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "mor.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static uint32_t midi_count, lead_lo = 999, lead_hi, bass_lo = 999, bass_hi, lead_count, chans;
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    midi_count++;
    if ((st & 0xF0u) == 0x90u && d2)
        chans |= 1u << (st & 15u);
    if (d2 && (st & 0xF0u) == 0x90u) {
        if ((st & 15u) == 2u) {                  /* the lead */
            lead_count++;
            lead_lo = d1 < lead_lo ? d1 : lead_lo;
            lead_hi = d1 > lead_hi ? d1 : lead_hi;
        } else if ((st & 15u) == 0u) {           /* the bass */
            bass_lo = d1 < bass_lo ? d1 : bass_lo;
            bass_hi = d1 > bass_hi ? d1 : bass_hi;
        }
    }
}

static int32_t buf[2 * 256];
static float peak;
static double sum2;
static long nfr;
static int bad;
static uint32_t hash;
static uint32_t seen;                           /* sections reached, bit i */
static void run(float secs)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 4096);
        for (j = 0; j < 512; j++) {
            float x = (float)buf[j] / 8388608.0f;
            if (x != x)
                bad = 1;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += (double)x * (double)x;
            hash = hash * 16777619u ^ (uint32_t)buf[j];
        }
        nfr += 256;
        seen |= 1u << en_sec;
    }
}
static void reset_meters(void)
{
    peak = 0.0f;
    sum2 = 0.0;
    nfr = 0;
    bad = 0;
    seen = 0;
    hash = 2166136261u;
}
static void start(int song, int arrange, uint32_t seed)
{
    int16_t par[P_NPARAMS];
    int i;
    en_init();
    en_song(song);
    en_song_recipe(song, par);
    if (arrange >= 0)
        par[P_ARRANGE] = (int16_t)arrange;
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, par[i]);
    en_seed(seed);
    en_play(1);
    reset_meters();
}

int main(void)
{
    int i, s, k;
    char nm[16];
    /* chords */
    {
        int pcs[4], n;
        n = en_chord_tones(9, 1, pcs);           /* A minor */
        CHECK(n == 3 && pcs[0] == 9 && pcs[1] == 0 && pcs[2] == 4, "Am tones");
        CHECK(!strcmp(en_chord_name(pcs, n, nm), "Am"), "Am name: %s", nm);
        n = en_chord_tones(5, 5, pcs);           /* F minor 11 */
        CHECK(n == 4 && !strcmp(en_chord_name(pcs, n, nm), "Fm11"), "Fm11 name: %s", nm);
        n = en_chord_tones(4, 4, pcs);
        CHECK(!strcmp(en_chord_name(pcs, n, nm), "Em7"), "Em7 name: %s", nm);
        n = en_chord_tones(3, 0, pcs);
        CHECK(!strcmp(en_chord_name(pcs, n, nm), "Eb"), "Eb name: %s", nm);
    }
    {   /* voice leading: every voice in range, every tone sung, small moves */
        int from[4] = {60, 64, 67, 72}, to[4], pcs[4], n = en_chord_tones(5, 0, pcs), m = 0, t, v;
        en_voice_lead(from, pcs, n, to, 53, 76);
        for (t = 0; t < n; t++) {
            int got = 0;
            for (v = 0; v < 4; v++)
                got |= to[v] % 12 == pcs[t];
            CHECK(got, "voice lead: tone %d missing", pcs[t]);
        }
        for (v = 0; v < 4; v++) {
            CHECK(to[v] >= 53 && to[v] <= 76, "voice %d out of range: %d", v, to[v]);
            m += abs(to[v] - from[v]);
        }
        CHECK(m <= 8, "C to F moved %d semitones in all", m);
    }
    /* the songs' tables */
    for (s = 0; s < EN_NSONGS; s++) {
        int16_t par[P_NPARAMS];
        en_song_recipe(s, par);
        CHECK(en_song_name(s)[0] && en_song_after(s)[0], "song %d: no name", s);
        CHECK(en_song_nsec(s) > 1 && en_song_nsec(s) <= EN_MAXSEC, "song %d: %d sections", s, en_song_nsec(s));
        for (i = 0; i < P_NPARAMS; i++) {
            const en_param_t *p = en_param_info(i);
            CHECK(par[i] >= p->lo && par[i] <= p->hi, "song %d: %s %d out of %d..%d", s, p->name, par[i], p->lo, p->hi);
        }
        CHECK(par[P_TEMPO] >= 110 && par[P_TEMPO] <= 130, "song %d: tempo %d", s, par[P_TEMPO]);
        for (k = 0; k < en_song_nsec(s); k++)
            CHECK(en_sec_bars(s, k) > 0 && en_sec_name(s, k)[0], "song %d section %d", s, k);
    }
    /* every song, 4 minutes at its recipe with Arrange off: sound, bounded, every section reached */
    for (s = 0; s < EN_NSONGS; s++) {
        float rms;
        lead_lo = bass_lo = 999;
        lead_hi = bass_hi = 0;
        start(s, 0, 7);
        run(240.0f);
        rms = (float)sqrt(sum2 / (double)(nfr * 2));
        CHECK(!bad, "song %d: NaN", s);
        CHECK(peak < 0.95f && peak > 0.05f, "song %d: peak %.3f", s, (double)peak);
        CHECK(rms > 0.01f, "song %d: too quiet, rms %.4f", s, (double)rms);
        CHECK(seen == (1u << en_song_nsec(s)) - 1u, "song %d: sections reached 0x%X", s, seen);
        CHECK(bass_lo >= 26 && bass_hi <= 64, "song %d: bass %u..%u", s, bass_lo, bass_hi);
        CHECK(lead_hi == 0 || (lead_lo >= 48 && lead_hi <= 96), "song %d: lead %u..%u", s, lead_lo, lead_hi);
        printf("  song %d %-7s peak %5.1f dB  rms %5.1f dB  bass %u..%u  lead %u..%u\n", s, en_song_name(s),
               20.0 * log10((double)peak), 20.0 * log10((double)rms), bass_lo, bass_hi, lead_lo, lead_hi);
    }
    /* Arrange at full: still bounded, still sound, a few minutes */
    for (s = 0; s < EN_NSONGS; s++) {
        start(s, 100, 11);
        run(180.0f);
        CHECK(!bad && peak < 0.95f && peak > 0.05f, "song %d at Arrange 100: peak %.3f", s, (double)peak);
        CHECK(en_sec < en_song_nsec(s), "song %d: section %d", s, en_sec);
    }
    /* four on the floor in a full section: Feel's Chorus (section 2) */
    start(0, 0, 3);
    for (i = 0; i < 2000 && en_sec != 2; i++)
        run(0.1f);
    run(1.0f);
    CHECK(en_sec == 2 && (en_dpat[0] & 0x1111u) == 0x1111u, "four on the floor: 0x%X", en_dpat[0]);
    /* Hold: the section repeats; let go and Next: on to the next on the next bar */
    start(0, 0, 3);
    run(2.0f);
    en_hold(1);
    run(40.0f);
    CHECK(en_sec == 0 && en_held, "hold: section %d", en_sec);
    en_hold(0);
    en_next();
    run(2.5f);
    CHECK(en_sec == 1, "next: section %d", en_sec);
    /* the key: Key moves the chord now */
    start(4, 0, 3);
    run(1.0f);
    CHECK(en_key == 0 && en_chord_root == 0, "Clock starts in C: key %d chord %d", en_key, en_chord_root);
    en_set(P_ROOT, 2);
    run(0.1f);
    CHECK(en_key == 2 && en_chord_root == 2, "Key D: key %d chord %d", en_key, en_chord_root);
    /* the pattern: Slow has four notes a bar */
    en_set(P_PATTERN, 6);
    run(0.1f);
    for (i = 0, k = 0; i < EN_STEPS; i++)
        k += en_bpat[i] >= 0;
    CHECK(k == 4, "Slow: %d notes", k);
    /* the same seed, the same music; another seed, another */
    {
        uint32_t h1, h2, h3;
        start(6, 60, 5);
        run(60.0f);
        h1 = hash;
        start(6, 60, 5);
        run(60.0f);
        h2 = hash;
        start(6, 60, 6);
        run(60.0f);
        h3 = hash;
        CHECK(h1 == h2, "not deterministic");
        CHECK(h1 != h3, "the seed changes nothing");
    }
    /* the drop-in effects: each alone, held 4 s in Feel's chorus, then let go: sound, bounded; all sixteen at once */
    {
        int f;
        float base;
        for (f = 0; f < EN_NFX; f++) {
            start(0, 0, 4);
            run(50.0f);
            reset_meters();
            en_fx(1u << f);
            run(4.0f);
            CHECK(!bad && peak < 0.99f && peak > 0.01f, "effect %s: peak %.3f", EN_FX_NAME[f], (double)peak);
            CHECK(en_fx_on == (1u << f), "effect %s: not on", EN_FX_NAME[f]);
            en_fx(0);
            run(3.0f);
            CHECK(!bad && en_fx_on == 0, "effect %s: not let go", EN_FX_NAME[f]);
        }
        start(0, 0, 4);
        run(50.0f);
        reset_meters();
        run(2.0f);
        base = (float)sqrt(sum2 / (double)(nfr * 2));
        reset_meters();
        en_fx(0xFFFFu);
        run(6.0f);
        CHECK(!bad && peak < 0.99f, "all effects: peak %.3f", (double)peak);
        en_fx(0);
        run(8.0f);
        reset_meters();
        run(2.0f);
        CHECK((float)sqrt(sum2 / (double)(nfr * 2)) > base * 0.4f, "after all effects: quieter than before");
    }
    {   /* Stutter: the output repeats every sixteenth */
        static int32_t a[2 * 6400], b[2 * 6400];
        int steplen = (int)(44100.0f * 60.0f / (126.0f * 4.0f)), i2, same = 0, tot = 0;
        start(0, 0, 4);
        run(50.0f);
        en_fx(1u << FX_STUTTER);
        run(1.0f);
        for (i2 = 0; i2 < steplen; i2++) {
            en_render(buf, 1, 4096);
            a[2 * i2] = buf[0];
        }
        for (i2 = 0; i2 < steplen; i2++) {
            en_render(buf, 1, 4096);
            b[2 * i2] = buf[0];
            tot++;
            same += abs(a[2 * i2] - b[2 * i2]) < 2000;
        }
        CHECK(same > tot * 9 / 10, "stutter: %d of %d samples repeat", same, tot);
        en_fx(0);
    }
    {   /* Octave: the bass an octave up while held */
        int lo1;
        start(4, 0, 4);
        run(3.0f);
        bass_lo = 999;
        run(2.0f);
        lo1 = (int)bass_lo;
        en_fx(1u << FX_OCTAVE);
        run(0.1f);
        bass_lo = 999;
        run(2.0f);
        CHECK((int)bass_lo >= lo1 + 12, "octave: bass from %d, then %u", lo1, bass_lo);
        en_fx(0);
    }
    {   /* the white keys: four notes played, a pause, and an answer of four; one note alone: no answer */
        static const int CALL[4] = {0, 2, 4, 2};
        int k;
        start(0, 0, 4);
        run(2.0f);                               /* Feel's intro: no lead of its own */
        lead_count = 0;
        for (k = 0; k < 4; k++) {
            en_lead_key(CALL[k], 1);
            run(0.24f);
            en_lead_key(CALL[k], 0);
            run(0.12f);
        }
        CHECK(lead_count == 4, "the call: %u lead notes", lead_count);
        run(3.5f);
        CHECK(lead_count == 8, "the answer: %u lead notes in all (4 expected after the call)", lead_count);
        lead_count = 0;
        en_lead_key(3, 1);
        run(0.3f);
        en_lead_key(3, 0);
        run(3.0f);
        CHECK(lead_count == 1, "one note: %u lead notes", lead_count);
    }
    /* MIDI out: the bass, pad, lead and drums on 1, 2, 3, 10; with the other channel set on 4, 5, 6, 11 */
    start(0, 0, 4);
    en_lead_key(2, 1);
    run(0.2f);
    en_lead_key(2, 0);
    chans = 0;
    run(50.0f);
    CHECK(chans == (1u << 0 | 1u << 1 | 1u << 2 | 1u << 9), "channels: 0x%X", chans);
    en_set(P_MCHAN, 1);
    run(0.1f);
    chans = 0;
    run(30.0f);
    CHECK(chans == (1u << 3 | 1u << 4 | 1u << 5 | 1u << 10), "channels, the other set: 0x%X", chans);
    /* stopped: silence after the tails */
    start(1, 0, 3);
    run(20.0f);
    en_play(0);
    run(12.0f);
    reset_meters();
    run(1.0f);
    CHECK(peak < 0.002f, "stopped: peak %.4f", (double)peak);
    CHECK(midi_count > 0, "no MIDI");
    if (fails)
        printf("%d FAILED\n", fails);
    else
        printf("mor_test: all passed\n");
    return fails ? 1 : 0;
}
