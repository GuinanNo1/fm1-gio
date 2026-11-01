# MOR-1 for the M-VAVE FM-1

MOR-1 turns the M-VAVE FM-1 into a generative electronic disco machine, after Giorgio Moroder's Munich sound. It
plays itself from the moment it's switched on: a Moog-style bass sequence out in front, its repeats on the other
side as on *I Feel Love*; strings, a choir or detuned saws through a phaser; a synth lead (ten voices) that writes its
own phrases, which you can play from the white keys and which answers you; four on the floor with claps, open hats and syndrum fills. Each song is a form (intro, verse, chorus,
break) with its own chord changes, and it loops for ever, changing as it goes: a section repeated, a layer let
out for a while, the whole song moved up a key for a cycle. You steer it: pick a song, a key, a bass pattern,
hold a section, push it on to the next.

![The Song page: Feel's verse, the bass's sixteen steps round the chord, the form across the top](docs/img/screen-song.png) ![Heaven at full tempo](docs/img/screen-heaven.png) ![The Bass page](docs/img/screen-bass.png)

**Status: beta, simulator only.** It runs in fm1-base's simulator and passes its tests; it has not yet been built
for or run on an FM-1. See [BUILDING.md](BUILDING.md) for the toolchain.

## The songs

Nine songs, each after a record: its tempo, key, chord changes and the shape of its arrangement. The lines are
MOR-1's own, written as it plays.

| # | Song | After | Tempo | Key | Bass | What's taken |
|---|---|---|---|---|---|---|
| 1 | Feel | *I Feel Love*, Donna Summer (1977) | 126 | C | Pump (eighths, octaves) | the dry-left, echo-right eighths; a vamp on C, the chorus C–E♭–F–G, the riff moving to B♭ |
| 2 | Click | *Giorgio by Moroder*, Daft Punk (2013) | 113 | A minor | Arpeggio | a click, the arpeggio, disco, strings, drum fills, a house beat; Am–Em, Am–F |
| 3 | Mutant | *Supernature*, Cerrone (1977) | 123 | F minor | Gallop | a long build from the pad alone; a minor vamp, i–♭VI, iv–i (the changes are MOR-1's) |
| 4 | Heaven | *The Number One Song in Heaven*, Sparks (1979) | 130 | A♭ | Slow, then Pump 16 | a half-time first half (A♭–E♭–Fm11–E♭), then full tempo (A♭–E♭–B♭–A♭, an Fm11 vamp) |
| 5 | Clock | *Beat the Clock*, Sparks (1979) | 126 | C, then E♭ | Pump 16 | G–F verses, Am–Dm–G–C, G–Em7–G; the turn through C–E♭ and the whole song again a minor third up |
| 6 | Energy | *Menergy*, Patrick Cowley (1981) | 123 | C minor | Octaves | the hi-NRG off-beat octave bass; a minor vamp with A♭–B♭ lifts (the changes are MOR-1's) |
| 7 | Fly | *Magic Fly*, Space (1977) | 126 | A minor | Octaves | the gliding lead out in front, strings; Am–F–G, Am–Dm (the changes are MOR-1's) |
| 8 | Lover | *Automatic Lover*, Dee D. Jackson (1978) | 130 | C minor | Pump 16 | the Robot lead; Cm–B♭, A♭–B♭–Cm (the changes are MOR-1's) |
| 9 | Express | *Chase*, Giorgio Moroder (1978) | 128 | D minor | Pump 16 | a bass alone, then layer on layer, rising; Dm–B♭–C, strings under a phaser (key, tempo and changes are MOR-1's) |

Tempo and key come from published analyses where there are some; several are machine-detected, and three of the
records' chord changes aren't published at all, so those are MOR-1's own in the record's manner. All of it is a
table in `app/dsp/mor.c` (`SONGS`), easy to correct.

## In short

| | |
|---|---|
| White keys | The lead, in the song's scale (the chord's notes lit); a call of three notes and a pause, and the lead answers and takes it as its theme |
| Black keys | Eleven drop-in effects, each while held |
| SEL | The Effects page: sixteen effects on the white keys (REC held: the same, while held) |
| PLAY · REC · EDIT | Play / stop · a tap holds the section · the next section, on the next bar |
| HOME · ENV · ARP · LFO · SEQ · FX · GLO | The pages: Song, Bass, Lead (again: Lead FX), Pad, Drums, Space, Setup (again: MIDI); KNOB 1–4 set their values |
| SELECT | The previous or next page; with HOME held, the tempo (110–130) |
| ALGORITHM · PRESETS | The bass pattern · the song |
| OCT− / OCT+ | The bass's octave |
| SAVE | Saves; held, puts the song back to its recipe |

Hold any button for a second and a card says what it does. The [manual](docs/MANUAL.md) has the rest.

## Building and testing

MOR-1 is an app on [fm1-base](https://github.com/charlesvestal/fm1-base) (`base/`, a git submodule); clone with
`--recursive`. The toolchain and prerequisites are in [BUILDING.md](BUILDING.md):

```
sh tests/run_tests.sh                                  # the instrument, the app in the simulator
./build.sh --host                                      # the simulator: app/build/host/mor_host
./build.sh                                             # app/build/mor.fwsc (needs JieLi's toolchain and SDK)
python3 base/tools/fm1_install.py app/build/mor.fwsc   # install it over USB
```

`app/build/host/mor_host SCRIPT OUTDIR` runs the whole app on a computer from a script (`tests/scenarios/*.mor`),
with audio and screenshots coming out. `tests/host/mor_render.c` renders one song's engine alone to a WAV, with
its sections, levels and the engine's time:

```
cc -O2 -ffp-contract=off -DEN_HOST -Iapp/dsp -Ibase/include -o mor_render tests/host/mor_render.c app/dsp/mor.c -lm
./mor_render 3 180 heaven.wav          # song 4, three minutes
CHORDS=1 ./mor_render 4 120 clock.wav  # with every chord change printed
```

## Credits

A fork of [AMB-1](https://github.com/charlesvestal/fm1-amb) by Charles Vestal, whose platform, panel, plate reverb
and tape echo it keeps. The idea of a song form that loops and changes is [DUB-1](https://github.com/sbegueria/fm1-dub)'s
and [NACHTFAHRT](https://github.com/FartyPants/fm1-nachtfahrt)'s (no code from either). Built on
[fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp),
from [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton Instruments). MPL-2.0; see
[LICENSING.md](LICENSING.md).

M-VAVE and FM-1 are trademarks of their owners. MOR-1 is not affiliated with M-VAVE, Giorgio Moroder, any artist
named here, or Hügelton Instruments.
