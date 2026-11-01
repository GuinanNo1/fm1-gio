# MOR-1

MOR-1 turns the M-VAVE FM-1 into a generative electronic disco machine, after Giorgio Moroder's Munich sound. It
plays itself from the moment it's switched on: a Moog-style bass sequence out in front, a pad of strings, a choir
or saws through a phaser, a synth lead that writes its own phrases, and four on the floor. Each song is a form of
sections with their own chord changes; it loops for ever and changes as it goes. You steer it.

> **Status: beta, simulator only.** MOR-1 has been built and tested in fm1-base's simulator. It has not yet been
> built for, or run on, an FM-1.

---

## Contents

1. Install
2. Getting started
3. Controls
4. The screen
5. The music
6. The songs
7. The pages
8. MIDI
9. Saving
10. Troubleshooting
11. Parameters
12. Credits

---

## 1. Install

MOR-1 has no release yet. Once it is built (`./build.sh`, see [BUILDING.md](../BUILDING.md)):

```
python3 base/tools/fm1_install.py app/build/mor.fwsc
```

with Python 3 and `pip3 install mido python-rtmidi`, the FM-1 on a USB data cable. Don't unplug it while it's
writing; when it finishes, the FM-1 restarts into MOR-1. fm1-base's web installer page (`tools/make_site.py`)
does the same from Chrome or Edge once a site is published.

**Go back:** the web installer's "Back to the stock firmware", or M-VAVE's own updater, M-UPGRADE, from
[m-vave.com/download](https://www.m-vave.com/download).

> Installing other firmware is at your own risk. If the FM-1 ever won't start, see Troubleshooting.

---

## 2. Getting started

1. Switch it on. MOR-1 plays the first song, **Feel**, from its intro.
2. Turn **PRESETS** for another song. It starts from the top of its form.
3. Press a **key**: the song moves to that key.
4. Turn **ALGORITHM** for another bass pattern; back to **Song** for the song's own.
5. Press **EDIT** to move on to the next section on the next bar; press **REC** to stay in this one.
6. Press **ENV** for the Bass page and turn KNOB 2: the cutoff opens.

---

## 3. Controls

| Control | What it does |
|---|---|
| Keys | The key. Any key moves the song to its note, at once. The keys light the notes of the chord playing. |
| PLAY | Plays / stops. From stopped, the song starts from its first section. |
| REC | HOLD: the section repeats until you press it again. |
| EDIT | The next section, on the next bar. |
| OCT− / OCT+ | The bass an octave down / up (−1 to +1). |
| ALGORITHM | The bass pattern: Song (the song's own, section by section), Pump, Pump 16, Arpeggio, Gallop, Octaves, Slow. |
| PRESETS | The song. Each remembers its settings as you left it. |
| HOME, ENV, ARP, LFO, SEQ, FX, GLO | The pages: Song, Bass, Lead, Pad, Drums, Space, Setup. |
| SELECT | The previous / next page. With HOME held: the tempo, 110 to 130. |
| KNOB 1–4 | The page's four values. A quick turn speeds up; a slow one goes a step at a time. |
| SAVE | Saves all the songs. Held 1.5 s: the song back to its recipe (then SAVE to keep that). |
| MASTER | The volume. |

Hold any button for a second and a card says what it does; let go and nothing else happens.

---

## 4. The screen

![The Song page](img/screen-song.png "The Song page")

- **The top line:** the song in a magenta tag, its key; on the right a beat light (a play triangle while stopped),
  the tempo, a dot while something isn't saved, and the battery. Messages show here for a moment.
- **The form:** a block for each section, as wide as its bars. The one playing is lit, filling as it goes; the one
  coming next is a shade brighter than the rest. While held, it turns ice blue. Under it: the section's name and the bar (3/16).
- **The ring:** the bass pattern's sixteen steps, the first at the top. A step's size and brightness is how high its
  note is; a ring around it is an accent; small dots are rests. The step playing is bright. In the middle, the chord,
  and under it the lead's note as it plays.
- **The meters** (left): bass, pad, lead, drums. A dark track means the layer is out of this section.
- **The drums** (right): kick, snare, closed hat, open hat, tom; this bar's hits, the step playing bright.
- **The knobs:** the page's four values; the one being turned is lit.

| | |
|---|---|
| ![Heaven at full tempo](img/screen-heaven.png) | ![Held](img/screen-hold.png) |

---

## 5. The music

**The bass** is the loudest thing: two saws and a square an octave down through a four-pole ladder low-pass, a
short decay on its level and its filter (Cutoff, Emphasis and Decay on the Bass page). It steps through a 16-step
pattern written in chord degrees (root, third, fifth, seventh, octave), so it follows the chords. Most of the echo is
its own: the dry note sits a little left, its repeat right, as on *I Feel Love*. With **Evolve**, a weak step changes
now and then (another chord note, a ghost note, a rest), and goes back home a step at a time.

**The pad** holds the chord on four voices, each moving as little as it can from chord to chord: **Strings** (two
saws), a **Choir** (a soft wave through two vowel formants, which **Bright** opens) or **Saws** (wider, swept). Then a
low-pass and a stereo **phaser** (four stages a side, swept slowly in quadrature, fed back).

**The lead** writes a four-bar phrase for each section it plays in: a motif, the motif answered, a second motif, the
first again; or, when **Busy** is low, a call and room for an answer. Its notes walk the song's scale, landing on the
chord on the strong beats, never a semitone off it. With **Evolve**, a note moves or the middle bar is rewritten at
each repeat. Voices: **Saw** (two, detuned), **Square**, **Robot** (a square through two formants, short notes, no
vibrato). **Glide** slides between notes; past 60 it also swoops up into a phrase.

**The drums**: a kick on every beat (on 1 and 3 in a half-time section), a snare-clap on 2 and 4, closed hats in
eighths or sixteenths (Hats), an open hat off the beat, and syndrum toms that fall in pitch. At the end of a section
there is often a fill (**Fills**: how often, and from 60 on, longer and with toms). A building section ends in a snare
roll. With **Evolve**, a syndrum or a ghost snare now and then.

**The form**: each song is a list of sections (Intro, Verse, Chorus, Break ...), each with its bars, its chord
changes, which layers play, a transposition and a few flags (half time, a build, a break). After the last section the
song goes back to a section of its own and goes round again. **Arrange** sets how freely it goes:

- a section may play twice;
- the song may jump to another section of its loop;
- a layer may sit a section out (the lead, the pad, the hats, the snare), or the lead come in where it wasn't;
- at the start of a cycle, the whole song may move up a tone, a semitone or a minor third (or down a tone) for a
  cycle, and come home after.

**Motion** is the slow sweep over minutes: the bass's filter opening and closing, the pad brightening, the phaser's
pace. A building section opens the bass filter as it goes; a break closes it and pushes the echo.

**The space**: a tape echo (two heads, the first at the echo's time on the right, the second at twice it on the left,
each repeat darker) and Dattorro's plate reverb.

---

## 6. The songs

Each song is after a record: its tempo, key, chord changes and the shape of its arrangement. The bass lines, leads
and drum parts are MOR-1's own, written as it plays.

| # | Song | After | Tempo | Key | The form |
|---|---|---|---|---|---|
| 1 | Feel | *I Feel Love* | 126 | C | Intro (bass, hats) · Verse (C) · Chorus (C–E♭–F–G) · Shift (on B♭) · Verse · Chorus · Break |
| 2 | Click | *Giorgio by Moroder* | 113 | A minor | Click (hats on the beat) · Arpeggio · Disco (Am–Em) · Strings (Am–F) · Fills · House |
| 3 | Mutant | *Supernature* | 123 | F minor | Dawn (the pad alone) · Sequence · Groove · Theme (Fm–D♭) · Bridge (B♭m–Fm) · Theme |
| 4 | Heaven | *The Number One Song in Heaven* | 130 | A♭ | Slow ×2 (half time, A♭–E♭–Fm11–E♭) · Lift · Fast (A♭–E♭–B♭–A♭) · Vamp (Fm11) · Fast |
| 5 | Clock | *Beat the Clock* | 126 | C | Intro · Verse (G–F) · Pre (Am–Dm–G–C) · Chorus (G–Em7–G) · Turn (C–E♭) · the same a minor third up · Outro |
| 6 | Energy | *Menergy* | 123 | C minor | Intro · Groove (Cm) · Lift (A♭–B♭) · Groove (Cm–B♭) · Break · Build |
| 7 | Fly | *Magic Fly* | 126 | A minor | Liftoff · Theme (Am–F–G) · Drift (Am–Dm) · Theme · Space |
| 8 | Lover | *Automatic Lover* | 130 | C minor | Intro · Verse (Cm–B♭) · Hook (A♭–B♭–Cm) · Robot · Hook |
| 9 | Express | *Chase* | 128 | D minor | Intro (the bass alone) · Chase · Strings · Theme (Dm–B♭–C) · Pursuit · Theme · Run |

Where the facts came from: tempo and key from published analyses (Hooktheory, Wikipedia) where there are some, and
from AcousticBrainz's automatic detection where not; the chord changes of *Beat the Clock*, *The Number One Song in
Heaven* and *I Feel Love* from published chord charts. *Supernature*, *Menergy*, *Magic Fly*, *Automatic Lover* and
*Chase* have no published changes, so theirs are MOR-1's own in the record's manner, and so are *Chase*'s key and
tempo. *Automatic Lover* (about 132) is set to 130 to stay within MOR-1's range.

---

## 7. The pages

| Page (button) | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| Song (HOME) | Tempo | Evolve | Motion | Arrange |
| Bass (ENV) | Level | Cutoff | Emphasis | Decay |
| Lead (ARP) | Level | Voice | Busy | Glide |
| Pad (LFO) | Level | Colour | Bright | Phaser |
| Drums (SEQ) | Level | Fills | Hats | Swing |
| Space (FX) | Echo | Time | Feedback | Reverb |
| Setup (GLO) | Key | Tune | MIDI out | Lights |

The Setup page also shows the audio's load (CPU) in the top line.

---

## 8. MIDI

- **In** (USB and the TRS jack): a note on channel 1 moves the key. MIDI clock drives the grid (EXT in the top line);
  Start plays the song from the top, Stop stops, Continue goes on.
- **Out** (USB, when MIDI out is on): the bass on channel 1, the pad on 2, the lead on 3, the drums on 10 (General
  MIDI: 36 kick, 39 clap, 42 and 46 hats, 45 tom), and MIDI clock.

The FM-1 also shows up on a computer as a USB audio input: MOR-1 can be recorded over the cable.

---

## 9. Saving

Each song keeps its settings as you left it. **SAVE** saves all of them; MOR-1 also saves by itself a few seconds
after a change while it is stopped (a save silences the sound for a moment, so never while playing). **SAVE held**
puts the song back to its recipe. Tune and MIDI out belong to the FM-1, not to a song.

---

## 10. Troubleshooting

**Update mode:** hold OCT− and OCT+ together for five seconds. A countdown appears; let go before it ends to cancel.

**Calibration:** if a button or knob does the wrong thing, hold OCT− and OCT+ while you switch the FM-1 on, and follow
the screen.

**After a crash:** a red CRASH screen, then a restart a few seconds later; the next start shows RESTARTED AFTER A
CRASH and an address. If MOR-1 crashes twice in a row while starting, it starts in SAFE MODE: no sound, but USB
works, so the installer can reinstall it or put the stock firmware back. If even that fails, the FM-1 drops into
the chip's own update mode; fm1-base's rescue (`felucca-bsp/tools/fm1_rescue.sh`, on a Mac) puts M-VAVE's firmware
back from there.

---

## 11. Parameters

| Parameter | Range | What it does |
|---|---|---|
| Tempo | 110–130 | Beats a minute |
| Evolve | 0–100 | How much the bass pattern, the lead's phrases and the drums change as they repeat |
| Motion | 0–100 | How far the slow sweeps go (bass filter, pad brightness, phaser's pace) |
| Arrange | 0–100 | How freely the form goes: repeats, jumps, layers out and in, a cycle in another key |
| Bass Level · Cutoff · Emphasis · Decay | 0–100 | The bass's level, its filter, the filter's resonance, the length of its notes and envelope |
| Lead Level · Busy · Glide | 0–100 | The lead's level, how many notes a bar (and whether it leaves room), its portamento |
| Lead Voice | Saw, Square, Robot | |
| Pad Level · Bright · Phaser | 0–100 | The pad's level, its brightness (the choir's vowel), the phaser's depth and feedback |
| Pad Colour | Strings, Choir, Saws | |
| Drums Level · Fills · Hats | 0–100 | The kit's level, how often and how long the fills, the hats' colour and density |
| Swing | 50–75% | The off sixteenths late |
| Echo · Feedback · Reverb | 0–100 | The echo's send (mostly the bass), its repeats, the plate's send |
| Echo time | 1/16, 1/8, 1/8., 1/4, 1/4., 1/2 | In time with the tempo |
| Key | C–B | The song's key |
| Tune | ±50 cents | |
| MIDI out | Off, On | |
| Lights | Off, On, Keys | The keys (and the buttons' glow) |
| Pattern (ALGORITHM) | Song, Pump, Pump 16, Arpeggio, Gallop, Octaves, Slow | |
| Bass octave (OCT) | −1, 0, +1 | |

---

## 12. Credits

A fork of [AMB-1](https://github.com/charlesvestal/fm1-amb) by Charles Vestal: its platform, panel, plate reverb,
tape echo and screen reader. Built on [fm1-base](https://github.com/charlesvestal/fm1-base) and
[felucca-bsp](https://github.com/hugelton/felucca-bsp), from [Felucca](https://github.com/hugelton/Felucca) by Leo
Kuroshita (Hügelton Instruments). The reverb is Jon Dattorro's plate ("Effect Design, Part 1", JAES 1997). MPL-2.0;
see [LICENSING.md](../LICENSING.md).

M-VAVE and FM-1 are trademarks of their owners. MOR-1 is not affiliated with M-VAVE, Giorgio Moroder or any artist
named here.
