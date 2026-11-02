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
        proj.leds > 2)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pars_ok(proj.slot[i].par))
            return 0;
    return 1;
}

int project_load(void)
{
    int n, i;
    memset(&proj, 0, sizeof proj);
    n = plat_store_load(OBJ_PROJ, &proj, sizeof proj);
    if (n >= 8 && proj.magic == PROJ_MAGIC_OLD)
        proj.magic = PROJ_MAGIC;
    if (n == (int)PROJ_V2_SIZE && proj.magic == PROJ_MAGIC && proj.format == 2u) {   /* format 2: the songs' own chords */
        proj.format = PROJ_FORMAT;
        for (i = 0; i < NPIECES; i++)
            chords_recipe(i);
    }
    if (n >= 8 && project_valid()) {
        for (i = 0; i < NPIECES; i++)
            if (!chords_ok(i))
                chords_recipe(i);
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
}

void project_apply(void)
{
    int i, k;
    for (i = 0; i < NPIECES; i++)
        for (k = 0; k < EN_NPROG * EN_PROG_MAX; k++)
            en_prog_set(i, k / EN_PROG_MAX, k % EN_PROG_MAX, proj.chords[i][k]);
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    en_song(proj.piece);                         /* after the values: the song starts with them */
}
