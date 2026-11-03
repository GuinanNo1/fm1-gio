/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, Ian Guinan */
/* GIO-1 project: one flash object (OBJ_PROJ). A project of another format, or none, gives the defaults. Part of the
 * unity build, after app.h.
 *
 * The songs (their recipes are the engine's, en_song_recipe): each keeps its own settings as you leave it; Tune and
 * the MIDI settings are global. */
project_t proj;
typedef char proj_fits_its_sector[sizeof(project_t) <= 3840u ? 1 : -1];   /* storage.c's ST_PAYLOAD_MAX */

const char *piece_name(int i) { return en_song_name(i); }

static int global_par(int p) { return p == P_TUNE || p == P_MIDI || p == P_MCHAN || p == P_MCLOCK || p == P_MCLKOUT; }

static void recipe(int i, slot_t *s) { en_song_recipe(i, s->par); }

static void chords_recipe(int i)                 /* song i's chord changes, its own */
{
    int k;
    for (k = 0; k < EN_NPROG * EN_PROG_MAX; k++)
        proj.chords[i][k] = en_prog_recipe(i, k / EN_PROG_MAX, k % EN_PROG_MAX);
}

static void patterns_recipe(int i)               /* song i's bass patterns, the shapes as they are; its drums as made */
{
    int a, k;
    for (a = 1; a <= EN_NSHAPES; a++) {
        for (k = 0; k < EN_STEPS; k++)
            proj.bass[i][a - 1][k] = en_bstep_recipe(a, k);
        proj.bacc[i][a - 1] = en_bacc_recipe(a);
    }
    for (k = 0; k < EN_NDRUM; k++)
        proj.drums[i][k] = 0;
    proj.drums_edited = (uint16_t)(proj.drums_edited & ~(1u << i));
}

static int patterns_ok(int i)                    /* every step a note the Bassline page gives, no tie past the bar */
{
    int a, k;
    for (a = 0; a < EN_NSHAPES; a++)
        for (k = 0; k < EN_STEPS; k++)
            if (EN_BS_NOTE(proj.bass[i][a][k]) >= EN_NBN || EN_BS_LEN(proj.bass[i][a][k]) > EN_STEPS - k)
                return 0;
    return 1;
}

static int chords_ok(int i)                      /* a length the Chords page gives; after an end, nothing */
{
    int p, k;
    for (p = 0; p < EN_NPROG; p++)
        for (k = 0; k < EN_PROG_MAX; k++) {
            uint16_t c = proj.chords[i][p * EN_PROG_MAX + k];
            int b = EN_CH_BEATS(c);
            if (EN_CH_R(c) > 11 || EN_CH_Q(c) >= EN_NQUAL || (b && b != 2 && b != 4 && b != 8 && b != 12 && b != 16 &&
                                                               b != 32))
                return 0;
            if (k && !EN_CH_BEATS(proj.chords[i][p * EN_PROG_MAX + k - 1]) && c)
                return 0;
            if (!k && !b && en_prog_recipe(i, p, 0))
                return 0;
        }
    return 1;
}

static void take(const slot_t *s)                /* a song's settings into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
}

static void keep(void)                           /* the song playing, into its slot */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = proj.par[p];
}

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &proj.slot[i]);
        chords_recipe(i);
        patterns_recipe(i);
    }
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info(i)->def;
    take(&proj.slot[0]);
    proj.leds = 1;
}

static int pars_ok(const int16_t *par)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo || par[i] > p->hi)
            return 0;
    }
    return 1;
}

static int project_valid(void)
{
    int i;
    if (proj.magic != PROJ_MAGIC || proj.format != PROJ_FORMAT || proj.piece >= NPIECES || !pars_ok(proj.par) ||
        proj.leds > 2 || proj.nomirror > 1)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pars_ok(proj.slot[i].par))
            return 0;
    return 1;
}

static void from_v3(int n)                       /* a format 2 or 3 project (in proj's bytes) as format 5 */
{
    static project_v3_t o;
    slot_t r;
    int i, p;
    memcpy(&o, &proj, sizeof o);
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.leds = o.leds;
    proj.piece = o.piece;
    for (i = 0; i < NPIECES; i++) {               /* each song: its 36 as left, the FX pages' from its recipe */
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < NPAR_V3 ? o.slot[i].par[p] : r.par[p];
        if (n == (int)sizeof o)
            memcpy(proj.chords[i], o.chords[i], sizeof proj.chords[i]);
        else
            chords_recipe(i);                     /* format 2: the songs' own chords */
        patterns_recipe(i);
    }
    recipe(o.piece < NPIECES ? o.piece : 0, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < NPAR_V3 ? o.par[p] : r.par[p];
}

int project_load(void)
{
    int n, i;
    memset(&proj, 0, sizeof proj);
    n = plat_store_load(OBJ_PROJ, &proj, sizeof proj);
    if (n >= 8 && proj.magic == PROJ_MAGIC_OLD)
        proj.magic = PROJ_MAGIC;
    if (proj.magic == PROJ_MAGIC && ((proj.format == 3u && n == (int)sizeof(project_v3_t)) ||
                                     (proj.format == 2u && n == (int)PROJ_V2_SIZE))) {
        from_v3(n);
        n = (int)sizeof proj;
    } else if (proj.magic == PROJ_MAGIC && proj.format == 4u && n == (int)PROJ_V4_SIZE) {
        for (i = 0; i < NPIECES; i++)            /* format 4: as it was, and the patterns and drums each song's own */
            patterns_recipe(i);
        proj.format = PROJ_FORMAT;
        n = (int)sizeof proj;
    }
    if (n == (int)sizeof proj && project_valid()) {   /* (its own size: format 4 grew before its release) */
        for (i = 0; i < NPIECES; i++) {
            if (!chords_ok(i))
                chords_recipe(i);
            if (!patterns_ok(i))
                patterns_recipe(i);
        }
        return 0;
    }
    project_defaults();
    return -1;
}

int project_save(void)
{
    keep();
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece]);
}

void project_piece_reset(void)
{
    slot_t s;
    recipe(proj.piece, &s);
    take(&s);
    chords_recipe(proj.piece);
    patterns_recipe(proj.piece);
}

void project_apply(void)
{
    int i, k;
    for (i = 0; i < NPIECES; i++) {
        for (k = 0; k < EN_NPROG * EN_PROG_MAX; k++)
            en_prog_set(i, k / EN_PROG_MAX, k % EN_PROG_MAX, proj.chords[i][k]);
        for (k = 1; k <= EN_NSHAPES; k++)
            en_bshape_put(i, k, proj.bass[i][k - 1], proj.bacc[i][k - 1]);
        en_drums_put(i, (int)(proj.drums_edited >> i & 1u), proj.drums[i]);
    }
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    en_song(proj.piece);                         /* after the values: the song starts with them */
}
