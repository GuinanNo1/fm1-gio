/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal, Ian Guinan */
/* The instrument: a generative electronic disco machine, after Giorgio Moroder's Munich sound. Four layers play
 * one song form (sections of bars, each its own chord changes and its own layers) that loops for ever and changes
 * as it goes:
 *
 *   BASS    the sequence, the loudest thing: a Moog-style voice (two saws and a square an octave down, a four-pole
 *           ladder low-pass, a short decay on both the filter and the level) stepping through a 16-step pattern. A
 *           pattern is a shape (pump, arpeggio, gallop, octaves, ...) written in chord degrees, so it follows the
 *           chords; Evolve changes a step now and then, and the bar after it may change back. Most of the echo is
 *           its: the dry note on one side and its repeat on the other.
 *   PAD     four voices on the chord (strings, a choir, or detuned saws through a filter), voice-led from chord to
 *           chord, through a stereo phaser.
 *   LEAD    a monophonic synth line with glide and vibrato: a phrase of short motifs written for each section, its
 *           notes on the chord on the strong beats and in the scale between, answered and varied as it repeats.
 *   DRUMS   four on the floor, a clap-snare on two and four, closed and open hats, and syndrum toms that fall in
 *           pitch, for fills at the ends of sections.
 *
 * Over it: the FORM. Each song is a list of sections (intro, verse, chorus, break ...), each with its bars, its
 * chord changes, which layers play, a transposition and a few flags (half time, a build, a break). The song plays
 * through and loops back to a section of its own; ARRANGE sets how freely it goes: repeating a section, going to
 * another, dropping a layer for a while, moving the whole song to another key for a cycle. MOTION is the slow
 * sweeps: the bass filter opening over minutes, the pad brightening, the phaser's pace.
 * Then: a tape echo (two heads, each repeat darker), the phaser on the pad, Dattorro's plate.
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render drains at the
 * start of a block; the state below marked (UI) is written by the render and only read elsewhere. Float only, no
 * libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NPAD 4                      /* pad voices */
#define EN_NDRUM 5                     /* kick, snare, closed hat, open hat, tom */
#define EN_STEPS 16                    /* the bass pattern: sixteenths of a bar */

/* parameters: value ranges in en_param_info() */
enum {
    P_TEMPO, P_EVOLVE, P_MOTION, P_ARRANGE,           /* the song */
    P_BASS, P_CUTOFF, P_RESO, P_DECAY,                /* the bass sequence */
    P_LEAD, P_LVOICE, P_BUSY, P_GLIDE,                /* the lead */
    P_PAD, P_PCOLOUR, P_BRIGHT, P_PHASER,             /* the pad */
    P_DRUMS, P_FILLS, P_HATS, P_SWING,                /* the drums */
    P_ECHO, P_ETIME, P_FEEDBACK, P_REVERB,            /* space */
    P_ROOT, P_TUNE, P_MIDI,                           /* setup */
    P_PATTERN, P_BOCT, P_LOCT,                        /* ALGORITHM, OCT- / OCT+, the lead's octave */
    P_LECHO, P_LREV, P_LTONE,                         /* the lead's own echo and reverb sends, its brightness */
    P_MCHAN, P_MCLOCK, P_MCLKOUT,                     /* MIDI: the channel set, the clock's source, clock out */
    P_NPARAMS
};
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
void en_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */

extern const char *const EN_NOTE_NAME[12];
#define EN_NPATTERNS 7                         /* P_PATTERN: 0 the song's, then each shape */
extern const char *const EN_PATTERN_NAME[EN_NPATTERNS];

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_play(int on);                          /* the song plays / stops (the tails ring on) */
void en_next(void);                            /* the next section, on the next bar */
void en_hold(int on);                          /* the section repeats until let go */
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_song(int i);                           /* a song: its form from the top (its parameters come by en_set) */
/* the drop-in effects (the Effects page): bit i of the mask on while held; each eases in and out */
enum { FX_THROW, FX_TIME, FX_TAPESTOP, FX_SWELL, FX_LOWPASS, FX_HIGHPASS, FX_SWEEP, FX_PHASER,
       FX_REVERB, FX_WASH, FX_FREEZE, FX_DROP, FX_STUTTER, FX_GATE, FX_OCTAVE, FX_ROLL, EN_NFX };
extern const char *const EN_FX_NAME[EN_NFX];   /* "Throw", "Time x2" ... (the grid's labels) */
extern const char *const EN_FX_ROW[4];         /* "Echo", "Filter", "Space", "Moves" */
void en_fx(uint32_t mask);
extern volatile uint16_t en_fx_on;             /* (UI) the effects held now */
/* the white keys play the lead: step 0 the key's note, then up the song's scale; three notes or more and a pause, and
 * the lead answers and takes them as its theme */
void en_lead_key(int step, int on);
extern volatile int8_t en_lead_step;           /* (UI) the step held now, -1 none */
int en_step_pc(int step);                      /* (UI) the pitch class white key `step` plays now */
/* MIDI clock in: ticks drive the 1/16 grid (6 a step) while they keep coming */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* the songs: recipes for every parameter (Tune and MIDI out are the player's) and a form */
#define EN_NSONGS 9
#define EN_MAXSEC 12
const char *en_song_name(int i);
void en_song_recipe(int i, int16_t *par);
int en_song_nsec(int i);
const char *en_sec_name(int song, int sec);
int en_sec_bars(int song, int sec);
int en_sec_prog(int song, int sec);            /* which of the song's chord changes the section plays */

/* the chord changes (the Chords page): each song has EN_NPROG progressions, EN_PROG_MAX chords each, a chord packed
 * as its root (semitones above the key) | its quality << 4 | its length in beats << 8; a length of 0 ends the
 * progression (the first chord always has one). The tables are live: en_prog_set writes one at once (a 16-bit store),
 * and the render takes it up on the next beat. */
#define EN_NPROG 5
#define EN_PROG_MAX 8
#define EN_NQUAL 8                             /* major, minor, 7, maj7, m7, m11, sus4, power */
#define EN_CH(r, q, beats) ((uint16_t)((unsigned)(r) | (unsigned)(q) << 4 | (unsigned)(beats) << 8))
#define EN_CH_R(c) ((int)((c) & 15u))
#define EN_CH_Q(c) ((int)((c) >> 4 & 15u))
#define EN_CH_BEATS(c) ((int)((c) >> 8))
extern const char *const EN_QUAL_SUFFIX[EN_NQUAL];   /* "", "m", "7", "M7", "m7", "m11", "sus4", "5" */
uint16_t en_prog_recipe(int song, int prog, int i);  /* the song's own */
uint16_t en_prog_get(int song, int prog, int i);
void en_prog_set(int song, int prog, int i, uint16_t c);
extern volatile int8_t en_prog_slot;           /* (UI) the chord playing in its progression, -1 none */

/* layers, as a section's mask */
enum { L_BASS = 1, L_PAD = 2, L_LEAD = 4, L_KICK = 8, L_SNARE = 16, L_HATS = 32, L_OHAT = 64, L_TOMS = 128 };

/* state for the screen (UI) */
extern volatile uint8_t en_playing, en_ext, en_held;
extern volatile uint8_t en_song_i, en_sec, en_sec_bar, en_sec_len, en_next_sec;
extern volatile uint8_t en_layers;             /* what plays now (L_*) */
extern volatile int8_t en_key;                 /* the song's key now: the root plus the cycle's move, pitch class */
extern volatile uint8_t en_chord[EN_NPAD];     /* the pad's notes (MIDI) */
extern volatile uint8_t en_chord_root, en_chord_n;
extern volatile uint8_t en_step;               /* the sixteenth in the bar, 0..15 */
extern volatile int8_t en_bpat[EN_STEPS];      /* the bass pattern: -1 rest, else its note now (MIDI) */
extern volatile uint8_t en_bacc[EN_STEPS];     /* accented steps */
extern volatile float en_bass_glow, en_lead_glow, en_pad_level, en_bass_cut;   /* en_bass_cut 0..1: the filter */
extern volatile uint8_t en_lead_note;          /* the lead's last note (MIDI), 0 none */
extern volatile float en_drum_glow[EN_NDRUM];
extern volatile uint32_t en_dpat[EN_NDRUM];    /* this bar's hits, bit i = step i */
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */
extern volatile float en_arc;                  /* the slow sweep, -1 .. +1 (times Motion) */

/* the pure parts, for the tests and the screen */
int en_chord_tones(int root, int quality, int *pcs);   /* a quality's pitch classes from root; count (3 or 4) */
void en_voice_lead(const int *from, const int *pcs, int ntones, int *to, int lo, int hi);
const char *en_chord_name(const int *pcs, int n, char *buf);   /* "Em7", "Dsus4", "C" ... */
