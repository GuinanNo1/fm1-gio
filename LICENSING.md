# MOR-1 licensing

MOR-1 is free software under the **Mozilla Public License 2.0** (`MPL-2.0`, full text in `LICENSE`), file by file.
If you change one of its files and share the result, share that file's source under the MPL. It is a fork of
[AMB-1](https://github.com/charlesvestal/fm1-amb) (MPL-2.0) and is built on
[fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp)
(`base/`, a git submodule), both MPL-2.0. It takes no code from DUB-1 or NACHTFAHRT (GPL-3.0), only ideas.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| The platform: `base/` (boot, USB and USB audio, the TRS jack, storage, the update path, screen, panel, the screen reader, the host simulator and the browser emulator, build and install tools) | [fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp), from [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (@kurogedelic), Hügelton Instruments, under MPL-2.0 with his permission ([felucca-bsp#4](https://github.com/hugelton/felucca-bsp/issues/4)) | MPL-2.0 |
| From AMB-1, Charles Vestal: the app's frame (`app.c`, `app.h`, `project.c`, `host.c`), the UI's input, drawing, help cards, autosave and screen reader (`ui.c`), and in `dsp/mor.c` the Dattorro plate (from FoMni-1), the tape echo, voice leading, the command queue and the clock | AMB-1 | MPL-2.0 |
| MOR-1's own: the engine's bass sequencer, pad and phaser, lead and its phrase generator, drums, the song forms and the arrangement (`dsp/mor.c`, `dsp/mor.h`), the screen's design, the songs, `tests/` | MOR-1 | MPL-2.0 |

## The songs

MOR-1's nine songs are written after records by Donna Summer, Daft Punk, Cerrone, Sparks, Patrick Cowley and
Sylvester, Space, Dee D. Jackson and Giorgio Moroder: their tempo, key, chord changes and the shape of their
arrangement, as data in `dsp/mor.c`. No melody, bassline, lyric, recording or sample of any of them is in MOR-1: the
generator writes its own lines in each song's style. The songs' names are its own; the records they are after are
named only to say what they are after.

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Fonts: Barlow Semi Condensed (The Barlow Project Authors), Terminus (Dimitar Toshkov Zhekov) | SIL OFL 1.1 | `base/assets/fonts/` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. "M-VAVE" and "FM-1" are trademarks of their
respective owners, used here only to say what hardware MOR-1 runs on. MOR-1 is independent firmware; it is not
affiliated with, endorsed by or supported by any of them, nor by Giorgio Moroder or any artist named here.

## Radio

MOR-1 never enables the Bluetooth / Wi-Fi radio of the hardware.
