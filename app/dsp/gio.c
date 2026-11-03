/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, Ian Guinan */
/* The instrument (gio.h). One compilation unit, -O2. From AMB-1's engine: the plate, the tape echo, voice leading,
 * the command queue and the clock; the rest is GIO-1's. */
#include "gio.h"
#include "fastmath.h"

#if defined EN_HOST || defined PLAT_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const EN_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *const EN_PATTERN_NAME[EN_NPATTERNS] = {"Song", "Pump", "Pump 16", "Arpeggio", "Gallop", "Octaves", "Slow"};
enum { A_SONG, A_PUMP, A_PUMP16, A_ARP, A_GALLOP, A_OCT, A_SLOW };

static const en_param_t PARAMS[P_NPARAMS] = {
    {"Tempo", 110, 130, 122}, {"Evolve", 0, 100, 40}, {"Motion", 0, 100, 50}, {"Arrange", 0, 100, 40},
    {"Bass", 0, 100, 80},     {"Cutoff", 0, 100, 45}, {"Resonance", 0, 100, 45}, {"Decay", 0, 100, 35},
    {"Lead", 0, 100, 50},     {"Voice", 0, 9, 0},     {"Busy", 0, 100, 45},   {"Glide", 0, 100, 30},
    {"Pad", 0, 100, 55},      {"Colour", 0, 2, 0},    {"Bright", 0, 100, 50}, {"Phaser", 0, 100, 40},
    {"Drums", 0, 100, 70},    {"Fills", 0, 100, 40},  {"Hats", 0, 100, 55},   {"Swing", 0, 100, 0},
    {"Echo", 0, 100, 45},     {"Echo time", 0, 5, 0}, {"Feedback", 0, 100, 35}, {"Reverb", 0, 100, 40},
    {"Key", 0, 11, 0},        {"Tune", -50, 50, 0},   {"MIDI out", 0, 1, 1},
    {"Pattern", 0, EN_NPATTERNS - 1, 0}, {"Bass oct", -1, 1, 0}, {"Lead oct", -1, 1, 0},
    {"Lead echo", 0, 100, 40}, {"Lead verb", 0, 100, 50}, {"Tone", 0, 100, 50},
    {"Channels", 0, 1, 0},    {"Clock", 0, 2, 0},     {"Clock out", 0, 1, 1},
    {"Bass verb", 0, 100, 50}, {"Pad echo", 0, 100, 0}, {"Pad verb", 0, 100, 50}, {"Drum echo", 0, 100, 45},
    {"Drum verb", 0, 100, 50},
    {"Bass voice", 0, 1, 0}, {"Ratio", 0, 5, 1}, {"FM depth", 0, 100, 50}, {"FM decay", 0, 100, 40},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

static const char *const ETIME_NAME[6] = {"1/16", "1/8", "1/8.", "1/4", "1/4.", "1/2"};
static const float ETIME_BEATS[6] = {0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f};
enum { LV_MINI, LV_ODYSSEY, LV_SYNTHI, LV_FLUTE, LV_THEREMIN, LV_FMLEAD, LV_FMBRASS, LV_FMREED, LV_CHOIR, LV_ROBOT, LV_N };
static const char *const LVOICE_NAME[LV_N] = {"Mini", "Odyssey", "Synthi", "Flute", "Theremin", "FM Lead", "FM Brass",
                                              "FM Reed", "Choir", "Robot"};
static const char *const PCOLOUR_NAME[3] = {"Strings", "Choir", "Saws"};
/* the FM bass's modulator against the note */
static const char *const BRATIO_NAME[6] = {"0.5", "1", "1.5", "2", "3", "4"};
static const float BRATIO[6] = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f};

static char *itoa_s(int v, char *b)
{
    char t[8];
    int n = 0, neg = v < 0;
    if (neg)
        v = -v;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 6);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}

void en_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    const char *s = 0;
    if (i == P_ETIME)
        s = ETIME_NAME[(unsigned)v < 6u ? v : 0];
    else if (i == P_MIDI || i == P_MCLKOUT)
        s = ONOFF[v ? 1 : 0];
    else if (i == P_MCHAN)
        s = v ? "4,5,6,11" : "1,2,3,10";
    else if (i == P_MCLOCK)
        s = v == 2 ? "TRS" : v == 1 ? "USB" : "Internal";
    else if (i == P_LVOICE)
        s = LVOICE_NAME[(unsigned)v < LV_N ? v : 0];
    else if (i == P_PCOLOUR)
        s = PCOLOUR_NAME[(unsigned)v < 3u ? v : 0];
    else if (i == P_BVOICE)
        s = v ? "FM" : "Analog";
    else if (i == P_BRATIO)
        s = BRATIO_NAME[(unsigned)v < 6u ? v : 1];
    else if (i == P_PATTERN)
        s = EN_PATTERN_NAME[(unsigned)v < EN_NPATTERNS ? v : 0];
    else if (i == P_ROOT)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    if (s) {
        int k = 0;
        while (s[k] && k < 9) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if (i == P_SWING) {                          /* as a swing percentage: 50% (straight) .. 75% */
        b = itoa_s(50 + v / 4, b);
        *b++ = '%';
        *b = 0;
        return;
    }
    if ((i == P_TUNE || i == P_BOCT || i == P_LOCT) && v > 0)
        *b++ = '+';
    itoa_s(v, b);
}

/* ------------------------------------------------------------- chords --- */
enum { Q_MAJ, Q_MIN, Q_7, Q_M7, Q_MIN7, Q_MIN11, Q_SUS4, Q_POW, Q_N };
typedef char q_n_is_en_nqual[Q_N == EN_NQUAL ? 1 : -1];
static const int8_t QT[Q_N][4] = {{0, 4, 7, -1}, {0, 3, 7, -1}, {0, 4, 7, 10}, {0, 4, 7, 11},
                                  {0, 3, 7, 10}, {0, 3, 10, 5}, {0, 5, 7, -1}, {0, 7, -1, -1}};

int en_chord_tones(int root, int q, int *pcs)
{
    int n = 0, i;
    q = (unsigned)q < Q_N ? q : 0;
    for (i = 0; i < 4; i++)
        if (QT[q][i] >= 0)
            pcs[n++] = ((root + QT[q][i]) % 12 + 12) % 12;
    return n;
}

static int nearest(int from, int pc, int lo, int hi)    /* the note of class pc nearest from, in lo..hi */
{
    int n = from + ((pc - from) % 12 + 12) % 12;          /* at or above */
    if (n - from > 6)
        n -= 12;
    while (n < lo)
        n += 12;
    while (n > hi)
        n -= 12;
    return n;
}

void en_voice_lead(const int *from, const int *pcs, int nt, int *to, int lo, int hi)
{
    static const uint8_t PERM[24][4] = {
        {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 1, 2}, {0, 3, 2, 1},
        {1, 0, 2, 3}, {1, 0, 3, 2}, {1, 2, 0, 3}, {1, 2, 3, 0}, {1, 3, 0, 2}, {1, 3, 2, 0},
        {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 1, 0, 3}, {2, 1, 3, 0}, {2, 3, 0, 1}, {2, 3, 1, 0},
        {3, 0, 1, 2}, {3, 0, 2, 1}, {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}};
    int t[4], p, v, best = 1 << 30;
    for (v = 0; v < 4; v++)
        t[v] = pcs[v < nt ? v : v - nt];                   /* fewer tones: the first ones doubled */
    for (p = 0; p < 24; p++) {
        int c = 0, n[4], a, b;
        for (v = 0; v < 4; v++) {
            n[v] = nearest(from[v], t[PERM[p][v]], lo, hi);
            c += n[v] > from[v] ? n[v] - from[v] : from[v] - n[v];
        }
        for (a = 0; a < 4; a++)                            /* no two voices on one note */
            for (b = a + 1; b < 4; b++)
                if (n[a] == n[b])
                    c += 40;
        if (c < best) {
            best = c;
            for (v = 0; v < 4; v++)
                to[v] = n[v];
        }
    }
}

const char *en_chord_name(const int *pcs, int n, char *buf)
{
    uint32_t iv = 0;
    int i, k = 0, third, fifth;
    const char *q = "", *r = EN_NOTE_NAME[pcs[0] % 12];
    for (i = 1; i < n; i++)
        iv |= 1u << ((pcs[i] - pcs[0] + 12) % 12);
    third = iv >> 4 & 1u ? 4 : iv >> 3 & 1u ? 3 : 0;
    fifth = iv >> 7 & 1u ? 7 : iv >> 6 & 1u ? 6 : iv >> 8 & 1u ? 8 : 0;
    if (third == 4) {
        q = fifth == 8 ? "aug" : iv >> 11 & 1u ? "M7" : iv >> 10 & 1u ? "7" : iv >> 9 & 1u ? "6"
            : iv >> 2 & 1u ? "add9" : "";
    } else if (third == 3) {
        if (fifth == 6)
            q = iv >> 10 & 1u ? "m7b5" : iv >> 9 & 1u ? "dim7" : "dim";
        else if (!fifth && (iv >> 5 & 1u) && (iv >> 10 & 1u))
            q = "m11";
        else
            q = iv >> 10 & 1u ? "m7" : iv >> 11 & 1u ? "mM7" : iv >> 9 & 1u ? "m6" : iv >> 2 & 1u ? "madd9" : "m";
    } else {
        q = iv >> 5 & 1u ? (iv >> 10 & 1u ? "7sus4" : "sus4") : iv >> 2 & 1u ? "sus2" : "5";
    }
    while (*r)
        buf[k++] = *r++;
    while (*q)
        buf[k++] = *q++;
    buf[k] = 0;
    return buf;
}

/* --------------------------------------------------------------- songs --- */
/* A song: its recipe (every parameter), a scale for the lead, and a form. A section: its bars, which chord
 * changes (prog), its layers, a bass shape (0: the song's), flags and a transposition in semitones. A chord: its
 * root in semitones above the key, its quality, its length in beats; a progression ends at a 0 length and loops for
 * the section. After the last section the song goes back to loop_to. */
enum { F_HALF = 1, F_BUILD = 2, F_BREAK = 4, F_CLICK = 8, F_TOMS = 16 };
enum { M_MAJOR, M_MINOR, M_DORIAN };
static const uint16_t MODE_POOL[3] = {0xAB5u, 0x5ADu, 0x6ADu};   /* bit i: i semitones above the key */
typedef struct {
    int8_t r;
    uint8_t q, beats;
} ch_t;
typedef struct {
    const char *name;
    uint8_t bars, prog, layers, pat, flags;
    int8_t tr;
} sec_t;
#define NPROG 5
#define PROG_MAX 8
typedef struct {
    const char *name;
    int16_t v[P_NPARAMS];
    uint8_t mode, pat, nsec, loop_to;
    sec_t sec[EN_MAXSEC];
    ch_t prog[NPROG][PROG_MAX];
} song_t;

#define DR (L_KICK | L_SNARE | L_HATS | L_OHAT)
#define ALL (L_BASS | L_PAD | L_LEAD | DR)
#define GR (L_BASS | L_PAD | DR)

/* recipes:    Tem Evo Mot Arr  Bas Cut Emp Dec  Led Voi Bus Gli  Pad Col Bri Pha  Drm Fil Hat Swi  Ech ET Fbk Rev  Key Tun MI  Pat BO LO  LEc LRv Ton Chn Clk CkO  BRv PEc PRv DEc DRv  BVo Rat FMd FMy */
static const song_t SONGS[EN_NSONGS] = {
    {"Feel", {126, 40, 55, 35, 85, 42, 50, 35, 40, 1, 35, 30, 45, 2, 45, 45, 70, 30, 55, 0, 60, 0, 30, 35, 0, 0, 1, 0, 0, 0, 40, 45, 50, 0, 0, 0, 50, 0, 50, 60, 50, 0, 1, 50, 40},
     M_MAJOR, A_PUMP, 7, 1,
     {{"Intro", 8, 0, L_BASS | L_HATS, 0, F_BUILD, 0},
      {"Verse", 16, 0, L_BASS | L_PAD | L_KICK | L_HATS | L_OHAT, 0, 0, 0},
      {"Chorus", 8, 1, ALL, 0, 0, 0},
      {"Shift", 8, 0, L_BASS | L_PAD | L_LEAD | L_KICK | L_HATS | L_OHAT, 0, 0, -2},
      {"Verse", 16, 0, L_BASS | L_PAD | L_KICK | L_HATS | L_OHAT | L_SNARE, 0, 0, 0},
      {"Chorus", 8, 1, ALL, 0, 0, 0},
      {"Break", 8, 0, L_BASS | L_PAD | L_HATS, 0, F_BREAK, 0}},
     {{{0, Q_MAJ, 16}}, {{0, Q_MAJ, 8}, {3, Q_MAJ, 8}, {5, Q_MAJ, 8}, {7, Q_MAJ, 8}}}},
    {"Gio", {113, 35, 50, 30, 80, 48, 35, 30, 50, 0, 55, 25, 55, 0, 55, 25, 70, 45, 50, 0, 25, 1, 30, 40, 9, 0, 1, 0, 0, 0, 35, 50, 55, 0, 0, 0, 50, 0, 50, 25, 50, 0, 1, 50, 40},
     M_MINOR, A_ARP, 6, 2,
     {{"Click", 8, 2, L_HATS, 0, F_CLICK, 0},
      {"Arpeggio", 8, 0, L_BASS | L_HATS, 0, F_BUILD | F_CLICK, 0},
      {"Disco", 16, 0, GR, 0, 0, 0},
      {"Strings", 8, 1, L_BASS | L_PAD | L_KICK, 0, 0, 0},
      {"Fills", 8, 0, GR | L_TOMS, 0, F_TOMS, 0},
      {"House", 16, 0, ALL, 0, 0, 0}},
     {{{0, Q_MIN, 8}, {7, Q_MIN, 8}}, {{0, Q_MIN, 8}, {8, Q_MAJ, 8}}, {{0, Q_MIN, 16}}}},
    {"Mutant", {123, 45, 60, 35, 80, 38, 58, 40, 45, 2, 40, 40, 50, 1, 40, 55, 65, 35, 45, 0, 40, 2, 45, 45, 5, 0, 1, 0, 0, 0, 45, 55, 45, 0, 0, 0, 50, 0, 50, 40, 50, 0, 1, 50, 40},
     M_MINOR, A_GALLOP, 6, 2,
     {{"Dawn", 8, 0, L_PAD, 0, F_BUILD, 0},
      {"Sequence", 8, 0, L_BASS | L_PAD, 0, F_BUILD, 0},
      {"Groove", 16, 0, GR, 0, 0, 0},
      {"Theme", 16, 1, ALL, 0, 0, 0},
      {"Bridge", 8, 2, L_BASS | L_PAD | L_KICK | L_SNARE, 0, 0, 0},
      {"Theme", 16, 1, ALL, 0, 0, 0}},
     {{{0, Q_MIN, 16}}, {{0, Q_MIN, 8}, {8, Q_MAJ, 8}}, {{5, Q_MIN, 8}, {0, Q_MIN, 8}}}},
    {"Heaven", {130, 35, 45, 25, 75, 45, 40, 30, 50, 1, 45, 30, 60, 1, 50, 35, 70, 40, 55, 0, 30, 1, 30, 40, 8, 0, 1, 0, 0, 0, 40, 55, 50, 0, 0, 0, 50, 0, 50, 30, 50, 0, 1, 50, 40},
     M_MAJOR, A_PUMP16, 6, 0,
     {{"Slow", 16, 0, L_PAD | L_BASS | L_KICK | L_SNARE | L_LEAD, A_SLOW, F_HALF, 0},
      {"Slow", 16, 0, L_PAD | L_BASS | L_KICK | L_SNARE | L_HATS | L_LEAD, A_SLOW, F_HALF, 0},
      {"Lift", 4, 2, L_PAD | L_BASS | L_SNARE | L_TOMS, A_SLOW, F_BUILD | F_HALF | F_TOMS, 0},
      {"Fast", 16, 1, ALL, 0, 0, 0},
      {"Vamp", 16, 2, ALL, 0, 0, 0},
      {"Fast", 16, 1, ALL, 0, 0, 0}},
     {{{0, Q_MAJ, 8}, {7, Q_MAJ, 8}, {9, Q_MIN11, 8}, {7, Q_MAJ, 8}},
      {{0, Q_MAJ, 4}, {7, Q_MAJ, 4}, {2, Q_MAJ, 4}, {0, Q_MAJ, 4}}, {{9, Q_MIN11, 16}}}},
    {"TickTock", {126, 40, 45, 30, 80, 50, 40, 25, 45, 2, 65, 20, 45, 2, 55, 20, 75, 50, 75, 0, 30, 0, 25, 35, 0, 0, 1, 0, 0, 0, 30, 40, 60, 0, 0, 0, 50, 0, 50, 30, 50, 0, 1, 50, 40},
     M_MAJOR, A_PUMP16, 9, 1,
     {{"Intro", 4, 4, L_BASS | L_KICK | L_HATS, 0, F_BUILD, 0},
      {"Verse", 8, 0, GR, 0, 0, 0},
      {"Pre", 4, 1, GR | L_LEAD, 0, 0, 0},
      {"Chorus", 8, 2, ALL, 0, 0, 0},
      {"Turn", 4, 3, GR | L_TOMS, 0, F_TOMS, 0},
      {"Verse", 8, 0, GR, 0, 0, 3},
      {"Pre", 4, 1, GR | L_LEAD, 0, 0, 3},
      {"Chorus", 8, 2, ALL, 0, 0, 3},
      {"Outro", 4, 4, GR, 0, 0, 3}},
     {{{7, Q_MAJ, 4}, {5, Q_MAJ, 4}}, {{9, Q_MIN, 4}, {2, Q_MIN, 4}, {7, Q_MAJ, 4}, {0, Q_MAJ, 4}},
      {{7, Q_MAJ, 8}, {4, Q_MIN7, 4}, {7, Q_MAJ, 4}}, {{0, Q_MAJ, 8}, {3, Q_MAJ, 8}}, {{0, Q_MAJ, 16}}}},
    {"Energy", {123, 40, 50, 35, 85, 45, 60, 25, 45, 0, 50, 25, 50, 2, 60, 30, 75, 40, 60, 0, 30, 2, 35, 35, 0, 0, 1, 0, 0, 0, 35, 45, 55, 0, 0, 0, 50, 0, 50, 30, 50, 0, 1, 50, 40},
     M_MINOR, A_OCT, 6, 1,
     {{"Intro", 8, 0, L_BASS | L_KICK | L_HATS, 0, F_BUILD, 0},
      {"Groove", 16, 0, GR, 0, 0, 0},
      {"Lift", 8, 1, ALL, 0, 0, 0},
      {"Groove", 16, 2, ALL, 0, 0, 0},
      {"Break", 8, 0, L_BASS | L_PAD, 0, F_BREAK, 0},
      {"Build", 4, 0, L_BASS | L_PAD | L_SNARE, 0, F_BUILD, 0}},
     {{{0, Q_MIN, 16}}, {{8, Q_MAJ, 8}, {10, Q_MAJ, 8}}, {{0, Q_MIN, 8}, {10, Q_MAJ, 8}}}},
    {"Fly", {126, 40, 60, 30, 70, 42, 45, 35, 70, 4, 40, 75, 55, 0, 45, 45, 65, 35, 50, 0, 35, 3, 35, 45, 9, 0, 1, 0, 0, 0, 55, 60, 50, 0, 0, 0, 50, 0, 50, 35, 50, 0, 1, 50, 40},
     M_MINOR, A_OCT, 5, 1,
     {{"Liftoff", 8, 2, L_PAD | L_BASS, 0, F_BUILD, 0},
      {"Theme", 16, 0, ALL, 0, 0, 0},
      {"Drift", 8, 1, L_BASS | L_PAD | L_KICK | L_HATS | L_OHAT, 0, 0, 0},
      {"Theme", 16, 0, ALL, 0, 0, 0},
      {"Space", 8, 2, L_PAD | L_LEAD | L_BASS, 0, F_BREAK, 0}},
     {{{0, Q_MIN, 8}, {8, Q_MAJ, 4}, {10, Q_MAJ, 4}}, {{0, Q_MIN, 8}, {5, Q_MIN, 8}}, {{0, Q_MIN, 16}}}},
    {"Lover", {130, 40, 45, 30, 80, 46, 50, 25, 55, 9, 55, 15, 45, 0, 50, 30, 75, 40, 60, 0, 35, 0, 30, 35, 0, 0, 1, 0, 0, 0, 30, 40, 55, 0, 0, 0, 50, 0, 50, 35, 50, 0, 1, 50, 40},
     M_MINOR, A_PUMP16, 5, 1,
     {{"Intro", 8, 2, L_BASS | L_KICK | L_HATS, 0, F_BUILD, 0},
      {"Verse", 16, 0, GR | L_LEAD, 0, 0, 0},
      {"Hook", 8, 1, ALL, 0, 0, 0},
      {"Robot", 8, 2, L_BASS | L_LEAD | L_KICK | L_HATS, 0, 0, 0},
      {"Hook", 8, 1, ALL, 0, 0, 0}},
     {{{0, Q_MIN, 8}, {10, Q_MAJ, 8}}, {{8, Q_MAJ, 4}, {10, Q_MAJ, 4}, {0, Q_MIN, 8}}, {{0, Q_MIN, 16}}}},
    {"Express", {128, 40, 60, 30, 85, 44, 55, 30, 50, 5, 50, 30, 50, 0, 50, 70, 75, 45, 60, 0, 45, 0, 30, 35, 2, 0, 1, 0, 0, 0, 40, 45, 55, 0, 0, 0, 50, 0, 50, 45, 50, 0, 1, 50, 40},
     M_MINOR, A_PUMP16, 7, 1,
     {{"Intro", 8, 0, L_BASS, 0, F_BUILD, 0},
      {"Drive", 16, 0, L_BASS | L_KICK | L_HATS | L_OHAT, 0, 0, 0},
      {"Strings", 16, 0, GR, 0, 0, 0},
      {"Theme", 16, 1, ALL, 0, 0, 0},
      {"Pursuit", 8, 0, GR | L_TOMS, 0, F_TOMS | F_BUILD, 0},
      {"Theme", 16, 1, ALL, 0, 0, 0},
      {"Run", 8, 2, L_BASS | DR | L_LEAD, 0, F_BUILD, 0}},
     {{{0, Q_MIN, 16}}, {{0, Q_MIN, 8}, {8, Q_MAJ, 4}, {10, Q_MAJ, 4}}, {{0, Q_MIN, 8}, {10, Q_MAJ, 8}}}},
};

static unsigned song_ok(int i) { return (unsigned)i < EN_NSONGS ? (unsigned)i : 0u; }
const char *en_song_name(int i) { return SONGS[song_ok(i)].name; }
void en_song_recipe(int i, int16_t *par)
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        par[p] = SONGS[song_ok(i)].v[p];
}
int en_song_nsec(int i) { return SONGS[song_ok(i)].nsec; }
const char *en_sec_name(int s, int k)
{
    const song_t *g = &SONGS[song_ok(s)];
    return (unsigned)k < g->nsec ? g->sec[k].name : "";
}
int en_sec_bars(int s, int k)
{
    const song_t *g = &SONGS[song_ok(s)];
    return (unsigned)k < g->nsec ? g->sec[k].bars : 0;
}
int en_sec_prog(int s, int k)
{
    const song_t *g = &SONGS[song_ok(s)];
    return (unsigned)k < g->nsec ? g->sec[k].prog : 0;
}

/* the chord changes, live (the Chords page edits them; en_prog_recipe is the song's own) */
const char *const EN_QUAL_SUFFIX[EN_NQUAL] = {"", "m", "7", "M7", "m7", "m11", "sus4", "5"};
static uint16_t progs[EN_NSONGS][NPROG][PROG_MAX];
static uint8_t progs_ready;
volatile int8_t en_prog_slot = -1;
uint16_t en_prog_recipe(int s, int p, int i)
{
    const ch_t *c;
    if ((unsigned)p >= NPROG || (unsigned)i >= PROG_MAX)
        return 0;
    c = &SONGS[song_ok(s)].prog[p][i];
    return c->beats ? EN_CH((c->r % 12 + 12) % 12, c->q, c->beats) : 0;
}
static void progs_init(void)
{
    int s, p, i;
    for (s = 0; s < EN_NSONGS; s++)
        for (p = 0; p < NPROG; p++)
            for (i = 0; i < PROG_MAX; i++)
                progs[s][p][i] = en_prog_recipe(s, p, i);
    progs_ready = 1;
}
uint16_t en_prog_get(int s, int p, int i)
{
    if (!progs_ready)
        progs_init();
    return (unsigned)p < NPROG && (unsigned)i < PROG_MAX ? progs[song_ok(s)][p][i] : 0;
}
void en_prog_set(int s, int p, int i, uint16_t c)
{
    if (!progs_ready)
        progs_init();
    if ((unsigned)p < NPROG && (unsigned)i < PROG_MAX && EN_CH_Q(c) < EN_NQUAL && EN_CH_R(c) < 12 && (i || EN_CH_BEATS(c)))
        progs[song_ok(s)][p][i] = c;
}

/* ------------------------------------------------------------ effects --- */
const char *const EN_FX_NAME[EN_NFX] = {"Throw", "Time x2", "Tape stop", "Swell", "Low-pass", "High-pass", "Sweep",
                                        "Phaser", "Reverb", "Wash", "Freeze", "Stutter", "Drop", "Gate", "Octave", "Roll"};
const char *const EN_FX_ROW[4] = {"Echo", "Filter", "Space", "Moves"};
volatile uint16_t en_fx_on;
static uint16_t fx_mask;
static float fx_a[EN_NFX];                      /* each effect's amount now, 0..1, eased */
static float drop_g = 1.0f, gate_ph;            /* Drop's gain now; where in its sixteenth the Gate is */
static uint32_t roll_n;                         /* Roll: sixteenths so far */

/* ------------------------------------------------------ master effects --- */
/* Stutter keeps a sixteenth of the mix (16-bit, stereo: 25.6 KB in ordinary RAM, not the pool, which the screen
 * mirror's buffers need; the same RAM, as fast) and loops it; Low-pass and High-pass are two
 * one-poles each on the mix; Phaser six all-passes a side, swept; Gate chops the pad in sixteenths; Drop takes all but
 * the bass and the kick out. */
#define STUT_N 6400
static int16_t stut[2 * STUT_N];
static int stut_len, stut_pos, stut_on;        /* stut_on: 1 recording, 2 looping */
static float m_lp[4], m_hp[2], m_ph[2][6], m_phfb[2], m_pha[2], m_phlfo, gate_g = 1.0f;

#define FXA(i) fx_a[FX_##i]

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_PLAY, C_NEXT, C_HOLD, C_SEED, C_PANIC, C_CLOCK, C_SONG, C_FX, C_LKEY };
#define QN 64u
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                                   /* full: dropped (never, at the UI's rate) */
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void en_set(int p, int v) { post(C_SET, p, v); }
void en_play(int on) { post(C_PLAY, on, 0); }
void en_next(void) { post(C_NEXT, 0, 0); }
void en_hold(int on) { post(C_HOLD, on, 0); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_song(int i) { post(C_SONG, i, 0); }
void en_fx(uint32_t mask) { post(C_FX, 0, (int)(mask & 0xFFFFu)); }
void en_lead_key(int step, int on) { post(C_LKEY, step, on); }
volatile int8_t en_lead_step = -1;

/* -------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_ext, en_held;
volatile uint8_t en_song_i, en_sec, en_sec_bar, en_sec_len, en_next_sec;
volatile uint8_t en_layers;
volatile int8_t en_key;
volatile uint8_t en_chord[EN_NPAD], en_chord_root, en_chord_n;
volatile uint8_t en_step;
volatile int8_t en_bpat[EN_STEPS];
volatile uint8_t en_bacc[EN_STEPS];
volatile float en_bass_glow, en_lead_glow, en_pad_level, en_bass_cut;
volatile uint8_t en_lead_note;
volatile float en_drum_glow[EN_NDRUM];
volatile uint32_t en_dpat[EN_NDRUM];
volatile uint32_t en_bars;
volatile float en_arc;

static int16_t par[P_NPARAMS];
static float tunefac = 1.0f;
static uint32_t rng = 0x9E3779B9u;

static uint32_t rnd(void)                       /* Marsaglia's xorshift (Xorshift RNGs, 2003): shifts 13, 17, 5 */
{
    uint32_t x = rng;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    rng = x ^ (x << 5);
    return rng;
}
static float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static int irand(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }

/* MIDI out: the bass on channel 1, the pad 2, the lead 3, the drums 10; or, with Channels at its other setting, 4, 5, 6
 * and 11. Clock (and start, stop, continue) only with Clock out on */
static void midi_raw(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (st < 0xF0u && par[P_MCHAN]) {
        uint32_t ch = st & 15u;
        st = (st & 0xF0u) | (ch == 9u ? 10u : ch < 3u ? ch + 3u : ch);
    }
    en_midi_out(st, d1, d2);
}
static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (!par[P_MIDI] || (st >= 0xF8u && !par[P_MCLKOUT]) || (st >= 0xFAu && st <= 0xFCu && !par[P_MCLKOUT]))
        return;
    midi_raw(st, d1, d2);
}

static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }

#define SN 1024
static float sint[SN + 1];
static inline float sine(float ph)              /* ph in turns, any value >= 0 */
{
    float x = (ph - (float)(int)ph) * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return sint[i] + (sint[i + 1] - sint[i]) * f;
}

static inline float blep_saw(float t, float dt)  /* a band-limited saw, -1..1, phase t in turns */
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}
static inline float blep_sq(float t, float dt)   /* a square: two saws half a turn apart */
{
    float u = t + 0.5f;
    if (u >= 1.0f)
        u -= 1.0f;
    return 0.5f * (blep_saw(t, dt) - blep_saw(u, dt));
}

static void svf_coefs(float *c, float f, float k)   /* TPT SVF (Zavalishin): a1, a2, a3, k */
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 30.0f, 16000.0f) * (1.0f / EN_SR));
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}
/* one sample: s holds the two states; returns the low-pass, *bp the band-pass */
static inline float svf(const float *c, float *s, float x, float *bp)
{
    float v3 = x - s[1], v1 = c[0] * s[0] + c[1] * v3, v2 = s[1] + c[1] * s[0] + c[2] * v3;
    s[0] = 2.0f * v1 - s[0];
    s[1] = 2.0f * v2 - s[1];
    *bp = v1;
    return v2;
}

/* --------------------------------------------------- slow random motion --- */
#define NMOD 6
static struct {
    float v, tgt, rate;
    uint32_t left;
} mod[NMOD];
static void mods_step(uint32_t n)
{
    int i;
    for (i = 0; i < NMOD; i++) {
        if (mod[i].left <= n) {
            float secs = 6.0f + 26.0f * frand();
            mod[i].tgt = frand() * 2.0f - 1.0f;
            mod[i].left = (uint32_t)(secs * EN_SR);
            mod[i].rate = (float)n / (secs * EN_SR * 0.5f);
        } else {
            mod[i].left -= n;
        }
        mod[i].v += (mod[i].tgt - mod[i].v) * fm_minf(mod[i].rate, 1.0f);
    }
}
static float motion, evolve, arrange, arcm;
static float arc, arc_tgt;
static uint32_t arc_left;

/* ------------------------------------------------------------- the form --- */
static const song_t *song = &SONGS[0];
static int si, sbar, slen, srep;                /* the section, its bar, its length, repeats in a row */
static uint8_t layers, hold, next_req;
static int8_t cyc_tr;                           /* the cycle's move: the whole song in another key for a while */
static int chord_q, chord_r, chord_pcs[4], chord_nt;   /* the chord: quality, root (pitch class), tones */
static int key;                                 /* the key now: Key + the section's and the cycle's moves */
static uint32_t sixteenth;
static float step_len;                          /* a sixteenth, in samples */
static int pulse;                               /* the MIDI clock's pulse in the sixteenth, 0..5 */

static const sec_t *sec_now(void) { return &song->sec[si]; }
static int sec_pat(void)
{
    const sec_t *s = sec_now();
    if (par[P_PATTERN] > 0)
        return par[P_PATTERN];
    return s->pat ? s->pat : song->pat;
}

/* --------------------------------------------------------------- bass --- */
/* the shapes, a sixteenth a character: R root, 3 third, 5 fifth, 7 seventh (or octave), O octave, U fifth an
 * octave up; '.' rest. Upper case and digits as they are; the accent mask beside */
static const char *const SHAPE[EN_NPATTERNS] = {
    "", "R.O.R.O.R.O.R.O.", "RORORORORORO5O7O", "R35OR35OO53RO53R", "R.RRO.OOR.RR5.57", "R.O.R.OOR.O.R.OO",
    "R...5...O...5..."};
static const uint16_t ACCENT[EN_NPATTERNS] = {0, 0x1111u, 0x1111u, 0x0101u, 0x1111u, 0x4444u, 0x1111u};
static int8_t bcode[EN_STEPS], bbase[EN_STEPS];  /* the pattern now, and its shape (codes: -1 rest, 0..5) */
static uint16_t bacc;
static int bpat_shape;

static int code_of(char c)
{
    switch (c) {
    case 'R': return 0;
    case '3': return 1;
    case '5': return 2;
    case '7': return 3;
    case 'O': return 4;
    case 'U': return 5;
    default: return -1;
    }
}
static int bass_interval(int code)              /* semitones above the chord's root */
{
    int third = (chord_q == Q_MIN || chord_q == Q_MIN7 || chord_q == Q_MIN11) ? 3 : chord_q == Q_SUS4 ? 5
                : chord_q == Q_POW ? 7 : 4;
    int sev = chord_q == Q_7 || chord_q == Q_MIN7 || chord_q == Q_MIN11 ? 10 : chord_q == Q_M7 ? 11 : 12;
    static const int8_t I[6] = {0, 0, 7, 0, 12, 19};
    return code == 1 ? third : code == 3 ? sev : I[code];
}
static int bass_note(int code)
{
    int n = 36 + chord_r + 12 * par[P_BOCT] + bass_interval(code) + (fx_mask >> FX_OCTAVE & 1u ? 12 : 0);
    if (chord_r > 6)
        n -= 12;                                  /* the root in E1..F#2 .. kept low and near */
    return n;
}
static void bass_shape(int a)                   /* a shape into the pattern, its home */
{
    int i;
    a = a > 0 && a < EN_NPATTERNS ? a : A_PUMP;
    bpat_shape = a;
    for (i = 0; i < EN_STEPS; i++)
        bbase[i] = bcode[i] = (int8_t)code_of(SHAPE[a][i]);
    bacc = ACCENT[a];
}
static void bass_publish(void)
{
    int i;
    for (i = 0; i < EN_STEPS; i++) {
        en_bpat[i] = (int8_t)(bcode[i] >= 0 ? bass_note(bcode[i]) : -1);
        en_bacc[i] = (uint8_t)(bacc >> i & 1u);
    }
}
static void bass_evolve(void)                   /* a bar: a weak step changes, or one goes back home */
{
    int s;
    if (frand() < evolve * 0.3f) {
        s = 1 + 2 * irand(8);                    /* an off sixteenth or an off eighth */
        if (frand() < 0.5f)
            s = 2 + 4 * irand(4);
        if (bcode[s] >= 0) {
            static const int8_t ALT[5] = {0, 4, 2, 3, 5};
            bcode[s] = frand() < evolve * 0.4f ? (int8_t)-1 : ALT[irand(5)];
        } else if (bpat_shape != A_SLOW) {
            bcode[s] = (int8_t)(frand() < 0.5f ? 0 : 4);   /* a ghost note */
        }
    }
    if (frand() < 0.2f) {                        /* and home again, a step at a time */
        s = irand(EN_STEPS);
        bcode[s] = bbase[s];
    }
}

typedef struct {
    float ph[3], inc[3];                        /* Analog: two saws and a square; FM: the carrier, modulator, sub */
    float fmenv;                                /* FM: the depth's envelope, 1 at the note, falling */
    float s[4];                                 /* the ladder */
    float amp, aenv, fenv, adec, fdec, rel;
    float g, k;                                 /* the ladder's coefficients (every 16 samples) */
    int32_t gate;                               /* samples left of the gate */
    float acc;
    uint8_t on, note;
} bass_t;
static bass_t bass;
static float bass_vol, bcut, breso, bdec, build;
static int bvoice;                              /* Bass 2: 0 Analog, 1 FM */
static float bratio = 1.0f, bfm = 0.75f, bfmdec;   /* FM: the modulator's ratio, the depth (turns), its decay a sample */

static void bass_trigger(int n, int accent, int32_t len)
{
    float f = mtof((float)n);
    if (bass.note && en_playing)
        midi(0x80, bass.note, 0);
    if (bvoice) {                                  /* FM: the carrier on the note, the modulator at the ratio */
        bass.inc[0] = f * (1.0f / EN_SR);
        bass.inc[1] = f * bratio * (1.0f / EN_SR);
    } else {
        bass.inc[0] = f * 0.99654f * (1.0f / EN_SR);   /* -6 and +6 cents */
        bass.inc[1] = f * 1.00347f * (1.0f / EN_SR);
    }
    bass.inc[2] = f * 0.5f * (1.0f / EN_SR);       /* the square (FM: a sine), an octave down */
    bass.fmenv = 1.0f;
    bass.aenv = 1.0f;
    bass.fenv = 1.0f;
    bass.acc = accent ? 1.0f : 0.0f;
    bass.gate = len;
    bass.on = 1;
    bass.note = (uint8_t)n;
    en_bass_glow = 1.0f;
    midi(0x90, (uint32_t)n, accent ? 110u : 85u);
}

static void bass_coefs(void)                    /* every 16 samples: the cutoff from the knob, envelope, sweeps */
{
    float oct = 1.2f + 6.3f * bcut + (2.2f + 1.3f * bass.acc) * bass.fenv + 1.1f * arcm + 1.6f * build, fc;
    if (sec_now()->flags & F_BREAK)
        oct -= 0.8f;
    oct += 3.2f * FXA(SWEEP);                    /* the Sweep effect: Moroder's hand on the cutoff */
    fc = fm_clampf(55.0f * fm_exp2f(oct), 40.0f, 15000.0f);
    bass.g = 1.0f - fm_expf(-FM_TWO_PI * fc * (1.0f / EN_SR));
    bass.k = breso * 0.92f;
    en_bass_cut = fm_clampf((oct - 1.2f) * (1.0f / 9.0f), 0.0f, 1.0f);
}

/* ---------------------------------------------------------------- pad --- */
typedef struct {
    float ph[2], inc[2];
    float note, target, amp, hf, vib;
    float fs[4], c1[4], c2[4];                  /* the choir's two formants */
    uint8_t hand, midi_note;
} pad_t;
static pad_t pad[EN_NPAD];
static float pad_vol, pad_gain, pad_gate, bright, phaser_d;
static int pcolour;
static const float PAD_PAN[EN_NPAD] = {-0.6f, -0.2f, 0.2f, 0.6f};
static const float VOWEL[3][2] = {{480.0f, 900.0f}, {730.0f, 1090.0f}, {530.0f, 1840.0f}};

static void pad_retarget(void)                  /* the chord to the four voices, voice-led */
{
    int from[4], to[4], i;
    for (i = 0; i < EN_NPAD; i++)
        from[i] = (int)(pad[i].target + 0.5f);
    en_voice_lead(from, chord_pcs, chord_nt, to, 53, 76);
    for (i = 0; i < EN_NPAD; i++) {
        if ((float)to[i] != pad[i].target && pad[i].amp > 0.002f && !pad[i].hand)
            pad[i].hand = 1;                     /* a new note: out quickly, in a little slower */
        else if (pad[i].amp <= 0.002f)
            pad[i].note = (float)to[i];
        pad[i].target = (float)to[i];
        en_chord[i] = (uint8_t)to[i];
    }
    en_chord_root = (uint8_t)chord_pcs[0];
    en_chord_n = (uint8_t)chord_nt;
}

static void pad_midi(pad_t *v)
{
    if (!en_playing)
        return;
    if (v->midi_note)
        midi(0x81, v->midi_note, 0);
    v->midi_note = (uint8_t)v->note;
    if (layers & L_PAD)
        midi(0x91, v->midi_note, 70);
    else
        v->midi_note = 0;
}

static void pad_coefs(pad_t *v, int i)          /* every 32 samples: handoffs, pitch, the choir's vowel */
{
    float f;
    if (v->hand == 1) {
        v->hf -= 32.0f / (0.03f * EN_SR);
        if (v->hf <= 0.0f) {
            v->hf = 0.0f;
            v->hand = 2;
            v->note = v->target;
            pad_midi(v);
        }
    } else if (v->hand == 2) {
        v->hf += 32.0f / (0.12f * EN_SR);
        if (v->hf >= 1.0f) {
            v->hf = 1.0f;
            v->hand = 0;
        }
    }
    v->vib += (5.2f + 0.31f * (float)i) * 32.0f / EN_SR;
    if (v->vib >= 1.0f)
        v->vib -= 1.0f;
    f = mtof(v->note + 0.07f * sine(v->vib) + 0.05f * motion * mod[i].v);
    if (pcolour == 2) {                         /* saws: wider */
        v->inc[0] = f * 0.99135f * (1.0f / EN_SR);
        v->inc[1] = f * 1.00870f * (1.0f / EN_SR);
    } else {
        v->inc[0] = f * 0.99538f * (1.0f / EN_SR);
        v->inc[1] = f * 1.00463f * (1.0f / EN_SR);
    }
    if (pcolour == 1) {
        float vw = fm_clampf(bright * 0.01f + 0.3f * arcm, 0.0f, 1.0f) * 1.999f, f1, f2;
        int a = (int)vw;
        vw -= (float)a;
        f1 = VOWEL[a][0] + (VOWEL[a + 1][0] - VOWEL[a][0]) * vw;
        f2 = VOWEL[a][1] + (VOWEL[a + 1][1] - VOWEL[a][1]) * vw;
        svf_coefs(v->c1, f1 * (1.0f + 0.03f * (float)i), 0.4f);
        svf_coefs(v->c2, f2 * (1.0f - 0.02f * (float)i), 0.3f);
    }
}

/* the phaser: four first-order all-passes a side, swept in quadrature, fed back */
static float ph_s[2][4], ph_fb[2], ph_a[2], ph_lfo;
static float padl[256], padr[256], plp[4];

/* --------------------------------------------------------------- lead --- */
#define PH_BARS 4
static uint16_t ph_on[PH_BARS];                 /* the phrase: onsets a bar */
static int8_t ph_deg[PH_BARS][EN_STEPS];        /* each onset's degree (scale steps above the key) */
static uint8_t ph_len[PH_BARS][EN_STEPS];       /* its length in sixteenths */
static uint8_t ph_rest[PH_BARS];                /* a bar left out: an answer's room */
/* the lead's voices: GIO-1's own takes on the classic lead synths (an envelope, a filter, a vibrato that comes in
 * late, and for the FM ones an index and a ratio) */
typedef struct {
    float att, dec, sus, rel;                   /* seconds, and the sustain's level */
    float fenv, cut, reso;                      /* the filter: its envelope (octaves), its cutoff (times the pitch), 0..1 */
    float vib, vdelay, glide;                   /* vibrato (semitones), when it comes in (s); the shortest glide (s) */
    float idx, idec, ratio;                     /* FM: the index (radians), what it decays to (times), the ratio */
    float gain;
} lvoice_t;
static const lvoice_t LV[LV_N] = {
    /*            att     dec    sus    rel    fenv cut  reso  vib   vdel  glide  idx  idec ratio gain */
    /* Mini */    {0.004f, 0.35f, 0.75f, 0.12f, 2.6f, 1.8f, 0.4f, 0.18f, 0.35f, 0.0f, 0.0f, 1.0f, 1.0f, 1.90f},
    /* Odyssey */ {0.003f, 0.25f, 0.62f, 0.10f, 3.0f, 1.4f, 0.62f, 0.15f, 0.30f, 0.0f, 0.0f, 1.0f, 1.0f, 1.38f},
    /* Synthi */  {0.012f, 0.50f, 0.80f, 0.20f, 1.0f, 2.8f, 0.78f, 0.10f, 0.50f, 0.0f, 0.0f, 1.0f, 1.0f, 1.42f},
    /* Flute */   {0.070f, 0.30f, 0.85f, 0.15f, 0.4f, 4.0f, 0.0f, 0.24f, 0.20f, 0.0f, 0.0f, 1.0f, 1.0f, 0.74f},
    /* Theremin */{0.040f, 0.50f, 0.95f, 0.28f, 0.0f, 16.0f, 0.0f, 0.34f, 0.0f, 0.12f, 0.0f, 1.0f, 1.0f, 0.63f},
    /* FM Lead */ {0.003f, 0.40f, 0.72f, 0.12f, 0.0f, 6.0f, 0.0f, 0.20f, 0.30f, 0.0f, 3.0f, 0.4f, 1.0f, 1.05f},
    /* FM Brass */{0.050f, 0.30f, 0.80f, 0.12f, 0.0f, 6.0f, 0.0f, 0.15f, 0.35f, 0.0f, 3.6f, 1.0f, 1.0f, 0.72f},
    /* FM Reed */ {0.030f, 0.40f, 0.85f, 0.10f, 0.0f, 5.0f, 0.0f, 0.12f, 0.40f, 0.0f, 1.7f, 1.0f, 2.0f, 0.70f},
    /* Choir */   {0.120f, 0.40f, 0.90f, 0.30f, 0.0f, 16.0f, 0.0f, 0.20f, 0.25f, 0.0f, 0.0f, 1.0f, 1.0f, 2.10f},
    /* Robot */   {0.002f, 0.10f, 0.90f, 0.03f, 0.6f, 3.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 2.60f},
};
typedef struct {
    float ph[3], inc, pitch, target, env, fe, vib_t, s[2], ls[4], lg, lk, c[4], fs[6], fc[3][4];
    float ka, kd, kr, kf;                       /* the envelope's and the filter envelope's per-sample rates */
    int32_t gate;
    uint8_t on, note, stage;                    /* stage: 0 attack, 1 decay and sustain */
} lead_t;
static lead_t lead;
static float lead_vol, glide_k, lead_echo = 0.14f, lead_rev = 0.5f, ltone = 0.5f;
static int lvoice;
static float rate_of(float secs) { return 1.0f - fm_expf(-4.6f / (fm_maxf(secs, 0.0005f) * EN_SR)); }
static void lead_voice_set(int v)
{
    const lvoice_t *L;
    lvoice = (unsigned)v < LV_N ? v : 0;
    L = &LV[lvoice];
    lead.ka = rate_of(L->att);
    lead.kd = rate_of(L->dec);
    lead.kr = rate_of(L->rel);
    lead.kf = rate_of(L->dec * 0.8f);
}

static int scale_steps[7];
static void scale_set(void)
{
    int i, m = 0;
    for (i = 0; i < 12; i++)
        if (MODE_POOL[song->mode] >> i & 1u)
            scale_steps[m++] = i;
}
static int deg_note(int d)                      /* a degree (0 = the key's note at C4..B4) to MIDI */
{
    int oct = d >= 0 ? d / 7 : -((6 - d) / 7), s = d - 7 * oct;
    return 60 + key + scale_steps[s] + 12 * oct + 12 * par[P_LOCT];
}

static int pick_onsets(void)                    /* a bar's rhythm, by Busy */
{
    static const uint8_t W[EN_STEPS] = {6, 1, 2, 1, 5, 1, 3, 1, 5, 1, 2, 1, 4, 1, 3, 2};
    int n = 1 + (int)((float)par[P_BUSY] * 0.06f + frand() * 1.5f), i, k;
    uint16_t m = 0;
    if (n > 8)
        n = 8;
    for (k = 0; k < n * 4 && __builtin_popcount(m) < n; k++) {
        i = irand(EN_STEPS);
        if (irand(7) < W[i] + (par[P_BUSY] > 60 ? 1 : 0))
            m |= (uint16_t)(1u << i);
    }
    if (!m)
        m = 1u;
    return m;
}
static void bar_degrees(int b, int start)       /* a stepwise walk through a bar's onsets */
{
    int d = start, s;
    for (s = 0; s < EN_STEPS; s++)
        if (ph_on[b] >> s & 1u) {
            ph_deg[b][s] = (int8_t)d;
            {
                float r = frand();
                int st = r < 0.55f ? 1 : r < 0.82f ? 2 : 3 + irand(2);
                d += frand() < 0.5f ? st : -st;
                if (d < 2)
                    d = 2 + irand(2);
                if (d > 12)
                    d = 11 - irand(2);
            }
        }
}
static void bar_lengths(int b)                  /* each note to the next, or short */
{
    int s, last = -1;
    for (s = 0; s < EN_STEPS; s++)
        if (ph_on[b] >> s & 1u) {
            if (last >= 0)
                ph_len[b][last] = (uint8_t)(s - last);
            last = s;
        }
    if (last >= 0)
        ph_len[b][last] = (uint8_t)(EN_STEPS - last);
    for (s = 0; s < EN_STEPS; s++)
        if ((ph_on[b] >> s & 1u) && (lvoice == 2 || frand() < 0.25f) && ph_len[b][s] > 1)
            ph_len[b][s] = (uint8_t)(ph_len[b][s] > 2 ? 2 : 1);   /* staccato (the robot always) */
}
/* the theme: what the player played on the white keys (a bar to four), which the lead then develops */
static uint8_t has_theme, th_bars;
static uint16_t th_on[PH_BARS];
static int8_t th_deg[PH_BARS][EN_STEPS];
static uint8_t th_len[PH_BARS][EN_STEPS];

static void theme_phrase(void)                  /* the theme, then grown out of it: carried on a third away, turned over */
{
    int b, st, kind = irand(3);
    for (b = 0; b < PH_BARS; b++) {
        int src = b % th_bars, d0 = -99;
        ph_on[b] = th_on[src];
        ph_rest[b] = 0;
        for (st = 0; st < EN_STEPS; st++) {
            int d = th_deg[src][st];
            ph_len[b][st] = th_len[src][st];
            if (d0 == -99 && (th_on[src] >> st & 1u))
                d0 = d;
            if (b >= th_bars)                    /* past the theme: a variation of it */
                d = kind == 0 ? d + 2 : kind == 1 ? (d0 == -99 ? d : 2 * d0 - d) : d - 2;
            ph_deg[b][st] = (int8_t)(d < 0 ? 0 : d > 13 ? 13 : d);
        }
    }
}

static void lead_phrase(void)                   /* A, A', B, A'' (or A, rest, A', rest when sparse) */
{
    int b, s, start = 5 + irand(4);
    if (has_theme) {
        theme_phrase();
        return;
    }
    ph_on[0] = (uint16_t)pick_onsets();
    bar_degrees(0, start);
    bar_lengths(0);
    ph_on[1] = ph_on[0];
    for (s = 0; s < EN_STEPS; s++) {
        ph_deg[1][s] = ph_deg[0][s];
        ph_len[1][s] = ph_len[0][s];
    }
    for (s = EN_STEPS - 1; s >= 0; s--)         /* A': its last note answers */
        if (ph_on[1] >> s & 1u) {
            ph_deg[1][s] = (int8_t)(ph_deg[1][s] + (frand() < 0.5f ? -2 : 2));
            break;
        }
    ph_on[2] = (uint16_t)pick_onsets();
    bar_degrees(2, start + (frand() < 0.5f ? 2 : -2));
    bar_lengths(2);
    ph_on[3] = ph_on[0];
    for (s = 0; s < EN_STEPS; s++) {
        ph_deg[3][s] = ph_deg[0][s];
        ph_len[3][s] = ph_len[0][s];
    }
    for (b = 0; b < PH_BARS; b++)
        ph_rest[b] = 0;
    if (par[P_BUSY] < 50 && frand() < 0.7f - 0.01f * (float)par[P_BUSY]) {
        ph_rest[1] = 1;                         /* call, room, call, room */
        ph_rest[3] = 1;
    }
}
static void lead_vary(void)                     /* Evolve, at each repeat of the phrase */
{
    int b = irand(PH_BARS), s;
    if (frand() < evolve * 0.6f)
        for (s = 0; s < EN_STEPS; s++)
            if ((ph_on[b] >> s & 1u) && frand() < 0.3f) {
                ph_deg[b][s] = (int8_t)(ph_deg[b][s] + (frand() < 0.5f ? 1 : -1));
                break;
            }
    if (frand() < evolve * 0.2f) {
        ph_on[2] = (uint16_t)pick_onsets();
        bar_degrees(2, 5 + irand(5));
        bar_lengths(2);
    }
}
static int lead_pitch(int d, int strong)        /* a degree, on the chord: snapped to it on the strong steps */
{
    int n = deg_note(d), i, best = n, bd = 99;
    for (i = 0; i < chord_nt; i++) {
        int c = nearest(n, chord_pcs[i], 0, 127), dd = c > n ? c - n : n - c;
        if (dd < bd) {
            bd = dd;
            best = c;
        }
    }
    if (strong || bd == 1)                      /* a semitone off the chord is a clash: on it */
        n = best;
    return n;
}
static void lead_note(int n, int32_t len)
{
    if (lead.on && lead.note && en_playing)
        midi(0x82, lead.note, 0);
    if (!lead.on || lead.env < 0.05f) {          /* from silence: no glide; a swoop up when Glide is high */
        lead.pitch = (float)n - (par[P_GLIDE] > 60 ? 5.0f : 0.0f);
        lead.vib_t = 0.0f;
    }
    if (lead.gate <= 0 || lead.env < 0.05f) {    /* a new note after a gap: the envelopes again; legato: they go on */
        lead.stage = 0;
        lead.fe = 1.0f;
    }
    lead.target = (float)n;
    lead.gate = len;
    lead.on = 1;
    lead.note = (uint8_t)n;
    en_lead_note = (uint8_t)n;
    en_lead_glow = 1.0f;
    midi(0x92, (uint32_t)n, 96);
}

/* the player: the white keys play the lead, a step of the song's scale each from the root up; what they play is
 * counted on the grid to the nearest sixteenth. Three notes or more and two beats' pause: a call, which the lead
 * answers a beat or two later in its rhythm, with notes of its own close by, ending on the chord; and it becomes
 * the theme, which the lead plays and develops from then on. */
#define PL_MAX 16
static struct {
    int16_t t;                                  /* sixteenths from the first note */
    int8_t deg;
    uint8_t len;
} pl[PL_MAX], an[PL_MAX];
static int pl_n, pl_held = -1, an_n, an_i;
static uint32_t pl_t0, pl_last, pl_down, an_start;
static uint8_t an_on;
static uint32_t grid_now(void);

static void player_key(int deg, int on)
{
    uint32_t q = grid_now();
    if (on) {
        if (pl_held >= 0 && pl_n)               /* legato: the last note ends here */
            pl[pl_n - 1].len = (uint8_t)fm_clampf((float)(q - pl_down), 1.0f, 16.0f);
        lead_note(deg_note(deg), 0x7FFFFFFF);
        pl_held = deg;
        an_on = 0;                              /* playing again: no answer now */
        if (en_playing) {
            if (!pl_n || q - pl_t0 >= 64u) {     /* a new call */
                pl_n = 0;
                pl_t0 = q;
            }
            if (pl_n < PL_MAX) {
                pl[pl_n].t = (int16_t)(q - pl_t0);
                pl[pl_n].deg = (int8_t)deg;
                pl[pl_n].len = 1;
                pl_n++;
            }
        }
        pl_down = pl_last = q;
    } else if (pl_held == deg) {
        if (pl_n)
            pl[pl_n - 1].len = (uint8_t)fm_clampf((float)(q - pl_down), 1.0f, 16.0f);
        lead.gate = 0;
        if (lead.note)
            midi(0x82, lead.note, 0);
        pl_held = -1;
        pl_last = q;
    }
}

static void player_answer(uint32_t now)         /* the call heard: the answer, and the theme */
{
    int i, b, st, d = pl[pl_n - 1].deg + (frand() < 0.5f ? -1 : 1) * (1 + irand(2));
    an_start = (now + 7u) & ~3u;                /* on a beat, a beat or two on */
    for (i = 0; i < pl_n; i++) {
        if (i)                                  /* the call's shape, its own notes */
            d += pl[i].deg - pl[i - 1].deg + (frand() < 0.35f ? (frand() < 0.5f ? -1 : 1) : 0);
        an[i].t = pl[i].t;
        an[i].deg = (int8_t)(d < 0 ? 0 : d > 13 ? 13 : d);
        an[i].len = pl[i].len;
    }
    an[pl_n - 1].len = (uint8_t)(an[pl_n - 1].len < 4 ? 4 : an[pl_n - 1].len);   /* the last held longer */
    an_n = pl_n;
    an_i = 0;
    an_on = 1;
    for (b = 0; b < PH_BARS; b++)               /* the theme */
        th_on[b] = 0;
    th_bars = 1;
    for (i = 0; i < pl_n; i++) {
        b = pl[i].t / 16;
        st = pl[i].t % 16;
        if (b >= PH_BARS)
            break;
        th_on[b] |= (uint16_t)(1u << st);
        th_deg[b][st] = pl[i].deg;
        th_len[b][st] = (uint8_t)(pl[i].len > 16 - st ? 16 - st : pl[i].len);
        if (b + 1 > th_bars)
            th_bars = (uint8_t)(b + 1);
    }
    if (th_bars == 3)
        th_bars = 4;                            /* a phrase of four bars divides by its theme */
    has_theme = 1;
    pl_n = 0;
}

static void player_tick(uint32_t now)           /* each sixteenth: the call ended? the answer's notes */
{
    if (pl_n && pl_held < 0 && now - pl_last >= 8u) {
        if (pl_n >= 3)
            player_answer(now);
        else
            pl_n = 0;                           /* one or two notes: just played */
    }
    while (an_on && an_i < an_n && an_start + (uint32_t)an[an_i].t == now) {
        int n, last = an_i == an_n - 1;
        if (last) {                             /* the end: on the chord's root or fifth */
            int a = nearest(deg_note(an[an_i].deg), chord_pcs[0], 48, 96), f = nearest(deg_note(an[an_i].deg), chord_nt > 2 ? chord_pcs[2] : chord_pcs[0], 48, 96);
            int m = deg_note(an[an_i].deg), da = a > m ? a - m : m - a, df = f > m ? f - m : m - f;
            n = da <= df ? a : f;
        } else {
            n = lead_pitch(an[an_i].deg, (an[an_i].t & 3) == 0);
        }
        lead_note(n, (int32_t)((float)an[an_i].len * step_len * 0.92f));
        an_i++;
    }
    if (an_on && an_i >= an_n && now >= an_start + (uint32_t)an[an_n - 1].t + an[an_n - 1].len) {
        an_on = 0;
        lead_phrase();                          /* from now on the theme */
    }
}

/* --------------------------------------------------------------- drums --- */
enum { D_KICK, D_SNARE, D_HAT, D_OHAT, D_TOM };
typedef struct {
    float ph, inc, inc_end, amp, adec, x1, x2, s[2], c[4], mph[4], att, tone, tdec, pend;
    int32_t wait;
    uint8_t on;
} drum_t;
static drum_t drum[EN_NDRUM];
static uint32_t dpat[EN_NDRUM];
static float dvel[EN_NDRUM][EN_STEPS];
static float drum_vol, swing, hat_f = 0.9f, hat_open = 0.35f, hat_lvl = 1.0f, pulse_left;
static uint32_t noise = 22222u;
static const uint8_t DRUM_GM[EN_NDRUM] = {36, 39, 42, 46, 45};
static const float HAT_INC[4] = {304.4f / EN_SR, 369.6f / EN_SR, 522.7f / EN_SR, 800.0f / EN_SR};
static uint8_t fill_bar;                        /* this bar ends in a fill */

static float nz(void)
{
    noise = noise * 1664525u + 1013904223u;
    return (float)(int32_t)noise * (1.0f / 2147483648.0f);
}

static void drum_fire(int d, float vel)
{
    drum_t *v = &drum[d];
    v->on = 1;
    v->amp = vel;
    v->att = 0.0f;
    switch (d) {
    case D_KICK:                                 /* a sine from 150 Hz down to 48, a click on top */
        v->ph = 0.0f;
        v->inc = 150.0f / EN_SR;
        v->inc_end = 48.0f / EN_SR;
        v->adec = fm_expf(-6.9f / (0.32f * EN_SR));
        v->tone = 1.0f;
        break;
    case D_SNARE:                                /* noise through a band-pass, and a short body */
        v->ph = 0.0f;
        v->inc = 185.0f / EN_SR;
        v->adec = fm_expf(-6.9f / (0.2f * EN_SR));
        v->tone = 1.0f;
        v->tdec = fm_expf(-6.9f / (0.07f * EN_SR));
        svf_coefs(v->c, 1700.0f, 1.1f);
        break;
    case D_HAT:
        v->adec = fm_expf(-6.9f / (0.045f * EN_SR));
        drum[D_OHAT].adec = fm_expf(-6.9f / (0.02f * EN_SR));   /* it chokes the open one */
        break;
    case D_OHAT:
        v->adec = fm_expf(-6.9f / (hat_open * EN_SR));
        break;
    default:                                     /* a syndrum: a sine falling an octave */
        v->ph = 0.0f;
        v->adec = fm_expf(-6.9f / (0.45f * EN_SR));
        break;
    }
    en_drum_glow[d] = 1.0f;
    midi(0x99, DRUM_GM[d], (uint32_t)fm_clampf(vel * 110.0f, 1.0f, 127.0f));
    midi(0x89, DRUM_GM[d], 0);
}
static void tom_pitch(float f0)
{
    drum[D_TOM].inc = f0 / EN_SR;
    drum[D_TOM].inc_end = f0 * 0.45f / EN_SR;
}
static void drum_hit(int d, float vel, int off)  /* off: an off sixteenth, which Swing delays */
{
    int32_t w = off ? (int32_t)(swing * step_len) : 0;
    if (w > 0) {
        drum[d].wait = w;
        drum[d].pend = vel;
    } else {
        drum_fire(d, vel);
    }
}
static int32_t drums_wait(void)
{
    int32_t m = 0;
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (!m || drum[d].wait < m))
            m = drum[d].wait;
    return m;
}
static void drums_wait_step(int32_t k)
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (drum[d].wait -= k) <= 0) {
            drum[d].wait = 0;
            drum_fire(d, drum[d].pend);
        }
}

/* a bar's drums, from the section's layers and flags, the knobs and the fill */
static void drums_bar(void)
{
    const sec_t *s = sec_now();
    int i, half = (s->flags & F_HALF) != 0, last = sbar == slen - 1, fills = par[P_FILLS];
    for (i = 0; i < EN_NDRUM; i++)
        dpat[i] = 0;
    for (i = 0; i < EN_STEPS; i++) {
        int beat = (i & 3) == 0;
        if (layers & L_KICK) {
            if (half ? (i == 0 || i == 8) : beat) {
                dpat[D_KICK] |= 1u << i;
                dvel[D_KICK][i] = i == 0 ? 1.0f : 0.92f;
            }
        }
        if (layers & L_SNARE) {
            if (half ? i == 8 : (i == 4 || i == 12)) {
                dpat[D_SNARE] |= 1u << i;
                dvel[D_SNARE][i] = 0.85f;
            }
        }
        if (layers & L_HATS) {
            int on = (s->flags & F_CLICK) ? beat : par[P_HATS] >= 35 ? 1 : (i & 1) == 0;
            if (half && !(s->flags & F_CLICK))
                on = (i & 1) == 0;
            if (on) {
                dpat[D_HAT] |= 1u << i;
                dvel[D_HAT][i] = (s->flags & F_CLICK) ? 0.7f : (i & 3) == 2 ? 0.75f : (i & 1) ? 0.35f : 0.55f;
            }
        }
        if ((layers & L_OHAT) && !half && (i & 3) == 2) {   /* the disco "tss" off the beat */
            dpat[D_OHAT] |= 1u << i;
            dpat[D_HAT] &= ~(1u << i);
            dvel[D_OHAT][i] = 0.7f;
        }
    }
    /* Evolve: a syndrum now and then on the last sixteenths, a ghost snare */
    if ((layers & (L_SNARE | L_TOMS)) && frand() < evolve * 0.12f * (0.4f + 0.012f * (float)fills)) {
        i = frand() < 0.5f ? 14 : 15;
        dpat[D_TOM] |= 1u << i;
        dvel[D_TOM][i] = 0.6f;
    }
    if ((layers & L_SNARE) && !half && frand() < evolve * 0.2f) {
        i = frand() < 0.5f ? 7 : 15;
        dpat[D_SNARE] |= 1u << i;
        dvel[D_SNARE][i] = 0.3f;
    }
    /* the fill: the last bar of a section, the more the higher Fills; toms for a TOMS section or high Fills */
    fill_bar = 0;
    if (last && (layers & (L_SNARE | L_KICK | L_TOMS)) && !hold && frand() < 0.15f + 0.85f * (float)fills * 0.01f) {
        int from = fills > 60 || (s->flags & F_TOMS) ? 8 : 12, toms = (layers & L_TOMS) || fills > 70;
        fill_bar = 1;
        for (i = from; i < EN_STEPS; i++) {
            float v = 0.45f + 0.5f * (float)(i - from) / (float)(EN_STEPS - from);
            if (toms && (i & 1) == 0) {
                dpat[D_TOM] |= 1u << i;
                dvel[D_TOM][i] = v;
            } else {
                dpat[D_SNARE] |= 1u << i;
                dvel[D_SNARE][i] = v * 0.9f;
            }
            dpat[D_OHAT] &= ~(1u << i);
        }
    }
    /* a build: the snare in eighths, then sixteenths, rising, over its last two bars */
    if ((s->flags & F_BUILD) && slen >= 2 && sbar >= slen - 2 && !hold) {
        int fine = sbar == slen - 1;
        for (i = 0; i < EN_STEPS; i++)
            if (fine || (i & 1) == 0) {
                dpat[D_SNARE] |= 1u << i;
                dvel[D_SNARE][i] = 0.3f + 0.5f * ((float)(sbar - (slen - 2)) * 16.0f + (float)i) / 32.0f;
            }
    }
    for (i = 0; i < EN_NDRUM; i++)
        en_dpat[i] = dpat[i];
}

/* ------------------------------------------------------------ harmony --- */
static void chord_at(int beat)                  /* the chord on beat `beat` of the section (its progression loops) */
{
    const uint16_t *p;
    int tot = 0, i, b, r, qq;
    if (!progs_ready)
        progs_init();
    p = progs[en_song_i][sec_now()->prog];
    for (i = 0; i < PROG_MAX && EN_CH_BEATS(p[i]); i++)
        tot += EN_CH_BEATS(p[i]);
    if (!tot) {
        en_prog_slot = -1;
        return;
    }
    b = beat % tot;
    for (i = 0; i < PROG_MAX && EN_CH_BEATS(p[i]); i++) {
        if (b < EN_CH_BEATS(p[i]))
            break;
        b -= EN_CH_BEATS(p[i]);
    }
    if (i >= PROG_MAX || !EN_CH_BEATS(p[i]))
        i = 0;
    en_prog_slot = (int8_t)i;
    r = ((key + EN_CH_R(p[i])) % 12 + 12) % 12;
    qq = EN_CH_Q(p[i]);
    if (r == chord_r && qq == chord_q && chord_nt)
        return;
    chord_r = r;
    chord_q = qq;
    chord_nt = en_chord_tones(r, qq, chord_pcs);
    pad_retarget();
    bass_publish();
}

static void key_set(void)
{
    key = ((par[P_ROOT] + sec_now()->tr + cyc_tr) % 12 + 12) % 12;
    en_key = (int8_t)key;
}

static void section_enter(int i)                /* a section from its first bar */
{
    const sec_t *s;
    si = i;
    s = sec_now();
    sbar = 0;
    slen = s->bars;
    layers = s->layers;
    build = 0.0f;
    /* Arrange: a layer out for this section, or the lead in where it wasn't */
    if (si && frand() < arrange * 0.35f) {
        static const uint8_t DROP[4] = {L_LEAD, L_PAD, L_HATS | L_OHAT, L_SNARE};
        uint8_t d = DROP[irand(4)];
        if ((layers & d) && (layers & ~d & (L_BASS | L_PAD)))
            layers = (uint8_t)(layers & ~d);
    } else if (si && !(layers & L_LEAD) && (layers & L_KICK) && frand() < arrange * 0.25f) {
        layers |= L_LEAD;
    }
    key_set();
    chord_nt = 0;
    chord_at(0);
    if (bpat_shape != sec_pat())
        bass_shape(sec_pat());
    bass_publish();
    if (layers & L_LEAD)
        lead_phrase();
    en_sec = (uint8_t)si;
    en_sec_len = (uint8_t)slen;
    en_sec_bar = 0;
    en_layers = layers;
    en_next_sec = (uint8_t)(si + 1 < song->nsec ? si + 1 : song->loop_to);
}

static void section_next(void)                  /* the end of a section: where next */
{
    int n = si + 1, wrapped = 0;
    if (hold && !next_req) {
        n = si;
    } else if (!next_req && si && frand() < arrange * 0.22f && srep < 1 && slen <= 16) {
        n = si;                                  /* once more */
    } else if (!next_req && frand() < arrange * 0.12f && song->nsec > song->loop_to + 1) {
        n = song->loop_to + irand(song->nsec - song->loop_to);   /* elsewhere in the loop */
    }
    if (n >= song->nsec) {
        n = song->loop_to;
        wrapped = 1;
    }
    next_req = 0;
    srep = n == si ? srep + 1 : 0;
    if (wrapped) {                               /* a new cycle: maybe all of it in another key, maybe home */
        if (cyc_tr && frand() < 0.6f)
            cyc_tr = 0;
        else if (!cyc_tr && frand() < arrange * 0.3f) {
            static const int8_t MOVE[4] = {2, 1, 3, -2};
            cyc_tr = MOVE[irand(4)];
        }
    }
    section_enter(n);
}

/* ------------------------------------------------------------- the grid --- */
static void lead_step(int st)
{
    int b = sbar % PH_BARS;
    if (!(layers & L_LEAD) || ph_rest[b] || pl_held >= 0 || an_on || sixteenth - pl_last < 16u)
        return;                                  /* the player has it, or is being answered */
    if (ph_on[b] >> st & 1u) {
        int n = lead_pitch(ph_deg[b][st], (st & 3) == 0);
        lead_note(n, (int32_t)((float)ph_len[b][st] * step_len * 0.92f));
    }
}

static uint32_t grid_now(void)                  /* the sixteenth nearest now (the step under way, or the next) */
{
    return sixteenth ? sixteenth - 1u + (pulse >= 3 ? 1u : 0u) : 0u;
}

static void tick(void)
{
    int st = (int)(sixteenth & 15u), d;
    if (st == 0) {                               /* a bar */
        if (sixteenth && (++sbar >= slen || next_req))
            section_next();                      /* the end of the section, or EDIT: on this bar */
        else if (sbar)
            bass_evolve();
        if ((sbar % PH_BARS) == 0 && sbar && (layers & L_LEAD))
            lead_vary();
        if (sec_now()->flags & F_BUILD)
            build = (float)(sbar + 1) / (float)slen;
        en_sec_bar = (uint8_t)sbar;
        drums_bar();
        bass_publish();
    }
    if ((st & 3) == 0)
        chord_at(sbar * 4 + st / 4);
    en_step = (uint8_t)st;
    /* the bass: its step */
    if ((layers & L_BASS) && bcode[st] >= 0) {
        int acc = (int)(bacc >> st & 1u), len16 = 1, k;
        for (k = st + 1; k < EN_STEPS && bcode[k] < 0 && len16 < 4; k++)
            len16++;
        if (bpat_shape == A_SLOW)
            len16 = 3;
        bass_trigger(bass_note(bcode[st]), acc, (int32_t)((float)len16 * step_len * (0.45f + 0.004f * (float)par[P_DECAY])));
    }
    player_tick(sixteenth);
    lead_step(st);
    for (d = 0; d < EN_NDRUM; d++)
        if (dpat[d] >> st & 1u) {
            if (d == D_TOM) {
                float f0 = fill_bar && st >= 8 ? 260.0f - 9.0f * (float)(st - 8) : 220.0f;
                tom_pitch(f0);
            }
            drum_hit(d, dvel[d][st] * (0.9f + 0.15f * frand()), st & 1);
        }
    if (fx_mask >> FX_ROLL & 1u)                 /* the Roll effect: a snare every sixteenth, rising */
        drum_fire(D_SNARE, fm_minf(0.3f + 0.03f * (float)roll_n++, 0.95f));
    else
        roll_n = 0;
    gate_ph = 0.0f;
    sixteenth++;
    en_bars = sixteenth;
}

/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in AMB-1 and FoMni): a predelay, four input diffusers, then
 * a figure-eight tank of two halves. The paper's lengths are for 29761 Hz; DS() scales them to 44.1 kHz. */
#define DS(n) (((n) * 14818 + 5000) / 10000)
#define PRE_N 882                                    /* 20 ms */
#define EXC DS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
static float pl_buf[DL_TOTAL] EN_POOL;
typedef struct {
    float *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.72f, lfo_s, lfo_c = 1.0f;

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return d->b[j < 0 ? j + d->n : j];
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = x;
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720));
    push(&dl[L_PRE], in);
    x = tap(&dl[L_PRE], PRE_N);
    pl_bw += 0.55f * (x - pl_bw);
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.0001f * lfo_c, c = lfo_c - 0.0001f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += 0.4f * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, 0.5f);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += 0.4f * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, 0.5f);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
}
static void plate_init(void)
{
    float *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0.0f;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ---------------------------------------------------- echo: the tape --- */
/* AMB-1's tape at a quarter rate (11 kHz, 16-bit): each repeat darker and thinner. Two heads: the first at the echo's
 * time, on the right (the dry note left, its repeat right); the second at twice it, on the left. The first
 * feeds back. */
/* 4.4 s at 11.025 kHz: the longest echo asked for, a half note at 110 (the slowest tempo) with Time x2 on, its second
 * head at twice that (48,108 samples); AMB-1's 62,000 does not leave the FM-1's pool room for the Stutter's buffer */
#define TAPE_N 49000
#define TAPE_SR 11025.0f
static int16_t tape[TAPE_N] EN_POOL;
static int tape_w, tape_ph;
static float tape_acc, tape_lp, tape_hp, tape_p1, tape_p2, tape_c1, tape_c2;
static float echo_send, echo_len, echo_len_t, echo_fb, rev_send, echo_base, fb_eff, echo_glide = 0.002f;
static float ts_slope;                          /* Tape stop: how fast the delay grows (the tape's slowing: pitch 1 - it) */
static float ts_gain = 1.0f;                    /* Tape stop: the tape's level, faded out once it can slow no further */
static uint8_t ts_on;                           /* Tape stop held */
/* each instrument's own sends (the FX pages): the bass's reverb, the pad's echo and reverb, the drums' echo and reverb;
 * at a recipe's values exactly as they were fixed before (the reverbs at 50, the drums' echo the bass's Echo) */
static float bass_rev = 0.06f, pad_echo, pad_rev = 0.45f, drum_echo = 0.2025f, drum_rev = 1.0f;

static inline float tape_read(float d)
{
    int k = (int)d, i0, i1;
    float f = d - (float)k, a, c;
    i0 = tape_w - k;
    i1 = i0 - 1;
    if (i0 < 0)
        i0 += TAPE_N;
    if (i1 < 0)
        i1 += TAPE_N;
    a = (float)tape[i0];
    c = (float)tape[i1];
    return (a + (c - a) * f) * (1.0f / 12000.0f);
}
static inline int16_t s16(float x)
{
    x *= 12000.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}
static void echo_time(void)
{
    float beat, t;
    if (par[P_TEMPO] <= 0)
        return;
    beat = 60.0f / (float)par[P_TEMPO];
    t = ETIME_BEATS[(unsigned)par[P_ETIME] < 6u ? par[P_ETIME] : 0] * beat;
    if (2.0f * t * TAPE_SR > (float)(TAPE_N - 8))
        t = (float)(TAPE_N - 8) / (2.0f * TAPE_SR);
    echo_base = t * TAPE_SR;
    echo_len_t = echo_base;
    if (echo_len <= 0.0f)
        echo_len = echo_len_t;
}
static inline void echoes(float in, float *el, float *er)
{
    tape_acc += in;
    if (++tape_ph >= 4) {
        float h1, h2;
        tape_ph = 0;
        if (ts_on && echo_len < (float)(TAPE_N - 8) * 0.5f - 2.0f) {   /* Tape stop: the tape slows on and on (the
                                                                     * delay growing ever faster), down to a crawl */
            ts_slope += (0.85f - ts_slope) * 0.00036f;   /* (~0.25 s to most of the way) */
            echo_len += ts_slope;
        } else if (ts_on) {
            ts_gain *= 0.9993f;                   /* as far as it goes: stopped, the sound dying away (~0.3 s) */
        } else {
            ts_slope = 0.0f;
            ts_gain += (1.0f - ts_gain) * 0.002f;
            echo_len += (echo_len_t - echo_len) * echo_glide;   /* (let go: back up to speed) */
        }
        h1 = tape_read(echo_len * (1.0f + 0.002f * motion * mod[5].v));
        h2 = tape_read(2.0f * echo_len);
        tape_lp += 0.6f * (h1 - tape_lp);
        tape_hp += 0.04f * (tape_lp - tape_hp);
        if (++tape_w >= TAPE_N)
            tape_w = 0;
        tape[tape_w] = s16(tape_acc * 0.25f + fb_eff * (tape_lp - tape_hp));
        tape_acc = 0.0f;
        tape_p1 = tape_c1;
        tape_p2 = tape_c2;
        tape_c1 = h1;
        tape_c2 = h2;
    }
    {
        float f = (float)tape_ph * 0.25f, h1 = tape_p1 + (tape_c1 - tape_p1) * f, h2 = tape_p2 + (tape_c2 - tape_p2) * f;
        const float t2 = 1.0f - fx_a[FX_TAPESTOP];       /* (Tape stop: the first head only; the second runs backwards) */
        *el = ((0.25f + 0.75f * (1.0f - t2)) * h1 + 0.6f * t2 * h2) * ts_gain;
        *er = (h1 + 0.2f * t2 * h2) * ts_gain;
    }
}

/* ---------------------------------------------------------- parameters --- */
static uint32_t ext_age;
static void tempo_set(void) { step_len = EN_SR * 60.0f / ((float)par[P_TEMPO] * 4.0f); }

static void notes_off(void);
static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    switch (p) {
    case P_TEMPO:
        tempo_set();
        echo_time();
        break;
    case P_EVOLVE: evolve = (float)v * 0.01f; break;
    case P_MOTION: motion = (float)v * 0.01f; break;
    case P_ARRANGE: arrange = (float)v * 0.01f; break;
    case P_BASS: bass_vol = (float)(v * v) * 0.0001f; break;
    case P_CUTOFF: bcut = (float)v * 0.01f; break;
    case P_RESO: breso = (float)v * 0.01f; break;
    case P_DECAY:
        bdec = (float)v * 0.01f;
        bass.adec = fm_expf(-6.9f / ((0.07f + 0.6f * bdec * bdec) * EN_SR));
        bass.fdec = fm_expf(-6.9f / ((0.04f + 0.35f * bdec) * EN_SR));
        break;
    case P_LEAD: lead_vol = (float)(v * v) * 0.0001f; break;
    case P_LVOICE: lead_voice_set(v); break;
    case P_GLIDE: glide_k = 1.0f / ((0.004f + 0.0022f * (float)v) * EN_SR); break;   /* 4 .. 225 ms */
    case P_PAD: pad_vol = (float)(v * v) * 0.0001f; break;
    case P_PCOLOUR: pcolour = v; break;
    case P_BRIGHT: bright = (float)v; break;
    case P_PHASER: phaser_d = (float)v * 0.01f; break;
    case P_DRUMS: drum_vol = (float)(v * v) * 0.0001f; break;
    case P_HATS:                                 /* the hats' level (as before at 55, none at 0), their tone, the open one's
                                                  * length; from 35 sixteenths (the next bar) */
        hat_lvl = (float)v * (1.0f / 55.0f);
        hat_f = 0.55f + 0.0070f * (float)v;
        hat_open = 0.45f - 0.0025f * (float)v;
        break;
    case P_SWING: swing = (float)v * 0.005f; break;
    case P_ECHO: echo_send = (float)(v * v) * 0.0001f; break;
    case P_ETIME: echo_time(); break;
    case P_FEEDBACK: echo_fb = 0.85f * (float)v * 0.01f; break;
    case P_REVERB: rev_send = (float)(v * v) * 0.0001f; break;
    case P_BREV: bass_rev = 0.0012f * (float)v; break;                  /* 50: 0.06, as it was */
    case P_PECHO: pad_echo = (float)(v * v) * 0.00005f; break;          /* new: none at 0 (the recipes) */
    case P_PREV: pad_rev = 0.009f * (float)v; break;                    /* 50: 0.45 */
    case P_DECHO: drum_echo = (float)(v * v) * 0.0001f; break;          /* the law of the bass's Echo */
    case P_DREV: drum_rev = 0.02f * (float)v; break;                    /* 50: as it was */
    case P_BVOICE: bvoice = v != 0; break;
    case P_BRATIO: bratio = BRATIO[(unsigned)v < 6u ? v : 1]; break;
    case P_BFM: bfm = 1.5f * (float)v * 0.01f; break;                    /* the depth: up to 1.5 turns */
    case P_BFMDEC: bfmdec = fm_expf(-6.9f / ((0.03f + 1.5f * (float)(v * v) * 0.0001f) * EN_SR)); break;   /* 30 ms .. 1.5 s */
    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); break;
    case P_LECHO: lead_echo = (float)(v * v) * 0.00009f; break;
    case P_LREV: lead_rev = (float)v * 0.01f; break;
    case P_LTONE: ltone = (float)v * 0.01f; break;
    case P_MCHAN:                                /* the notes on the old channels off first */
        par[P_MCHAN] = (int16_t)!v;
        notes_off();
        par[P_MCHAN] = (int16_t)v;
        break;
    case P_ROOT:
    case P_BOCT:
        if (chord_nt) {                          /* now: the chord again in the new key */
            int r = chord_r;
            key_set();
            chord_nt = 0;
            chord_r = r + 100;
            chord_at(sbar * 4 + (int)(sixteenth & 15u) / 4);
        }
        break;
    case P_PATTERN:
        if (chord_nt) {
            bass_shape(sec_pat());
            bass_publish();
        }
        break;
    case P_MIDI:
        if (!v)
            midi_raw(0xB0, 123, 0);
        break;
    default: break;
    }
}

static void notes_off(void)
{
    int i;
    for (i = 0; i < 3; i++)
        midi_raw(0xB0 | (uint32_t)i, 123, 0);
    midi_raw(0xB9, 123, 0);
    for (i = 0; i < EN_NPAD; i++)
        pad[i].midi_note = 0;
    bass.note = 0;
    lead.note = 0;
}
static void quiet(void)
{
    int i;
    for (i = 0; i < EN_NDRUM; i++)
        drum[i].on = 0, drum[i].wait = 0;
    for (i = 0; i < EN_NPAD; i++)
        pad[i].amp = 0.0f;
    bass.on = 0;
    bass.amp = 0.0f;
    lead.on = 0;
    lead.env = 0.0f;
    notes_off();
}

static void song_start(int i)                   /* a song from its first section, its first bar */
{
    song = &SONGS[song_ok(i)];
    en_song_i = (uint8_t)song_ok(i);
    scale_set();
    cyc_tr = 0;
    srep = 0;
    has_theme = 0;
    pl_n = an_on = 0;
    pl_held = -1;
    hold = 0;
    en_held = 0;
    next_req = 0;
    sixteenth = 0;
    pulse = 0;
    bpat_shape = -1;
    section_enter(0);
    drums_bar();
}

void en_init(void)
{
    int i;
    rng = 0x9E3779B9u;                           /* from the same state every time: a seed decides the rest */
    en_playing = 0;
    en_ext = 0;
    fx_mask = 0;
    en_fx_on = 0;
    for (i = 0; i < EN_NFX; i++)
        fx_a[i] = 0.0f;
    stut_on = stut_pos = 0;
    roll_n = 0;
    drop_g = gate_g = 1.0f;
    m_phlfo = 0.0f;
    for (i = 0; i < 4; i++)
        m_lp[i] = 0.0f;
    m_hp[0] = m_hp[1] = m_phfb[0] = m_phfb[1] = 0.0f;
    {
        int k;
        for (k = 0; k < 6; k++)
            m_ph[0][k] = m_ph[1][k] = 0.0f;
    }
    ext_age = 0;
    pad_gate = 0.0f;
    pulse_left = 0.0f;
    noise = 22222u;
    arc = arc_tgt = 0.0f;
    arc_left = 0;
    ph_lfo = 0.0f;
    for (i = 0; i < 4; i++)
        plp[i] = ph_s[0][i] = ph_s[1][i] = 0.0f;
    ph_fb[0] = ph_fb[1] = 0.0f;
    tape_acc = tape_lp = tape_hp = tape_p1 = tape_p2 = tape_c1 = tape_c2 = 0.0f;
    {
        static const bass_t B0;
        static const lead_t L0;
        static const drum_t D0;
        bass = B0;
        lead = L0;
        for (i = 0; i < EN_NDRUM; i++)
            drum[i] = D0;
    }
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    plate_init();
    for (i = 0; i < TAPE_N; i++)
        tape[i] = 0;
    tape_w = tape_ph = 0;
    echo_len = 0.0f;
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    for (i = 0; i < EN_NPAD; i++) {
        static const pad_t P0;
        pad[i] = P0;
        pad[i].note = pad[i].target = (float)(55 + 5 * i);
        pad[i].amp = 0.0f;
        pad[i].hf = 1.0f;
        pad[i].hand = 0;
        pad[i].ph[0] = 0.13f * (float)i;
        pad[i].ph[1] = 0.37f + 0.2f * (float)i;
        pad[i].vib = 0.25f * (float)i;
        pad[i].midi_note = 0;
    }
    arcm = 0.0f;
    pad_gain = 0.0f;
    for (i = 0; i < P_NPARAMS; i++)              /* every value first: one applied may read another */
        par[i] = PARAMS[i].def;
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    song_start(0);
    for (i = 0; i < EN_NPAD; i++)
        pad[i].note = pad[i].target, pad[i].hand = 0;
}

/* ------------------------------------------------------------ transport --- */
static void clock_pulse(void)
{
    if (pulse == 0)
        tick();
    if (!en_ext)
        midi(0xF8, 0, 0);
    if (++pulse >= 6)
        pulse = 0;
}
static void play_start(int from_top)
{
    int i;
    if (from_top)
        song_start(en_song_i);
    pulse_left = 0.0f;
    en_playing = 1;
    pad_gate = 1.0f;
    for (i = 0; i < EN_NPAD; i++)
        pad_midi(&pad[i]);
    if (!en_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}
static void play_stop(void)
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        drum[d].wait = 0;
    en_playing = 0;
    pad_gate = 0.0f;
    bass.gate = 0;
    lead.gate = 0;
    if (!en_ext)
        midi(0xFC, 0, 0);
    notes_off();
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 24) {
        case C_SET:
            if (a < P_NPARAMS)
                apply(a, b);
            break;
        case C_PLAY:
            if (a && !en_playing)
                play_start(1);
            else if (!a && en_playing)
                play_stop();
            break;
        case C_NEXT: next_req = 1; break;
        case C_HOLD:
            hold = (uint8_t)(a != 0);
            en_held = hold;
            break;
        case C_SEED:
            rng ^= (uint32_t)(b & 0xFFFF) * 2654435761u;
            if (!rng)
                rng = 1u;
            break;
        case C_CLOCK:
            en_ext = 1;
            ext_age = 0;
            if (a == EN_CLK_TICK) {
                if (en_playing)
                    clock_pulse();
            } else if (a == EN_CLK_START) {
                play_start(1);
            } else if (a == EN_CLK_CONTINUE) {
                if (!en_playing)
                    play_start(0);
            } else if (en_playing) {
                play_stop();
            }
            break;
        case C_PANIC: quiet(); break;
        case C_SONG:
            if (en_playing)
                notes_off();
            song_start(a);
            break;
        case C_LKEY:
            player_key(a, b);
            en_lead_step = (int8_t)pl_held;
            break;
        case C_FX:
            fx_mask = (uint16_t)(c & 0xFFFFu);
            en_fx_on = fx_mask;
            break;
        }
    }
}

/* --------------------------------------------------------------- render --- */
#define BLK 256
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK];

static void render_bass(uint32_t n)
{
    uint32_t j;
    const float g = bass_vol * 1.0f;
    bass_t *v = &bass;
    float p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], s0 = v->s[0], s1 = v->s[1], s2 = v->s[2], s3 = v->s[3];
    float a = v->amp, ae = v->aenv, fe = v->fenv, me = v->fmenv;
    const int fm = bvoice;
    const float idx0 = bfm * (0.25f + 0.75f * (1.0f + 0.5f * v->acc)), mdec = bfmdec;
    if (!v->on && a < 1e-5f) {
        v->amp = 0.0f;
        return;
    }
    for (j = 0; j < n; j++) {
        float x, tgt, y;
        if (!(j & 15u)) {
            v->fenv = fe;
            bass_coefs();
        }
        if (v->gate > 0) {
            v->gate--;
            tgt = 0.22f + 0.78f * ae;            /* the decay, to a low sustain */
            ae *= v->adec;
        } else {
            tgt = 0.0f;
        }
        fe *= v->fdec;
        a += (tgt - a) * (tgt > a ? 0.06f : 0.0035f);   /* ~0.4 ms attack, ~6 ms release */
        if (fm) {                                /* FM: a sine on a sine (the depth falling after the note), a sine sub */
            float idx = idx0 * (0.25f + 0.75f * me);
            x = 0.62f * sine(p0 + 8.0f + idx * sine(p1)) + 0.22f * sine(p2);   /* (as loud as Analog: measured) */
            me *= mdec;
        } else {
            x = 0.5f * blep_saw(p0, v->inc[0]) + 0.42f * blep_saw(p1, v->inc[1]) + 0.45f * blep_sq(p2, v->inc[2]);
        }
        p0 += v->inc[0];
        if (p0 >= 1.0f)
            p0 -= 1.0f;
        p1 += v->inc[1];
        if (p1 >= 1.0f)
            p1 -= 1.0f;
        p2 += v->inc[2];
        if (p2 >= 1.0f)
            p2 -= 1.0f;
        /* the ladder: four one-poles, the input through a tanh with the resonance fed back */
        x = fm_tanhf(1.2f * (x - 4.0f * v->k * s3));
        s0 += v->g * (x - s0);
        s1 += v->g * (s0 - s1);
        s2 += v->g * (s1 - s2);
        s3 += v->g * (s2 - s3);
        y = s3 * (1.0f + 1.6f * v->k) * a * g;
        mixl[j] += y;
        mixr[j] += y * 0.78f;                    /* the dry note a little left */
        esend[j] += y * echo_send;
        rsend[j] += y * bass_rev;
    }
    v->ph[0] = p0;
    v->ph[1] = p1;
    v->ph[2] = p2;
    v->s[0] = fm_flush(s0);
    v->s[1] = fm_flush(s1);
    v->s[2] = fm_flush(s2);
    v->s[3] = fm_flush(s3);
    v->amp = a;
    v->aenv = ae;
    v->fenv = fe;
    v->fmenv = me;
    if (v->gate <= 0 && a < 1e-4f)
        v->on = 0;
}

static void pad_bus(uint32_t n)                 /* a low-pass, then the phaser */
{
    const float fc = fm_clampf(900.0f * fm_exp2f(bright * 0.035f + 0.7f * arcm + (pcolour == 2 ? 0.6f * mod[4].v * motion : 0.0f)),
                               300.0f, 12000.0f);
    const float c = 1.0f - fm_expf(-FM_TWO_PI * fc * (1.0f / EN_SR));
    const float d = phaser_d, fb = 0.25f + 0.45f * d, ga = FXA(GATE), gstep = step_len > 1.0f ? 1.0f / step_len : 0.0f;
    uint32_t j;
    int k, ch;
    for (j = 0; j < n; j++) {
        float in[2], o[2];
        if (!(j & 31u)) {                        /* the sweep: 0.12 .. 0.5 Hz, 250 Hz .. 2.2 kHz */
            ph_lfo += (0.12f + 0.25f * motion + 0.12f * mod[3].v * motion) * 32.0f / EN_SR;
            if (ph_lfo >= 1.0f)
                ph_lfo -= 1.0f;
            for (ch = 0; ch < 2; ch++) {
                float lf = 0.5f + 0.5f * sine(ph_lfo + 0.25f * (float)ch), f = 250.0f * fm_exp2f(3.1f * lf);
                float t = fm_tanf(FM_PI * f * (1.0f / EN_SR));
                ph_a[ch] = (t - 1.0f) / (t + 1.0f);
            }
        }
        plp[0] += c * (padl[j] - plp[0]);
        plp[1] += c * (plp[0] - plp[1]);
        plp[2] += c * (padr[j] - plp[2]);
        plp[3] += c * (plp[2] - plp[3]);
        in[0] = plp[1];
        in[1] = plp[3];
        for (ch = 0; ch < 2; ch++) {
            float x = in[ch] + fb * ph_fb[ch], a = ph_a[ch];
            for (k = 0; k < 4; k++) {            /* y = a x + s; s = x - a y */
                float y = a * x + ph_s[ch][k];
                ph_s[ch][k] = x - a * y;
                x = y;
            }
            ph_fb[ch] = fm_flush(x);
            o[ch] = in[ch] + d * x;
        }
        {   /* the Gate effect: the pad off for the second half of each sixteenth; Drop: out */
            gate_g += ((1.0f - ga * (gate_ph > 0.5f ? 1.0f : 0.0f)) - gate_g) * 0.03f;
            gate_ph += gstep;
            o[0] *= gate_g * drop_g;
            o[1] *= gate_g * drop_g;
        }
        mixl[j] += o[0] * 0.8f;
        mixr[j] += o[1] * 0.8f;
        rsend[j] += (o[0] + o[1]) * pad_rev;
        esend[j] += (o[0] + o[1]) * pad_echo;
    }
    for (k = 0; k < 4; k++) {
        plp[k] = fm_flush(plp[k]);
        ph_s[0][k] = fm_flush(ph_s[0][k]);
        ph_s[1][k] = fm_flush(ph_s[1][k]);
    }
}

static void render_pad(uint32_t n)
{
    int i;
    float lvl = 0.0f, gate = pad_gate * ((layers & L_PAD) ? 1.0f : 0.0f);
    uint32_t j;
    const float g = pad_vol * 0.36f;
    for (j = 0; j < n; j++)
        padl[j] = padr[j] = 0.0f;
    pad_gain += (gate - pad_gain) * fm_minf((float)n / ((gate > 0.0f ? 0.8f : 1.5f) * EN_SR), 1.0f);
    if (pad_gain < 1e-4f && gate <= 0.0f) {
        for (i = 0; i < EN_NPAD; i++)
            pad[i].amp = 0.0f;
        en_pad_level = 0.0f;
        pad_bus(n);
        return;
    }
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        float p0 = v->ph[0], p1 = v->ph[1], amp = v->amp, tgt = 0.0f, gl, gr, f[4];
        for (j = 0; j < 4; j++)
            f[j] = v->fs[j];
        gl = fm_cosf((PAD_PAN[i] + 1.0f) * (FM_PI / 4.0f)) * g;
        gr = fm_sinf((PAD_PAN[i] + 1.0f) * (FM_PI / 4.0f)) * g;
        for (j = 0; j < n; j++) {
            float x;
            if (!(j & 31u)) {
                pad_coefs(v, i);
                tgt = pad_gain * v->hf;
            }
            if (pcolour == 1) {                  /* a choir: a soft triangle and a little saw through two formants */
                float bp1, bp2, l1;
                x = (p0 < 0.5f ? 4.0f * p0 - 1.0f : 3.0f - 4.0f * p0) + 0.4f * blep_saw(p1, v->inc[1]);
                l1 = svf(v->c1, f, x, &bp1);
                svf(v->c2, f + 2, x, &bp2);
                x = (bp1 * v->c1[3] + 0.5f * bp2 * v->c2[3] + 0.35f * l1) * 0.5f;
            } else {                             /* strings and saws: two saws */
                x = 0.5f * (blep_saw(p0, v->inc[0]) + blep_saw(p1, v->inc[1]));
            }
            p0 += v->inc[0];
            if (p0 >= 1.0f)
                p0 -= 1.0f;
            p1 += v->inc[1];
            if (p1 >= 1.0f)
                p1 -= 1.0f;
            amp += (tgt - amp) * 0.004f;
            x *= amp;
            padl[j] += x * gl;
            padr[j] += x * gr;
        }
        for (j = 0; j < 4; j++)
            v->fs[j] = fm_flush(f[j]);
        v->ph[0] = p0;
        v->ph[1] = p1;
        v->amp = amp;
        lvl = fm_maxf(lvl, amp);
    }
    en_pad_level = lvl;
    pad_bus(n);
}

static void render_lead(uint32_t n)
{
    lead_t *v = &lead;
    const lvoice_t *L = &LV[lvoice];
    uint32_t j;
    const float g = lead_vol * 0.45f * drop_g * L->gain;
    const int fm = lvoice >= LV_FMLEAD && lvoice <= LV_FMREED;
    float p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], env = v->env, fe = v->fe, s[2] = {v->s[0], v->s[1]}, fs[6];
    float idx = 0.0f;
    if (!v->on && env < 1e-5f) {
        v->env = 0.0f;
        return;
    }
    for (j = 0; j < 6; j++)
        fs[j] = v->fs[j];
    for (j = 0; j < n; j++) {
        float x, bp, y;
        if (!(j & 31u)) {                        /* the pitch: glide, vibrato; the filters */
            float vib, f, gk = fm_minf(glide_k, L->glide > 0.0f ? 1.0f / (L->glide * EN_SR) : glide_k), fc;
            v->pitch += (v->target - v->pitch) * fm_minf(32.0f * gk * 4.0f, 1.0f);
            v->vib_t += 32.0f / EN_SR;
            if (v->vib_t > 600.0f)
                v->vib_t -= 500.0f;              /* whole turns of the vibrato: no jump */
            vib = fm_clampf((v->vib_t - L->vdelay) * 2.5f, 0.0f, 1.0f) * L->vib * sine(v->vib_t * 5.5f);
            if (lvoice == LV_SYNTHI)
                vib += 0.08f * mod[2].v;         /* the Synthi drifts */
            f = mtof(v->pitch + vib);
            v->inc = f * (1.0f / EN_SR);
            fc = fm_clampf(f * L->cut * fm_exp2f(L->fenv * fe + 0.5f * arcm) * (0.35f + 1.3f * ltone), 120.0f, 15000.0f);
            if (lvoice == LV_MINI) {             /* a ladder, as the bass's */
                v->lg = 1.0f - fm_expf(-FM_TWO_PI * fc * (1.0f / EN_SR));
                v->lk = L->reso * 0.9f;
            } else {
                svf_coefs(v->c, fc, 2.0f - 1.85f * L->reso);
            }
            if (lvoice == LV_CHOIR) {            /* three vowel resonances: Tone from "oo" to "ee" */
                static const float VW[3][3] = {{320.0f, 800.0f, 2500.0f}, {730.0f, 1090.0f, 2440.0f}, {270.0f, 2290.0f, 3010.0f}};
                float t = fm_clampf(ltone, 0.0f, 1.0f) * 1.999f;
                int a = (int)t, k;
                t -= (float)a;
                for (k = 0; k < 3; k++)
                    svf_coefs(v->fc[k], VW[a][k] + (VW[a + 1][k] - VW[a][k]) * t, 0.12f + 0.06f * (float)k);
            } else if (lvoice == LV_ROBOT) {
                float o = 0.5f + 0.5f * sine((float)(v->note % 7) * 0.13f);
                svf_coefs(v->fc[0], 450.0f + 350.0f * o, 0.25f);
                svf_coefs(v->fc[1], 1300.0f + 900.0f * o, 0.2f);
            }
            if (fm)                              /* the index: Lead's falls, Brass's follows the level, Reed's stays */
                idx = L->idx * (lvoice == LV_FMBRASS ? 0.3f + 0.7f * env : L->idec + (1.0f - L->idec) * fe) *
                      (0.6f + 0.8f * ltone) * (1.0f / FM_TWO_PI);
        }
        if (v->gate > 0) {                       /* attack, then decay to the sustain */
            v->gate--;
            if (!v->stage) {
                env += (1.08f - env) * v->ka;
                if (env >= 1.0f) {
                    env = 1.0f;
                    v->stage = 1;
                }
            } else {
                env += (L->sus - env) * v->kd;
            }
        } else {
            env -= env * v->kr;
            v->on = env > 1e-4f;
        }
        fe -= fe * v->kf;
        switch (lvoice) {
        case LV_MINI:                            /* three saws, one an octave down */
            x = 0.42f * blep_saw(p0, v->inc) + 0.36f * blep_saw(p1, v->inc * 1.0045f) +
                0.3f * blep_saw(p2, v->inc * 0.5f);
            break;
        case LV_ODYSSEY: {                       /* a saw and a narrow pulse */
            float u = p1 + 0.38f;
            if (u >= 1.0f)
                u -= 1.0f;
            x = 0.55f * blep_saw(p0, v->inc) + 0.3f * (blep_saw(p1, v->inc * 1.003f) - blep_saw(u, v->inc * 1.003f));
            break;
        }
        case LV_SYNTHI:                          /* a triangle and a square */
            x = 0.75f * (p0 < 0.5f ? 4.0f * p0 - 1.0f : 3.0f - 4.0f * p0) + 0.35f * blep_sq(p1, v->inc);
            break;
        case LV_FLUTE:                           /* a sine, a little triangle, breath */
            x = sine(p0) + 0.12f * (p0 < 0.5f ? 4.0f * p0 - 1.0f : 3.0f - 4.0f * p0) + 0.1f * nz() * (0.3f + fe);
            break;
        case LV_THEREMIN:
            x = sine(p0) + 0.06f * sine(2.0f * p0);
            break;
        case LV_CHOIR:
        case LV_ROBOT:
            x = lvoice == LV_CHOIR ? 0.5f * (blep_saw(p0, v->inc) + blep_saw(p1, v->inc * 1.006f)) : blep_sq(p0, v->inc) * 1.2f;
            break;
        default:                                 /* FM: one modulator on one carrier */
            x = sine(p0 + sine(p1) * idx + 4.0f);
            break;
        }
        p0 += v->inc;
        if (p0 >= 1.0f)
            p0 -= 1.0f;
        p1 += fm ? v->inc * L->ratio : v->inc * 1.004f;
        if (p1 >= 1.0f)
            p1 -= 1.0f;
        p2 += v->inc * 0.5f;
        if (p2 >= 1.0f)
            p2 -= 1.0f;
        if (lvoice == LV_MINI) {
            float t = fm_tanhf(1.3f * (x - 4.0f * v->lk * v->ls[3]));
            v->ls[0] += v->lg * (t - v->ls[0]);
            v->ls[1] += v->lg * (v->ls[0] - v->ls[1]);
            v->ls[2] += v->lg * (v->ls[1] - v->ls[2]);
            v->ls[3] += v->lg * (v->ls[2] - v->ls[3]);
            y = v->ls[3] * (1.0f + 1.4f * v->lk);
        } else if (lvoice == LV_CHOIR) {
            float b0, b1, b2;
            svf(v->fc[0], fs, x, &b0);
            svf(v->fc[1], fs + 2, x, &b1);
            svf(v->fc[2], fs + 4, x, &b2);
            y = (b0 + 0.7f * b1 + 0.35f * b2) * 0.22f;
        } else {
            y = svf(v->c, s, x, &bp);
            if (lvoice == LV_ROBOT) {            /* a robot: the square through two formants, a vocoder's edge */
                float b1, b2;
                svf(v->fc[0], fs, y, &b1);
                svf(v->fc[1], fs + 2, y, &b2);
                y = (b1 * 2.2f + b2 * 1.6f) * 0.11f;
            }
        }
        y *= env * g;
        mixl[j] += y * 0.85f;
        mixr[j] += y;
        rsend[j] += y * lead_rev;
        esend[j] += y * lead_echo;
    }
    v->ph[0] = p0;
    v->ph[1] = p1;
    v->ph[2] = p2;
    v->env = env;
    v->fe = fe;
    v->s[0] = fm_flush(s[0]);
    v->s[1] = fm_flush(s[1]);
    for (j = 0; j < 4; j++)
        v->ls[j] = fm_flush(v->ls[j]);
    for (j = 0; j < 6; j++)
        v->fs[j] = fm_flush(fs[j]);
}

static void render_hat(drum_t *v, uint32_t n, float g, float pl, float pr)
{
    uint32_t j;
    int k;
    float x1, x2, amp, att;
    if (!v->on)
        return;
    x1 = v->x1, x2 = v->x2, amp = v->amp, att = v->att;
    for (j = 0; j < n; j++) {
        float in = 0.0f, hp, x;
        for (k = 0; k < 4; k++) {
            v->mph[k] += HAT_INC[k];
            if (v->mph[k] >= 1.0f)
                v->mph[k] -= 1.0f;
            in += v->mph[k] < 0.5f ? 0.25f : -0.25f;
        }
        in = 0.4f * in + 0.7f * nz();
        hp = in - x2 - 0.8f * x1;
        x1 += hat_f * hp;
        x2 += hat_f * x1;
        x = hp * amp * att * g;
        mixl[j] += x * pl;
        mixr[j] += x * pr;
        rsend[j] += x * 0.15f * drum_rev;
        amp *= v->adec;
        att += (1.0f - att) * 0.3f;
    }
    v->x1 = fm_flush(x1);
    v->x2 = fm_flush(x2);
    v->amp = amp;
    v->att = att;
    v->on = amp > 0.0005f;
}

static void render_drums(uint32_t n)
{
    uint32_t j;
    const float g = drum_vol * 1.25f;              /* (1.0 until 0.2: the drums about 2 dB up in the mix) */
    drum_t *v = &drum[D_KICK];
    if (v->on) {
        float ph = v->ph, inc = v->inc, amp = v->amp, cl = v->tone;
        for (j = 0; j < n; j++) {
            float x = (sine(ph) + 0.35f * cl * nz()) * amp * g * 0.95f;
            mixl[j] += x;
            mixr[j] += x;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            inc += (v->inc_end - inc) * 0.0009f;
            amp *= v->adec;
            cl *= 0.93f;
        }
        v->ph = ph, v->inc = inc, v->amp = amp, v->tone = cl;
        v->on = amp > 0.0005f;
    }
    v = &drum[D_SNARE];
    if (v->on) {
        float ph = v->ph, amp = v->amp, tn = v->tone, s[2] = {v->s[0], v->s[1]};
        for (j = 0; j < n; j++) {
            float bp, x;
            svf(v->c, s, nz(), &bp);
            x = (bp * 1.6f * amp + 0.5f * sine(ph) * tn) * g * drop_g * 0.42f;
            mixl[j] += x * 0.95f;
            mixr[j] += x;
            rsend[j] += x * 0.5f * drum_rev;
            esend[j] += x * 0.08f * drum_echo;
            ph += v->inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            amp *= v->adec;
            tn *= v->tdec;
        }
        v->ph = ph, v->amp = amp, v->tone = tn;
        v->s[0] = fm_flush(s[0]);
        v->s[1] = fm_flush(s[1]);
        v->on = amp > 0.0005f;
    }
    render_hat(&drum[D_HAT], n, g * drop_g * hat_lvl * 0.16f, 0.65f, 1.0f);
    render_hat(&drum[D_OHAT], n, g * drop_g * hat_lvl * 0.13f, 1.0f, 0.7f);
    v = &drum[D_TOM];
    if (v->on) {
        float ph = v->ph, inc = v->inc, amp = v->amp;
        for (j = 0; j < n; j++) {
            float x = sine(ph) * amp * g * drop_g * 0.5f;
            mixl[j] += x * 1.0f;
            mixr[j] += x * 0.75f;
            rsend[j] += x * 0.5f * drum_rev;
            esend[j] += x * 0.15f * drum_echo;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            inc += (v->inc_end - inc) * 0.00012f;   /* the syndrum's fall */
            amp *= v->adec;
        }
        v->ph = ph, v->inc = inc, v->amp = amp;
        v->on = amp > 0.0005f;
    }
}

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    const float eg = (sec_now()->flags & F_BREAK) ? 1.4f : 1.0f;
    /* Reverb and Wash: measured on Feel (tests: the mix's RMS held against without), Reverb about +1.5 dB, Wash about
     * level with the dry mix it replaces (they were +5.5 and +3.1 dB: 0.8 and 1.0 / 0.6) */
    /* Throw, Swell, Wash and Tape stop send the mix into the echo; Wash and Tape stop take the dry sound down (Tape stop:
     * all of it, so the music itself winds down, through the tape) */
    const float thr = 1.2f * FXA(THROW) + 1.3f * FXA(SWELL) + 0.7f * FXA(WASH) + 1.6f * FXA(TAPESTOP),
                rx = 0.32f * FXA(REVERB) + 0.42f * FXA(WASH);
    const float dry = (1.0f - 0.85f * FXA(WASH)) * (1.0f - 0.95f * FXA(TAPESTOP)), rin = 1.0f - FXA(FREEZE);
    const float alp = FXA(LOWPASS), ahp = FXA(HIGHPASS), aph = FXA(PHASER), ast = FXA(STUTTER);
    float clp = 0.0f, chp = 0.0f;
    if (alp > 0.001f)                            /* 18 kHz down to 350 Hz */
        clp = 1.0f - fm_expf(-FM_TWO_PI * 18000.0f * fm_exp2f(-5.7f * alp) * (1.0f / EN_SR));
    if (ahp > 0.001f)                            /* 20 Hz up to 1.3 kHz */
        chp = 1.0f - fm_expf(-FM_TWO_PI * 20.0f * fm_exp2f(6.0f * ahp) * (1.0f / EN_SR));
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = 0.0f;
    render_pad(n);
    render_bass(n);
    render_lead(n);
    render_drums(n);
    for (j = 0; j < n; j++) {
        float wl, wr, l, r, el, er, dm = (mixl[j] + mixr[j]) * 0.5f;
        echoes((esend[j] + dm * thr) * eg, &el, &er);
        reverb(((rsend[j] + (el + er) * 0.25f) * rev_send * 0.9f + dm * rx) * rin, &wl, &wr);
        l = mixl[j] * dry + 0.8f * el + 1.2f * wl;
        r = mixr[j] * dry + 0.8f * er + 1.2f * wr;
        if (stut_on == 1) {                      /* Stutter: a sixteenth caught ... */
            stut[2 * stut_pos] = (int16_t)fm_clampf(l * 8000.0f, -32767.0f, 32767.0f);
            stut[2 * stut_pos + 1] = (int16_t)fm_clampf(r * 8000.0f, -32767.0f, 32767.0f);
            if (++stut_pos >= stut_len) {
                stut_on = 2;
                stut_pos = 0;
            }
        } else if (stut_on == 2) {               /* ... and looped */
            l += ((float)stut[2 * stut_pos] * (1.0f / 8000.0f) - l) * ast;
            r += ((float)stut[2 * stut_pos + 1] * (1.0f / 8000.0f) - r) * ast;
            if (++stut_pos >= stut_len)
                stut_pos = 0;
        }
        if (clp > 0.0f) {
            m_lp[0] += clp * (l - m_lp[0]);
            m_lp[1] += clp * (m_lp[0] - m_lp[1]);
            m_lp[2] += clp * (r - m_lp[2]);
            m_lp[3] += clp * (m_lp[2] - m_lp[3]);
            l = m_lp[1];
            r = m_lp[3];
        } else {
            m_lp[0] = m_lp[1] = l;
            m_lp[2] = m_lp[3] = r;
        }
        if (chp > 0.0f) {
            m_hp[0] += chp * (l - m_hp[0]);
            m_hp[1] += chp * (r - m_hp[1]);
            l -= m_hp[0];
            r -= m_hp[1];
        } else {
            m_hp[0] = m_hp[1] = 0.0f;
        }
        if (aph > 0.001f) {                      /* the Phaser effect: on everything, six stages, fed back */
            float in[2] = {l, r};
            int ch, k;
            if (!(j & 31u)) {
                m_phlfo += 0.45f * 32.0f / EN_SR;
                if (m_phlfo >= 1.0f)
                    m_phlfo -= 1.0f;
                for (ch = 0; ch < 2; ch++) {
                    float lf = 0.5f + 0.5f * sine(m_phlfo + 0.3f * (float)ch), t;
                    t = fm_tanf(FM_PI * 200.0f * fm_exp2f(3.6f * lf) * (1.0f / EN_SR));
                    m_pha[ch] = (t - 1.0f) / (t + 1.0f);
                }
            }
            for (ch = 0; ch < 2; ch++) {
                float x = in[ch] + 0.55f * m_phfb[ch];
                for (k = 0; k < 6; k++) {
                    float y = m_pha[ch] * x + m_ph[ch][k];
                    m_ph[ch][k] = fm_flush(x - m_pha[ch] * y);
                    x = y;
                }
                m_phfb[ch] = fm_flush(x);
                in[ch] += aph * x;
            }
            l = in[0];
            r = in[1];
        }
        l *= 0.42f * gain;
        r *= 0.42f * gain;
        out[2 * j] = (int32_t)(fm_tanhf(l) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r) * 8300000.0f);
    }
    for (j = 0; j < 4; j++)
        m_lp[j] = fm_flush(m_lp[j]);
    m_hp[0] = fm_flush(m_hp[0]);
    m_hp[1] = fm_flush(m_hp[1]);
}

/* the arc: a slow tide, minutes long; Motion sets how far it goes (arc, arc_tgt, arc_left above) */
static void control(uint32_t n)
{
    int i;
    mods_step(n);
    if (arc_left <= n) {
        float r = frand() * 2.0f - 1.0f;
        arc_tgt = r * (0.6f + 0.4f * frand());
        arc_left = (uint32_t)((40.0f + 80.0f * frand()) * EN_SR);
    } else {
        arc_left -= n;
    }
    if (en_playing)
        arc += (fm_clampf(arc_tgt, -1.0f, 1.0f) - arc) * (float)n * (1.0f / (30.0f * EN_SR));
    arcm = arc * motion;
    en_arc = arcm;
    for (i = 0; i < EN_NFX; i++) {               /* the effects ease in (30 ms) and out (a quarter second) */
        float t = (float)(fx_mask >> i & 1u), tau = i == FX_STUTTER ? 0.006f : t > fx_a[i] ? 0.03f : 0.25f;
        fx_a[i] += (t - fx_a[i]) * fm_minf((float)n / (tau * EN_SR), 1.0f);
        if (fx_a[i] < 1e-4f && !t)
            fx_a[i] = 0.0f;
    }
    if ((fx_mask >> FX_STUTTER & 1u) && !stut_on) {   /* a sixteenth from now on, then round */
        stut_len = (int)fm_minf(step_len, (float)STUT_N);
        stut_pos = 0;
        stut_on = stut_len > 32 ? 1 : 0;
    } else if (!(fx_mask >> FX_STUTTER & 1u) && FXA(STUTTER) <= 0.0f) {
        stut_on = 0;
    }
    echo_len_t = fm_minf(echo_base * (1.0f + FXA(TIME)), (float)(TAPE_N - 8) * 0.5f);
    ts_on = (uint8_t)(fx_mask >> FX_TAPESTOP & 1u);
    echo_glide = ts_on || FXA(TAPESTOP) > 0.02f ? 0.0006f : 0.002f;   /* (let go: the tape back up to speed, ~0.15 s) */
    fb_eff = fm_maxf(echo_fb, 0.985f * FXA(SWELL));
    pl_decay = 0.72f + 0.27f * FXA(FREEZE);
    drop_g = 1.0f - FXA(DROP);
    en_bass_glow *= 0.94f;
    en_lead_glow *= 0.96f;
    for (i = 0; i < EN_NDRUM; i++)
        en_drum_glow[i] *= 0.92f;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    drain();
    if (en_ext && ++ext_age > 100u)
        en_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        int timed = en_playing && !en_ext;
        if (timed) {
            if (pulse_left <= 0.0f) {
                clock_pulse();
                pulse_left += step_len * (1.0f / 6.0f);
            }
            if ((float)k > pulse_left)
                k = (uint32_t)pulse_left + 1u;
        }
        if (k > n)
            k = n;
        {
            int32_t w = drums_wait();
            if (w > 0 && (uint32_t)w < k)
                k = (uint32_t)w;
        }
        if (timed)
            pulse_left -= (float)k;
        control(k);
        render_chunk(out, k, gain);
        drums_wait_step((int32_t)k);
        out += 2 * k;
        n -= k;
    }
}

int en_step_pc(int step)                         /* (UI) a white key's pitch class now: the key, up the scale */
{
    return (key + scale_steps[(step < 0 ? 0 : step) % 7]) % 12;
}
