/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, Ian Guinan */
/* GIO-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host simulator
 * includes it too. From AMB-1's: the input, the knobs' acceleration, the help cards, the drawing, the autosave and
 * the screen reader.
 *
 *   white keys     the lead, a step of the song's scale each (the chord's notes lit); a call of three notes or
 *                  more and a pause: the lead answers, and takes it as its theme
 *   black keys     eleven of the effects, each while held
 *   FX             the FX pages, one a press: BASS FX, PAD FX, LEAD FX, DRUMS FX (each instrument's echo and reverb,
 *                  the one echo's time and feedback), SPACE; on each, the sixteen effects on the white keys (REC held:
 *                  the same, on any page, for as long)
 *   EDIT           the Chords page: the section's chord changes, a chord at a time (its root, type and length)
 *   SEL            the next section, on the next bar
 *   PLAY           plays / stops
 *   REC            a tap: HOLD, the section repeats until let go  OCT- / OCT+   the bass's octave
 *   ALGORITHM      the bass's pattern (Song: the song's own)      PRESETS       the song
 *   HOME ENV ARP LFO SEQ FX GLO   the pages: SONG (again: MIXER), BASS, LEAD (again: LEAD 2), PAD, DRUMS, the FX pages,
 *                  SETUP (again: MIDI, MIRROR); SELECT
 *                  steps through them (with HOME held: the tempo)
 *   KNOB 1-4       the page's four values
 *   SAVE           saves (it also saves by itself a few seconds after a change, while stopped);
 *                  held 1.5 s: the song back to its recipe */
#include "fastmath.h"

enum { V_SONG, V_MIXER, V_BASS, V_LEAD, V_LEAD2, V_PAD, V_DRUMS, V_BASSFX, V_PADFX, V_LEADFX, V_DRUMFX, V_SPACE, V_SETUP,
       V_MIDI, V_MIRROR, V_CHORDS, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Song", "Mixer", "Bass", "Lead", "Lead 2", "Pad", "Drums", "Bass FX",
                                              "Pad FX", "Lead FX", "Drums FX", "Space", "Setup", "MIDI", "Mirror",
                                              "Chords"};
/* FX: each instrument's FX page, the Space page, and round again. On each, the effects grid: the white keys play the
 * sixteen effects (as REC held does anywhere) */
static const uint8_t FX_CYCLE[] = {V_BASSFX, V_PADFX, V_LEADFX, V_DRUMFX, V_SPACE};
#define GRID_PAGE(v) ((v) >= V_BASSFX && (v) <= V_SPACE)
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_HOME, B_ENV, B_ARP, B_ARP, B_LFO, B_SEQ, B_FX, B_FX, B_FX, B_FX, B_FX,
                                         B_GLO, B_GLO, B_GLO, B_EDIT};
enum { K_LEDS = 100, K_CPU, K_NONE, K_CSLOT, K_CROOT, K_CTYPE, K_CLEN, K_MIRROR };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_TEMPO, P_EVOLVE, P_MOTION, P_ARRANGE},
    {P_BASS, P_PAD, P_LEAD, P_DRUMS},             /* the Mixer: each instrument's Level (its own page's KNOB 1) */
    {P_BASS, P_CUTOFF, P_RESO, P_DECAY},
    {P_LEAD, P_LVOICE, P_BUSY, P_GLIDE},
    {P_LECHO, P_LREV, P_LOCT, P_LTONE},
    {P_PAD, P_PCOLOUR, P_BRIGHT, P_PHASER},
    {P_DRUMS, P_FILLS, P_HATS, P_SWING},
    {P_ECHO, P_BREV, P_ETIME, P_FEEDBACK},        /* the FX pages: the instrument's echo and reverb, the echo's own */
    {P_PECHO, P_PREV, P_ETIME, P_FEEDBACK},       /* time and feedback (one echo, shared: the same on each) */
    {P_LECHO, P_LREV, P_ETIME, P_FEEDBACK},
    {P_DECHO, P_DREV, P_ETIME, P_FEEDBACK},
    {P_ECHO, P_ETIME, P_FEEDBACK, P_REVERB},
    {P_ROOT, P_TUNE, P_MIDI, K_LEDS},
    {P_MIDI, P_MCHAN, P_MCLOCK, P_MCLKOUT},
    {K_MIRROR, K_NONE, K_NONE, K_NONE},           /* the screen mirror: on or off (the FM-1's, not a song's) */
    {K_CSLOT, K_CROOT, K_CTYPE, K_CLEN},
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Tempo", "Evolve", "Motion", "Arrange"},
    {"Bass", "Pad", "Lead", "Drums"},
    {"Level", "Cutoff", "Emphasis", "Decay"},
    {"Level", "Voice", "Busy", "Glide"},
    {"Echo", "Reverb", "Octave", "Tone"},
    {"Level", "Colour", "Bright", "Phaser"},
    {"Level", "Fills", "Hats", "Swing"},
    {"Echo", "Reverb", "Time", "Feedback"},
    {"Echo", "Reverb", "Time", "Feedback"},
    {"Echo", "Reverb", "Time", "Feedback"},
    {"Echo", "Reverb", "Time", "Feedback"},
    {"Echo", "Time", "Feedback", "Reverb"},
    {"Key", "Tune", "MIDI out", "Lights"},
    {"MIDI out", "Channels", "Clock", "Clock out"},
    {"Mirror", "", "", ""},
    {"Chord", "Root", "Type", "Bars"},
};
/* the black keys' effects, left to right */
static const uint8_t BLACK_FX[11] = {FX_THROW, FX_TAPESTOP, FX_SWELL, FX_LOWPASS, FX_HIGHPASS, FX_SWEEP, FX_PHASER,
                                     FX_FREEZE, FX_STUTTER, FX_DROP, FX_ROLL};
/* a key (0 = F3 .. 26 = G5) as a white key's index (0..15), or as a black key's (0..10) with 0x80 set */
static int key_role(int i)
{
    int k, w = 0, b = 0;
    for (k = 0; k < i; k++) {
        int pc = (5 + k) % 12;
        if (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10)
            b++;
        else
            w++;
    }
    {
        int pc = (5 + i) % 12;
        return (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10) ? 0x80 | b : w;
    }
}

/* the screen's colours: a night club in 1977, a mirror ball over a dark floor. The bass cyan (the loudest
 * thing, the most light), the pad violet, the lead gold, the drums red; the accent magenta. Each a dim tint too. */
#define K_BG RGB(16, 14, 34)
#define K_PANEL RGB(40, 36, 78)
#define K_LINE RGB(58, 54, 104)
#define K_DIM RGB(136, 128, 186)
#define K_TEXT RGB(244, 240, 255)
#define K_ACC RGB(255, 72, 170)
#define K_BASS RGB(0, 214, 240)
#define K_BASS_T RGB(22, 72, 104)
#define K_PAD RGB(176, 124, 255)
#define K_PAD_T RGB(62, 46, 112)
#define K_LEAD RGB(255, 196, 40)
#define K_LEAD_T RGB(96, 76, 32)
#define K_DRUM RGB(255, 92, 92)
#define K_DRUM_T RGB(92, 40, 56)
#define K_HOLD RGB(120, 224, 255)

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];
    uint32_t down_t[NB];
    uint32_t turn_t;
    uint8_t help;
    uint8_t reset_done;
    uint8_t dirty;
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];
    uint8_t rec_grid;                  /* REC held: the effects on the white keys */
    uint32_t fx_keys, lead_keys;       /* keys down as effects, as the lead (each keeps its role until let go) */
    uint16_t fx_sent;
    uint8_t cslot;                     /* the Chords page: the chord being edited (its progression's length: a new one) */
} ui;

static uint8_t sp_said;
static struct {
    uint8_t playing;
    uint32_t play_t;
} sp_tp;
static void sp_piece(void);
static void msg_text(const char *a, const char *b, char *t)
{
    uint32_t i, k = 0;
    for (i = 0; i < 23u && a && a[i]; i++)
        t[k++] = a[i];
    if (b && b[0])
        t[k++] = ' ';
    for (i = 0; i < 23u && b && b[i]; i++)
        t[k++] = b[i];
    while (k && t[k - 1] == '.')
        k--;
    t[k] = 0;
}
static void say_as(const char *a, const char *b, uint32_t flags)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (flags && ann_on()) {
        msg_text(a, b, t);
        ann_say(t, 0, flags);
        sp_said = 1;
    }
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

static void caps(char *d, const char *s, int n)
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

/* ------------------------------------------------------------ chords --- */
/* The Chords page edits the chord changes of the section playing (REC holds it there; SEL moves on). Its chords are
 * the slots up to the progression's end, then one more, empty, where a turn adds a chord; the last one's Bars down
 * to End takes it away. A root is in semitones above the key, shown as the note it is now. */
static const uint8_t LEN_BEATS[7] = {0, 2, 4, 8, 12, 16, 32};
static const char *const LEN_NAME[7] = {"End", "1/2", "1", "2", "3", "4", "8"};
static const char *const QUAL_NAME[EN_NQUAL] = {"Maj", "Min", "7", "Maj7", "Min7", "Min11", "Sus4", "Power"};
static int cprog(void) { return en_sec_prog(proj.piece, en_sec); }
static uint16_t *cslot_p(int i) { return &proj.chords[proj.piece][cprog() * EN_PROG_MAX + i]; }
static int ccount(void)
{
    int n = 0;
    while (n < EN_PROG_MAX && EN_CH_BEATS(*cslot_p(n)))
        n++;
    return n;
}
static int cslot(void)                          /* the slot edited, within the progression (+1: a new chord) */
{
    int n = ccount(), top = n < EN_PROG_MAX ? n : EN_PROG_MAX - 1;
    if (ui.cslot > top)
        ui.cslot = (uint8_t)top;
    return ui.cslot;
}
static int len_index(int beats)
{
    int i;
    for (i = 1; i < 7; i++)
        if (LEN_BEATS[i] >= beats)
            return i;
    return 6;
}
static uint16_t cshown(void)                    /* the slot's chord, or for a new one the last (its length 0) */
{
    int s = cslot(), n = ccount();
    if (s < n)
        return *cslot_p(s);
    return n ? (uint16_t)(*cslot_p(n - 1) & 0xFFu) : 0u;
}
static void cput(int i, uint16_t c)
{
    *cslot_p(i) = c;
    en_prog_set(proj.piece, cprog(), i, c);
}
static void chord_edit(int k, int v)            /* a value on the Chords page */
{
    int s = cslot(), n = ccount();
    uint16_t c = cshown();
    int r = EN_CH_R(c), q = EN_CH_Q(c), b = EN_CH_BEATS(c);
    if (s >= n) {                               /* a new chord: the last one's, a bar unless its length says */
        if (k == K_CLEN && v == 0)
            return;
        b = n ? EN_CH_BEATS(*cslot_p(n - 1)) : 4;
    }
    if (k == K_CROOT)
        r = (v % 12 + 12) % 12;
    else if (k == K_CTYPE)
        q = v;
    else if (k == K_CLEN)
        b = LEN_BEATS[v];
    if (!b) {                                   /* End: the last chord taken away */
        if (s == n - 1 && n > 1)
            cput(s, 0);
        return;
    }
    cput(s, EN_CH(r, q, b));
}
static void chord_text(uint16_t c, char *b)     /* "Eb", "Fm7" ... in the key now */
{
    const char *r = EN_NOTE_NAME[(en_key + EN_CH_R(c)) % 12], *q = EN_QUAL_SUFFIX[EN_CH_Q(c)];
    while (*r)
        *b++ = *r++;
    while (*q)
        *b++ = *q++;
    *b = 0;
}

/* ------------------------------------------------------------ values --- */
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_MIRROR)
        return !proj.nomirror;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    if (k == K_CSLOT)
        return cslot();
    if (k == K_CROOT)
        return EN_CH_R(cshown());
    if (k == K_CTYPE)
        return EN_CH_Q(cshown());
    if (k == K_CLEN)
        return EN_CH_BEATS(cshown()) ? len_index(EN_CH_BEATS(cshown())) : 0;
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
    } else {
        int n = ccount(), s = cslot();
        *lo = k == K_CLEN && s < n && !(s == n - 1 && n > 1) ? 1 : 0;
        *hi = k == K_LEDS ? 2 : k == K_MIRROR ? 1 : k == K_CPU ? 100 : k == K_CSLOT ? (n < EN_PROG_MAX ? n : EN_PROG_MAX - 1)
            : k == K_CROOT ? 11 : k == K_CTYPE ? EN_NQUAL - 1 : k == K_CLEN ? 6 : 0;
    }
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_LEDS || k == K_MIRROR) {
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else if (k == K_CSLOT) {
        if (v >= ccount()) {
            const char *n = "New";
            while ((*b++ = *n++))
                ;
        } else {
            itoa_u((uint32_t)v + 1u, b);
        }
    } else if (k == K_CROOT) {
        const char *n = EN_NOTE_NAME[(en_key + v) % 12];
        while ((*b++ = *n++))
            ;
    } else if (k == K_CTYPE || k == K_CLEN) {
        const char *n = k == K_CTYPE ? QUAL_NAME[v] : LEN_NAME[v];
        while ((*b++ = *n++))
            ;
    } else {
        b[0] = 0;
    }
}

static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v;
        en_set(k, v);
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    } else if (k == K_MIRROR) {
        proj.nomirror = (uint8_t)!v;
    } else if (k == K_CSLOT) {
        ui.cslot = (uint8_t)v;
        return;
    } else {
        chord_edit(k, v);
    }
    mark_dirty();
}

/* knob acceleration by speed (X0X's, by way of AMB-1) */
static uint8_t accel_off;
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE || k == K_CPU)
        return;
    knob_range(k, &lo, &hi);
    if (k == K_CROOT) {                          /* the root goes round */
        knob_set(k, (int)(((knob_get(k) + e) % 12 + 12) % 12));
        return;
    }
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ---------------------------------------------------------- the song --- */
static void set_key(int pc)                     /* a key: the song moves to its note */
{
    pc = (pc % 12 + 12) % 12;
    if (pc == proj.par[P_ROOT]) {
        say("KEY", EN_NOTE_NAME[pc]);
        return;
    }
    knob_set(P_ROOT, pc);
    say("KEY", EN_NOTE_NAME[pc]);
}

static void set_pattern(int p)
{
    p = (p % EN_NPATTERNS + EN_NPATTERNS) % EN_NPATTERNS;
    knob_set(P_PATTERN, p);
    say("PATTERN", EN_PATTERN_NAME[p]);
}

static void set_piece(int i)                    /* the song playing kept as it is; song i as it was left */
{
    project_piece(i);
    en_seed(plat_ms());
    project_apply();
    say_as("SONG", piece_name(proj.piece), 0);
    sp_piece();
    mark_dirty();
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    if (v == V_CHORDS && ui.view != V_CHORDS)
        ui.cslot = (uint8_t)(en_prog_slot > 0 ? en_prog_slot : 0);
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void button(int b)
{
    int v;
    if (b == B_SEL) {                            /* SEL: the next section */
        en_next();
        say("NEXT SECTION", en_playing ? "ON THE BAR" : 0);
        return;
    }
    if (b == B_FX) {                             /* FX: each instrument's FX page, Space, and round again (from any other
                                                  * page: Bass FX) */
        for (v = 0; v < (int)sizeof FX_CYCLE; v++)
            if (FX_CYCLE[v] == ui.view)
                break;
        set_view(FX_CYCLE[v < (int)sizeof FX_CYCLE ? (v + 1) % (int)sizeof FX_CYCLE : 0]);
        return;
    }
    if (VIEW_BTN[ui.view] == b) {                /* its page again: the button's next page (ARP: Lead 2) */
        for (v = ui.view + 1; v < NVIEWS && VIEW_BTN[v] != b; v++)
            ;
        if (v < NVIEWS) {
            set_view(v);
            return;
        }
    }
    for (v = 0; v < NVIEWS; v++)                 /* a page's button: its page */
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_PLAY:
        sp_tp.playing = !en_playing;
        sp_tp.play_t = plat_ms();
        if (en_playing) {
            en_play(0);
            say("STOPPED", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("PLAYING", 0);
        }
        break;
    case B_REC:
        say("HOLD", en_held ? "OFF" : "ON");
        en_hold(!en_held);
        break;
    case B_OCTDN: case B_OCTUP:
        knob_set(P_BOCT, proj.par[P_BOCT] + (b == B_OCTUP ? 1 : -1));
        {
            char t[8];
            en_param_text(P_BOCT, proj.par[P_BOCT], t);
            say("BASS OCTAVE", t);
        }
        break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            warn("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* MIDI in (USB and the TRS jack): a channel 1 note moves the key; clock and start / stop drive the grid, from the port
 * the MIDI page's Clock names (Internal: none; with no clock coming in, GIO-1 keeps its own time) */
static void midi_in(void)
{
    uint32_t pkt;
    int port = 0;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu) {
            int src = proj.par[P_MCLOCK];
            if ((src == 1 && port == PORT_USB) || (src == 2 && port == PORT_TRS))
                en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        }
        else if (st == 0x90u && v)
            set_key((int)(n % 12u));
    }
}

#define HELP_HOLD 900u
#define RESET_HOLD 1500u
static void help_tick(uint32_t btn, uint32_t keys)
{
    uint32_t now = plat_ms();
    int i;
    if (ui.help) {
        uint32_t m = 1u << (ui.help - 1);
        if (btn != m || keys || (int32_t)(ui.turn_t - ui.down_t[ui.help - 1]) > 0)
            ui.help = 0;
        return;
    }
    if (!btn || (btn & (btn - 1u)) || keys)
        return;
    for (i = 0; i < NB; i++)
        if (btn == 1u << i && i != B_PLAY && i != B_REC && !(ui.btn_used & btn) && now - ui.down_t[i] >= HELP_HOLD &&
            (int32_t)(ui.turn_t - ui.down_t[i]) <= 0) {
            ui.help = (uint8_t)(i + 1);
            ui.btn_used |= btn;
        }
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (i == B_SAVE)
                ui.reset_done = 0;
            if (i == B_PLAY)
                button((int)i);
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
    if ((btn >> B_REC & 1u) && !ui.rec_grid && plat_ms() - ui.down_t[B_REC] >= 350u) {
        ui.rec_grid = 1;                         /* REC held: the effects grid while it is */
        ui.btn_used |= 1u << B_REC;
    }
    if (!(btn >> B_REC & 1u))
        ui.rec_grid = 0;
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++) {                /* white keys the lead, black keys effects; on the grid all effects */
        uint32_t m = 1u << i;
        int r;
        if (!(ch & m))
            continue;
        r = key_role((int)i);
        if (keys & m) {
            if ((r & 0x80) || GRID_PAGE(ui.view) || ui.rec_grid) {
                int f = (r & 0x80) ? BLACK_FX[r & 0x7F] : r;
                ui.fx_keys |= m;
                say("EFFECT", EN_FX_NAME[f]);
            } else {
                ui.lead_keys |= m;
                en_lead_key(r, 1);
            }
        } else if (ui.fx_keys & m) {
            ui.fx_keys &= ~m;
        } else if (ui.lead_keys & m) {
            ui.lead_keys &= ~m;
            en_lead_key(r, 0);
        }
    }
    {
        uint16_t fx = 0;
        for (i = 0; i < NKEYS; i++)
            if (ui.fx_keys >> i & 1u) {
                int r = key_role((int)i);
                fx |= (uint16_t)(1u << ((r & 0x80) ? BLACK_FX[r & 0x7F] : r));
            }
        if (fx != ui.fx_sent) {
            ui.fx_sent = fx;
            en_fx(fx);
        }
    }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages; with HOME held, the tempo */
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_SONG) {
                ui.touched = 0;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else {
            int v = (int)ui.view + (e > 0 ? 1 : -1);
            if (v >= 0 && v < NVIEWS)
                set_view(v);
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        set_pattern(proj.par[P_PATTERN] + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        set_piece(proj.piece + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((btn >> B_SAVE & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_SAVE] > RESET_HOLD) {
        ui.reset_done = 1;
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        project_piece_reset();
        project_apply();
        say("RESET", piece_name(proj.piece));
        mark_dirty();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
    help_tick(btn, keys);
}

/* ------------------------------------------------------------- screen --- */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (w <= 0 || h <= 0)
        return;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    if (r > w / 2)
        r = w / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    const float th = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
    }
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }

/* the battery (plat_battery): 0 (low) .. 3 bars, 4 a USB host powers the FM-1, -1 not read yet */
#ifdef PLAT_HOST
static int8_t batt_host = -2;
static int batt(void) { return batt_host > -2 ? batt_host : plat_battery(); }
#else
#define batt() plat_battery()
#endif
#define K_LOW RGB(255, 110, 60)
#define BATT_W 20
static void draw_batt(int32_t x, int b)
{
    static const int8_t BOLT[7][2] = {{10, 2}, {9, 2}, {8, 2}, {7, 5}, {9, 2}, {8, 2}, {7, 2}};
    uint16_t c = b <= 1 ? K_LOW : K_DIM;
    int i;
    cv_round(x, 9, 18, 10, 2, c);
    cv_rect(x + 1, 10, 16, 8, K_BG);
    cv_rect(x + 18, 12, 2, 4, c);
    if (b == 4) {
        for (i = 0; i < 7; i++)
            cv_rect(x + BOLT[i][0], 10 + i, BOLT[i][1], 1, K_TEXT);
        return;
    }
    for (i = 0; i < b && i < 3; i++)
        cv_rect(x + 2 + i * 5, 11, 4, 6, b <= 1 ? K_LOW : K_BASS);
}
static const char *batt_words(int b)
{
    static const char *const W[5] = {"Battery low", "Battery one third", "Battery two thirds", "Battery full",
                                     "On USB power"};
    return b >= 0 && b <= 4 ? W[b] : 0;
}

/* the header: the song in a magenta tag, its key; on the right, the battery, the tempo with a beat light (a play
 * triangle while stopped) and a dot while there is something unsaved. Messages take its place for a moment. */
static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)en_key << 8),
                           en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_ext << 6),
                      (uint32_t)proj.par[P_TEMPO] | (ui.view == V_SETUP ? plat_cpu_pct() << 8 : 0u) |
                          (uint32_t)(batt() + 1) << 24);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (en_playing)
        h = hash(h, (en_bars / 4u) & 1u);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        if (batt() >= 0) {
            x -= BATT_W;
            draw_batt(x, batt());
            x -= 6;
        }
        if (ui.dirty) {
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        if (en_ext) {
            t[0] = 'E';
            t[1] = 'X';
            t[2] = 'T';
            t[3] = 0;
        } else {
            itoa_u((uint32_t)proj.par[P_TEMPO], t);
        }
        if (ui.view == V_SETUP) {
            char c[12] = "CPU ";
            itoa_u(plat_cpu_pct(), c + 4);
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_DIM);
            x -= 10;
        }
        x -= text_w(&FONT_S, t);
        cv_text(x, 6, &FONT_S, t, en_playing ? K_TEXT : K_DIM);
        x -= 16;
        if (en_playing) {                         /* a beat light: every beat */
            int on = ((en_bars / 4u) & 1u) == 0u;
            cv_round(x, 9, 10, 10, 5, on ? K_ACC : K_LINE);
        } else {
            int32_t j;
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
        }
        w = text_w(&FONT_B, piece_name(proj.piece)) + 12;
        cv_round(4, 4, w, 20, 6, K_ACC);
        cv_text(10, 6, &FONT_B, piece_name(proj.piece), K_BG);
        w += 10;
        if (w + text_w(&FONT_XS, EN_NOTE_NAME[(int)en_key]) + 4 <= x) {
            w = cv_text(w, 8, &FONT_XS, EN_NOTE_NAME[(int)en_key], K_TEXT);
            if (w + 30 <= x)
                cv_text(w + 3, 8, &FONT_XS, "KEY", K_DIM);
        }
    }
    cv_blit(0, 0);
}

/* the main band: the form across the top (a block a section, its width its bars; the one playing lit, how far
 * through it), the section's name and bar under it; then the bass's sixteen steps in a ring round the chord, each
 * step's height its note; the four layers' levels on the left, the drums' grid on the right */
#define CX 112
#define CY 96
static const char *chord_now(char *nm)
{
    int pcs[EN_NPAD + 1], k;
    pcs[0] = en_chord_root;
    for (k = 0; k < EN_NPAD; k++)
        pcs[k + 1] = en_chord[k] % 12;
    return en_chord_name(pcs, EN_NPAD + 1, nm);
}

static void draw_form(void)
{
    int n = en_song_nsec(proj.piece), i, tot = 0;
    int32_t x = 8, wall = 224;
    for (i = 0; i < n; i++)
        tot += en_sec_bars(proj.piece, i);
    for (i = 0; i < n; i++) {
        int32_t w = (int32_t)(wall * en_sec_bars(proj.piece, i) / (tot ? tot : 1)) - 2, cur = i == en_sec;
        if (w < 3)
            w = 3;
        cv_round(x, 4, w, 10, 3, cur ? (en_held ? K_HOLD : K_ACC) : (i == en_next_sec && en_playing ? K_LINE : K_PANEL));
        if (cur && en_sec_len) {                  /* how far through */
            int32_t f = (int32_t)((float)w * ((float)en_sec_bar + (float)en_step / 16.0f) / (float)en_sec_len);
            cv_round(x, 4, f < 3 ? 3 : f, 10, 3, en_held ? mix(K_HOLD, K_TEXT, 6) : mix(K_ACC, K_TEXT, 6));
        }
        x += w + 2;
    }
    {
        char t[24], b[12];
        caps(t, en_sec_name(proj.piece, en_sec), 16);
        cv_text(8, 17, &FONT_XS, t, en_held ? K_HOLD : K_TEXT);
        if (en_held)
            cv_text(10 + text_w(&FONT_XS, t), 17, &FONT_XS, " HOLD", K_HOLD);
        itoa_u((uint32_t)en_sec_bar + 1u, b);
        {
            int32_t k = 0;
            while (b[k])
                k++;
            b[k++] = '/';
            itoa_u(en_sec_len, b + k);
        }
        cv_text(232 - text_w(&FONT_XS, b), 17, &FONT_XS, b, K_DIM);
    }
}

static void draw_ring(void)
{
    int s, lo = 127, hi = 0;
    for (s = 0; s < EN_STEPS; s++)
        if (en_bpat[s] >= 0) {
            lo = en_bpat[s] < lo ? en_bpat[s] : lo;
            hi = en_bpat[s] > hi ? en_bpat[s] : hi;
        }
    for (s = 0; s < EN_STEPS; s++) {
        float a = (float)s * (FM_TWO_PI / 16.0f), r = 42.0f, sx = fm_sinf(a), sy = -fm_cosf(a);
        int cur = en_playing && s == en_step;
        if (en_bpat[s] >= 0) {
            float h = hi > lo ? (float)(en_bpat[s] - lo) / (float)(hi - lo) : 0.5f, rr = 2.2f + 2.6f * h;
            uint16_t c = cur ? mix(K_BASS, K_TEXT, q16(en_bass_glow) / 2) : mix(K_BASS_T, K_BASS, 7 + (int)(7.0f * h));
            if (en_bacc[s])
                disc((float)CX + sx * r, (float)CY + sy * r, rr + 1.6f, cur ? K_TEXT : K_LINE);
            disc((float)CX + sx * r, (float)CY + sy * r, rr + (cur ? 1.2f : 0.0f), c);
        } else {
            disc((float)CX + sx * r, (float)CY + sy * r, cur ? 1.8f : 1.1f, cur ? K_DIM : K_LINE);
        }
    }
    {   /* the chord, and the lead's note under it as it sings */
        char nm[16];
        chord_now(nm);
        if (text_w(&FONT_B, nm) <= 58)
            text_c(CX, CY - 10, &FONT_B, nm, en_held ? K_HOLD : K_TEXT);
        else
            text_c(CX, CY - 7, &FONT_XS, nm, K_TEXT);
        if (en_lead_note && en_lead_glow > 0.08f && (en_layers & L_LEAD))
            text_c(CX, CY + 10, &FONT_XS, EN_NOTE_NAME[en_lead_note % 12], mix(K_LEAD_T, K_LEAD, q16(en_lead_glow * 1.5f)));
    }
}

static void draw_meters(void)                    /* left: bass, pad, lead, drums */
{
    static const uint16_t C[4] = {K_BASS, K_PAD, K_LEAD, K_DRUM}, T[4] = {K_BASS_T, K_PAD_T, K_LEAD_T, K_DRUM_T};
    static const uint8_t L[4] = {L_BASS, L_PAD, L_LEAD, L_KICK | L_SNARE | L_HATS | L_OHAT | L_TOMS};
    float lv[4], dm = 0.0f;
    int i;
    for (i = 0; i < EN_NDRUM; i++)
        dm = fm_maxf(dm, en_drum_glow[i]);
    lv[0] = en_bass_glow * (0.4f + 0.6f * en_bass_cut) * (float)proj.par[P_BASS] * 0.01f;
    lv[1] = en_pad_level * (float)proj.par[P_PAD] * 0.01f;
    lv[2] = en_lead_glow * (float)proj.par[P_LEAD] * 0.01f;
    lv[3] = dm * (float)proj.par[P_DRUMS] * 0.01f;
    for (i = 0; i < 4; i++) {
        int32_t x = 8 + i * 10, h = (int32_t)(fm_clampf(lv[i], 0.0f, 1.0f) * 84.0f + 0.5f);
        cv_round(x, 46, 6, 90, 3, (en_layers & L[i]) ? T[i] : K_PANEL);
        if (h > 0)
            cv_round(x, 46 + 90 - h - 2, 6, h + 2, 3, C[i]);
    }
}

static void draw_drums(void)                     /* right: kick, snare, closed hat, open hat, tom; a cell a step */
{
    int d, s;
    for (d = 0; d < EN_NDRUM; d++) {
        int32_t y = 52 + d * 15;
        uint32_t p = en_dpat[d];
        int g = q16(en_drum_glow[d]);
        for (s = 0; s < EN_STEPS; s++) {
            int32_t x = 168 + s * 4;
            int cur = en_playing && s == en_step, on = (int)(p >> s & 1u);
            uint16_t c = on ? (cur ? mix(K_DRUM, K_TEXT, g / 2) : mix(K_DRUM_T, K_DRUM, s % 4 == 0 ? 12 : 8))
                            : (cur ? K_DIM : (s % 4 == 0 ? K_LINE : K_PANEL));
            cv_rect(x, y, 3, on ? 8 : 3, c);
            if (!on)
                cv_rect(x, y + 5, 3, 3, c);
        }
    }
}

/* a button's card (help_tick): its name, then what it does */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("THE SONG: TEMPO, EVOLVE,"); L(" MOTION, ARRANGE"); L("AGAIN: THE MIXER"); L("+ SELECT: TEMPO"); break;
    case B_ENV: L("THE BASS SEQUENCE"); L("LEVEL, CUTOFF, EMPHASIS,"); L(" DECAY"); break;
    case B_ARP: L("THE LEAD"); L("LEVEL, VOICE, BUSY, GLIDE"); L("AGAIN: LEAD 2: ECHO,"); L(" REVERB, OCTAVE, TONE"); break;
    case B_LFO: L("THE PAD"); L("LEVEL, COLOUR, BRIGHT,"); L(" PHASER"); break;
    case B_SEQ: L("THE DRUMS"); L("LEVEL, FILLS, HATS, SWING"); break;
    case B_FX: L("BASS, PAD, LEAD, DRUMS FX:"); L(" ECHO, REVERB, TIME,"); L(" FEEDBACK; THEN SPACE"); L("THE 16 EFFECTS ON THE KEYS"); break;
    case B_GLO: L("SETUP: KEY, TUNE,"); L(" MIDI OUT, LIGHTS"); L("AGAIN: MIDI"); L("AGAIN: THE SCREEN MIRROR"); break;
    case B_EDIT: L("THE CHORDS: A CHORD,"); L(" ITS ROOT, TYPE AND BARS"); L("REC HOLDS THE SECTION"); L("SEL: THE NEXT ONE"); break;
    case B_SEL: L("THE NEXT SECTION,"); L("ON THE NEXT BAR"); break;
    case B_REC: L("HOLD: THE SECTION"); L(" REPEATS"); L("AGAIN: LET IT GO ON"); break;
    case B_OCTDN: L("THE BASS AN OCTAVE DOWN"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP: L("THE BASS AN OCTAVE UP"); L("WITH OCT-, 5 S: UPDATE MODE"); break;
    case B_SAVE: L("SAVES ALL THE SONGS"); L("KEEP HOLDING: THE SONG"); L(" BACK TO ITS RECIPE"); break;
    default: break;
    }
#undef L
    return n;
}

static void draw_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    cv_round(16, 14, 208, 116, 6, K_PANEL);
    cv_round(16, 14, 208, 26, 6, K_ACC);
    cv_rect(16, 34, 208, 6, K_ACC);
    text_c(120, 18, &FONT_B, BTN_LABEL[ui.help - 1], K_BG);
    for (k = 0; k < n; k++)
        text_c(120, 50 + k * 20, &FONT_S, l[k] + (l[k][0] == ' '), K_TEXT);
}

/* the screen mirror (GLO three times): on or off (KNOB 1), and how to show it on a computer */
static int mirror_showing(void)                  /* a computer is showing the screen now */
{
#if FM1B_MIRROR
    return mirror_on;
#else
    return 0;
#endif
}
void ui_mirror_guard(void)                       /* off: a computer's session ended at once (each main-loop pass, before
                                                  * anything is drawn: it sees nothing, and plays nothing) */
{
#if FM1B_MIRROR
    if (proj.nomirror && mirror_on)
        mirror_off();
#endif
}
static void draw_mirror(void)
{
    static const char *const L[] = {"1. USB CABLE TO A COMPUTER", "2. IN CHROME OR EDGE:", "GUINANNO1.GITHUB.IO/FM1-GIO",
                                    "THEN MIRROR", "3. CONNECT, ALLOW MIDI"};
    int k, show = mirror_showing();
    text_c(120, 6, &FONT_B, "THIS SCREEN ON A COMPUTER", K_TEXT);
    for (k = 0; k < 5; k++)
        text_c(120, 32 + k * 18, &FONT_S, L[k], k == 2 || k == 3 ? K_ACC : K_DIM);
    cv_round(40, 122, 160, 20, 6, proj.nomirror ? K_PANEL : show ? K_ACC : K_PANEL);
    text_c(120, 124, &FONT_S, proj.nomirror ? "MIRROR OFF" : show ? "SHOWING NOW" : "MIRROR ON, READY",
           proj.nomirror ? K_DIM : show ? K_BG : K_TEXT);
}

/* the effects grid (FX, or REC held): four rows of four, a white key each; lit while held */
static void draw_grid(void)
{
    static const uint16_t RC[4] = {K_BASS, K_PAD, K_LEAD, K_DRUM};
    int r, c;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++) {
            int f = r * 4 + c, on = (int)(en_fx_on >> f & 1u);
            int32_t x = 4 + c * 59, y = 4 + r * 35;
            char t[4];
            cv_round(x, y, 56, 32, 5, on ? RC[r] : K_PANEL);
            itoa_u((uint32_t)f + 1u, t);
            cv_text(x + 4, y, &FONT_XS, t, on ? K_BG : K_DIM);
            if (!c)
                cv_text(x + 52 - text_w(&FONT_XS, EN_FX_ROW[r]), y, &FONT_XS, EN_FX_ROW[r], on ? K_BG : RC[r]);
            text_c(x + 28, y + 15, &FONT_XS, EN_FX_NAME[f], on ? K_BG : K_TEXT);
        }
}

/* the Chords page: the form across the top, as the Song page's; then the section's chords, two rows of four: each its
 * name in the key now and its length; the one playing lit violet (the pad's), the one edited outlined */
static void draw_chords(void)
{
    int n = ccount(), s = cslot(), i;
    draw_form();
    for (i = 0; i < EN_PROG_MAX; i++) {
        int32_t x = 4 + (i % 4) * 59, y = 32 + (i / 4) * 56;
        int play = en_playing && i < n && i == en_prog_slot, ed = i == s;
        uint16_t c = *cslot_p(i), bg = play ? K_PAD : i < n ? K_PANEL : K_BG, fg = play ? K_BG : K_TEXT;
        char t[16];
        if (ed) {
            cv_round(x, y, 56, 52, 6, K_ACC);
            cv_round(x + 2, y + 2, 52, 48, 5, bg);
        } else if (i < n) {
            cv_round(x, y, 56, 52, 6, bg);
        } else {
            cv_round(x, y, 56, 52, 6, K_PANEL);
            cv_round(x + 1, y + 1, 54, 50, 5, K_BG);
        }
        itoa_u((uint32_t)i + 1u, t);
        cv_text(x + 5, y + 2, &FONT_XS, t, play ? K_BG : K_DIM);
        if (i < n) {
            int li = len_index(EN_CH_BEATS(c));
            chord_text(c, t);
            if (text_w(&FONT_B, t) <= 50)
                text_c(x + 28, y + 15, &FONT_B, t, fg);
            else
                text_c(x + 28, y + 19, &FONT_S, t, fg);
            caps(t, LEN_NAME[li], sizeof t);
            {
                int32_t k = 0;
                while (t[k])
                    k++;
                t[k++] = ' ';
                caps(t + k, li >= 3 ? "bars" : "bar", 6);
            }
            text_c(x + 28, y + 37, &FONT_XS, t, play ? K_BG : K_DIM);
        } else if (i == n) {
            text_c(x + 28, y + 15, &FONT_B, "+", ed ? K_ACC : K_LINE);
        }
    }
}

static void draw_main(void)
{
    uint32_t h = hash(2166136261u, proj.piece | (uint32_t)en_sec << 8 | (uint32_t)en_sec_bar << 12 |
                                       (uint32_t)en_held << 20 | (uint32_t)en_playing << 21 | (uint32_t)en_layers << 24),
             i;
    if (ui.frame % 2u)                           /* 30 frames a second */
        return;
    if (ui.help) {
        h = hash(0x48454C50u, ui.help);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_help();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (ui.view == V_CHORDS && !ui.rec_grid) {
        h = hash(hash(h, en_step | (uint32_t)en_key << 8 | (uint32_t)(uint8_t)en_prog_slot << 16 | (uint32_t)cslot() << 24),
                 (uint32_t)en_next_sec);
        for (i = 0; i < EN_PROG_MAX; i++)
            h = hash(h, *cslot_p((int)i));
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_chords();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (ui.view == V_MIRROR && !ui.rec_grid) {
        h = hash(0x4D495252u, (uint32_t)proj.nomirror | (uint32_t)mirror_showing() << 1);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_mirror();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (GRID_PAGE(ui.view) || ui.rec_grid) {
        h = hash(0x46584752u, en_fx_on | (uint32_t)ui.rec_grid << 16);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_grid();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    h = hash(h, en_step | (uint32_t)en_next_sec << 8 | (uint32_t)en_key << 16);
    for (i = 0; i < EN_NPAD; i++)
        h = hash(h, en_chord[i]);
    for (i = 0; i < EN_STEPS; i++)
        h = hash(h, (uint32_t)(uint8_t)en_bpat[i] | (uint32_t)en_bacc[i] << 8);
    for (i = 0; i < EN_NDRUM; i++)
        h = hash(h, en_dpat[i] ^ (uint32_t)q16(en_drum_glow[i]) << 20);
    h = hash(h, (uint32_t)q16(en_pad_level) | (uint32_t)q16(en_bass_glow) << 5 | (uint32_t)q16(en_lead_glow) << 10 |
                    (uint32_t)en_lead_note << 16);
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)proj.par[i * 4 + 4]);
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    draw_form();
    draw_ring();
    draw_meters();
    draw_drums();
    cv_blit(0, MAIN_Y);
}

/* the knob strip (X0X's, by way of AMB-1) */
static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
    h = hash(h, (uint32_t)now_touch);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {
        char t[12];
        int k;
        int32_t xd = 237 - NVIEWS * 6, xt;
        caps(t, VIEW_NAME[ui.view], sizeof t);
        xt = xd - 5 - text_w(&FONT_XS, t);
        cv_rect(4, 5, xt - 9, 1, K_LINE);
        cv_text(xt, -1, &FONT_XS, t, K_ACC);
        for (k = 0; k < NVIEWS; k++)
            cv_round(xd + k * 6, 3, 4, 4, 2, k == ui.view ? K_ACC : K_LINE);
    }
    for (i = 0; i < 4; i++) {
        int k = VIEW_KNOB[ui.view][i], lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : K_BASS;
        if (k == K_NONE)
            continue;
        if (i)
            cv_rect(x, 16, 1, 46, K_LINE);
        if (hot)
            cv_round(x + 3, 11, 54, 57, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, KNOB_LABEL[ui.view][i], sizeof t);
        text_c(cx, 12, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        if (k != K_CPU && hi - lo < 6) {
            int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 2, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        text_c(cx, 50, &FONT_B, t, hot ? K_ACC : K_TEXT);
    }
    cv_blit(0, KNB_Y);
}

/* the lights: the page's button, PLAY while playing, REC while held; on the white keys the chord's notes (the lead's
 * safe notes) and the one played, on the grid the effects on; the black keys lit, dark while held */
static void leds(void)
{
    uint32_t b = 0, k = 0, i, ch = 0;
    int grid = GRID_PAGE(ui.view) || ui.rec_grid;
    b |= 1u << VIEW_BTN[ui.view];
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_held || ui.rec_grid)
        b |= 1u << B_REC;
    for (i = 0; i < EN_NPAD; i++)
        ch |= 1u << (en_chord[i] % 12u);
    if (proj.leds)
        for (i = 0; i < NKEYS; i++) {
            int r = key_role((int)i), on;
            if (r & 0x80)
                on = !(ui.keys >> i & 1u);
            else if (grid)
                on = (int)(en_fx_on >> r & 1u) || (r & 3) == 0;
            else
                on = (int)(ch >> en_step_pc(r) & 1u) || (ui.keys >> i & 1u);
            if (on)
                k |= 1u << i;
        }
    plat_glow(proj.leds == 1);
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, while stopped and quiet, nothing touched (a flash erase silences the
 * audio for a moment: never while playing; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || en_pad_level > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

/* ------------------------------------------------------ screen reader --- */
/* The screen as text for a screen reader on the computer (announce.c; fm1-reader): a page once it settles, with
 * its knobs; a knob's value as it turns; a help card; the section as it changes and the chord as it moves (queued);
 * playing and stopping from MIDI; what a button did. WHERE is the whole screen in two: the page, its knobs and the
 * header, then the song (the section, the chord, what plays). Nothing is built while no reader listens. */
static struct {
    uint8_t view, help, ext, started, sec;
    int16_t val[4];
    uint32_t chord;
} sp;
static char sp_buf[ANN_MAX + 1];
static uint32_t sp_n;
static const char *const CHORD_Q[][2] = {
    {"", "major"}, {"m", "minor"}, {"7", "7"}, {"M7", "major 7"}, {"m7", "minor 7"}, {"m11", "minor 11"},
    {"6", "6"}, {"m6", "minor 6"}, {"add9", "add 9"}, {"madd9", "minor add 9"}, {"aug", "augmented"},
    {"dim", "diminished"}, {"dim7", "diminished 7"}, {"m7b5", "half diminished"}, {"mM7", "minor major 7"},
    {"sus2", "suspended 2"}, {"sus4", "suspended 4"}, {"7sus4", "7 suspended 4"}, {"5", "5"},
};
#define NCHORD_Q (sizeof CHORD_Q / sizeof CHORD_Q[0])

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX)
        sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static void sp_int(uint32_t v)
{
    char t[12];
    itoa_u(v, t);
    sp_cat(t);
}
static int sp_same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}
static void sp_knob(int view, int i)
{
    char t[16];
    sp_cat(KNOB_LABEL[view][i]);
    sp_cat(" ");
    knob_text(VIEW_KNOB[view][i], t);
    sp_cat(t);
}
static void sp_chord_q(int root, const char *q)
{
    uint32_t i;
    sp_cat(EN_NOTE_NAME[root % 12]);
    sp_cat(" ");
    for (i = 0; i < NCHORD_Q; i++)
        if (sp_same(q, CHORD_Q[i][0])) {
            sp_cat(CHORD_Q[i][1]);
            return;
        }
    sp_cat(q);
}
static void sp_chord(void)
{
    char nm[16];
    const char *q = chord_now(nm), *r = EN_NOTE_NAME[en_chord_root % 12u];
    while (*r && *q == *r)
        q++, r++;
    sp_chord_q(en_chord_root % 12u, q);
}

static void sp_page(int all)
{
    int i;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(VIEW_NAME[ui.view]);
    sp_cat(" page");
    for (i = 0; i < 4; i++) {
        sp_cat(i ? ", " : ": ");
        sp_knob(ui.view, i);
    }
    if (!all)
        return;
    sp_cat(". Song ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", key of ");
    sp_cat(EN_NOTE_NAME[(int)en_key]);
    if (en_ext) {
        sp_cat(", tempo EXT");
    } else if (ui.view != V_SONG) {
        sp_cat(", tempo ");
        sp_int((uint32_t)proj.par[P_TEMPO]);
    }
    sp_cat(sp_tp.playing ? ", playing" : ", stopped");
    if (ui.dirty)
        sp_cat(", not saved yet");
    if (ui.view == V_SETUP) {
        sp_cat(". CPU ");
        sp_int(plat_cpu_pct());
        sp_cat("%");
    }
    if (batt_words(batt())) {
        sp_cat(". ");
        sp_cat(batt_words(batt()));
    }
}

static void sp_section(void)                   /* "Section Chorus, bar 3 of 8" */
{
    sp_cat("Section ");
    sp_cat(en_sec_name(proj.piece, en_sec));
    if (en_held)
        sp_cat(", held");
    sp_cat(", bar ");
    sp_int((uint32_t)en_sec_bar + 1u);
    sp_cat(" of ");
    sp_int(en_sec_len);
}
static void sp_song(void)                      /* the section, the chord, what plays */
{
    static const char *const LN[4] = {"bass", "pad", "lead", "drums"};
    static const uint8_t LM[4] = {L_BASS, L_PAD, L_LEAD, L_KICK | L_SNARE | L_HATS | L_OHAT | L_TOMS};
    int i, any = 0;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_section();
    sp_cat(". Chord ");
    sp_chord();
    sp_cat(". Playing:");
    for (i = 0; i < 4; i++)
        if (en_layers & LM[i]) {
            sp_cat(any ? ", " : " ");
            sp_cat(LN[i]);
            any = 1;
        }
    if (!any)
        sp_cat(" nothing");
    sp_cat(". Pattern ");
    sp_cat(EN_PATTERN_NAME[proj.par[P_PATTERN]]);
}

static void sp_line(const char *s)
{
    char p = sp_n ? sp_buf[sp_n - 1] : 0;
    if (*s == ' ')
        s++;
    else if (p != ',' && p != ':')
        sp_cat(".");
    sp_cat(" ");
    sp_cat(s);
}
static void sp_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(BTN_LABEL[ui.help - 1]);
    sp_cat(" button");
    for (k = 0; k < n; k++)
        sp_line(l[k]);
}

static void sp_piece_text(void)                /* "SONG Heaven, key of Ab, tempo 130" */
{
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("SONG ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", key of ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(", tempo ");
    sp_int((uint32_t)proj.par[P_TEMPO]);
}
static void sp_piece(void)
{
    if (!ann_on())
        return;
    sp_piece_text();
    ann_say(sp_buf, 0, ANN_NOW);
    sp_said = 1;
}

static uint32_t sp_chord_sig(void)
{
    char nm[16];
    const char *s = chord_now(nm);
    uint32_t h = 2166136261u;
    while (*s)
        h = hash(h, (uint8_t)*s++);
    return h;
}
static void sp_snap(void)
{
    int i;
    for (i = 0; i < 4; i++)
        sp.val[i] = (int16_t)knob_get(VIEW_KNOB[ui.view][i]);
    sp.view = ui.view;
    sp.help = ui.help;
    sp.ext = en_ext;
    sp.chord = sp_chord_sig();
    sp.sec = en_sec;
}

static void speak(void)
{
    uint32_t now = plat_ms(), c;
    int where = ann_where(), i, changed = -1;
    if (!ann_on()) {
        sp.started = 0;
        sp_said = 0;
        return;
    }
    if (!sp.started || where) {
        sp.started = 1;
        sp_snap();
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            sp_page(1);
        } else {
            sp_page(1);
            ann_say(sp_buf, 0, ANN_NOW);
            sp_song();
        }
        ann_say(sp_buf, 0, 0);
        sp_said = 0;
        return;
    }
    if (sp_said) {
        sp_said = 0;
        sp_snap();
        return;
    }
    if (en_playing != sp_tp.playing && now - sp_tp.play_t > 150u) {
        sp_tp.playing = en_playing;
        ann_say(en_playing ? "PLAYING" : "STOPPED", 0, 0);
    }
    if (en_ext != sp.ext) {
        sp.ext = en_ext;
        sp_n = 0;
        sp_cat("Tempo ");
        if (en_ext)
            sp_cat("EXT");
        else
            sp_int((uint32_t)proj.par[P_TEMPO]);
        ann_say(sp_buf, 0, 0);
    }
    if (ui.help != sp.help) {
        sp.help = ui.help;
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            return;
        }
    }
    if (ui.view != sp.view) {
        sp_snap();
        sp_page(0);
        ann_later(sp_buf, ANN_NOW, 200u, now);
        return;
    }
    for (i = 0; i < 4; i++) {
        int v = knob_get(VIEW_KNOB[ui.view][i]);
        if (v != sp.val[i]) {
            sp.val[i] = (int16_t)v;
            if (changed < 0)
                changed = i;
        }
    }
    if (changed >= 0) {
        sp_n = 0;
        sp_knob(ui.view, changed);
        ann_later(sp_buf, ANN_NOW, 120u, now);
    }
    if (en_sec != sp.sec) {                           /* a new section: queued */
        sp.sec = en_sec;
        sp_n = 0;
        sp_cat("Section ");
        sp_cat(en_sec_name(proj.piece, en_sec));
        ann_say(sp_buf, 0, 0);
    }
    c = sp_chord_sig();
    if (c != sp.chord) {
        sp.chord = c;
        sp_n = 0;
        sp_cat("Chord ");
        sp_chord();
        ann_say(sp_buf, 0, 0);
    }
}

#ifdef PLAT_HOST
static void sp_print(const char *kind, const char *t) { printf("%s\t%s\n", kind, t); }
static void speak_dump(void)
{
    static const char *const MSG[][2] = {{"UPDATE MODE IN", "3.."}, {"UPDATE MODE", "CANCELLED"},
                                         {"NEXT SECTION", "ON THE BAR"}, {"NEXT SECTION", 0}, {"STOPPED", 0},
                                         {"PLAYING", 0}, {"HOLD", "ON"}, {"HOLD", "OFF"}, {"SAVED", 0}};
    static const char *const WARN[][2] = {{"NOT SAVED", "FLASH ERROR"}};
    project_t keep = proj;
    uint8_t keep_view = ui.view, keep_help = ui.help, keep_dirty = ui.dirty, keep_play = sp_tp.playing;
    uint8_t keep_ext = en_ext;
    int v, i, x, lo, hi;
    char t[48];
    for (v = 0; v < NVIEWS; v++) {
        ui.view = (uint8_t)v;
        sp_page(0);
        sp_print("SCREEN", sp_buf);
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[v][i];
            knob_range(k, &lo, &hi);
            if (k >= K_CSLOT && k <= K_CLEN)        /* the Chords page's: as they are */
                lo = hi = knob_get(k);
            for (x = lo; x <= hi; x++) {
                if (k < P_NPARAMS)
                    proj.par[k] = (int16_t)x;
                else if (k == K_LEDS)
                    proj.leds = (uint8_t)x;
                else if (k == K_MIRROR)
                    proj.nomirror = (uint8_t)!x;
                sp_n = 0;
                sp_knob(v, i);
                sp_print("KNOB", sp_buf);
            }
            proj = keep;
        }
    }
    for (x = 0; x < 4; x++) {
        ui.view = (uint8_t)(x * 3 % NVIEWS);
        sp_tp.playing = (uint8_t)(x & 1);
        ui.dirty = (uint8_t)(x >> 1 & 1);
        en_ext = (uint8_t)(x == 3);
        sp_page(1);
        sp_print("WHERE", sp_buf);
        sp_song();
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    ui.dirty = keep_dirty;
    sp_tp.playing = keep_play;
    en_ext = keep_ext;
    ui.view = keep_view;
    for (i = 0; i < NB; i++) {
        const char *l[HELP_LINES];
        if (!help_lines(i, l))
            continue;
        ui.help = (uint8_t)(i + 1);
        sp_help();
        sp_print("HELP", sp_buf);
    }
    ui.help = keep_help;
    for (x = 0; x < 12; x++)
        for (i = 0; i < (int)NCHORD_Q; i++) {
            sp_n = 0;
            sp_cat("Chord ");
            sp_chord_q(x, CHORD_Q[i][0]);
            sp_print("CHORD", sp_buf);
        }
    for (i = 0; i < (int)(sizeof MSG / sizeof MSG[0]); i++) {
        msg_text(MSG[i][0], MSG[i][1], t);
        sp_print("MSG", t);
    }
    for (i = 0; i < (int)(sizeof WARN / sizeof WARN[0]); i++) {
        msg_text(WARN[i][0], WARN[i][1], t);
        sp_print("WARN", t);
    }
    for (x = 0; x < 12; x++) {
        msg_text("KEY", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = 0; x < EN_NPATTERNS; x++) {
        msg_text("PATTERN", EN_PATTERN_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = 0; x < EN_NFX; x++) {
        msg_text("EFFECT", EN_FX_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = -1; x <= 1; x++) {
        char o[8];
        en_param_text(P_BOCT, x, o);
        msg_text("BASS OCTAVE", o, t);
        sp_print("MSG", t);
    }
    for (x = 0; x < NPIECES; x++) {
        int s;
        project_piece(x);
        sp_piece_text();
        sp_print("MSG", sp_buf);
        msg_text("RESET", piece_name(x), t);
        sp_print("MSG", t);
        for (s = 0; s < en_song_nsec(x); s++) {
            sp_n = 0;
            sp_cat("Section ");
            sp_cat(en_sec_name(x, s));
            sp_print("MSG", sp_buf);
        }
    }
    proj = keep;
    sp_print("MSG", "Tempo EXT");
    for (x = 0; x <= 4; x++)
        sp_print("WHERE", batt_words(x));
}
#endif

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    lcd_fill(0, 0, 240, 240, K_BG);
    if (!en_playing)
        en_play(1);                              /* it plays from the start */
    sp_tp.playing = 1;
    sp_tp.play_t = plat_ms();
}

void ui_frame(void)
{
    input();
    autosave();
    draw_header();
    draw_main();
    draw_knobs();
    leds();
    speak();
    ann_tick(plat_ms());
    ui.frame++;
}

void ui_input_only(void) { input(); }
