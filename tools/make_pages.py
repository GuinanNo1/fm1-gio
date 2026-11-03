#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal, Ian Guinan
"""Make the GIO-1 site for GitHub Pages (https://GuinanNo1.github.io/fm1-gio/), dark, after AMB-1's:

  index.html                  what GIO-1 is, and the ways in: try, install, manual, quickstart, download, source
  img/                        screenshots (docs/img)
  emu/                        GIO-1 in the browser (app/build/emu, from ./build.sh --web), with its guide
  quickstart/index.html       one page: every control, page, effect and song
  manual/index.html, img/     the manual (fm1-base's tools/manual_html.py, from docs/MANUAL.md), in the site's colours
  install/index.html          fm1-base's web installer, when a package is given; else a page saying it's coming
  firmware/gio-VERSION.fwsc   the package the installer writes; also the download (when given)
  words/                      the screen reader's words files (app/web/words)

  tools/make_pages.py OUT_DIR [PKG VERSION]

A package must be a release build (./build.sh --release X.Y): its identity, FM-1_58XXYYZZ, is what the installer
checks the download against and what the FM-1 reports after the install."""
import hashlib
import html
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
BASE = SRC / "base"
sys.path.insert(0, str(BASE / "tools"))
from make_site import MBOOT_VERSION, product_of, read_conf, strip_module  # noqa: E402  (fm1-base's: the package format)

REPO = "https://github.com/GuinanNo1/fm1-gio"
FONTS = ('<link rel="preconnect" href="https://fonts.googleapis.com">\n'
         '<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>\n'
         '<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow:ital,wght@0,400;0,600;1,400'
         '&family=Barlow+Semi+Condensed:wght@500;700&display=swap">\n')

# the site's colours: GIO-1's own screen. A night-club navy, the cards a shade up; magenta for what's live; the bass's
# cyan, the pad's violet, the lead's gold for the three kinds of thing
TOKENS = """
:root {
  --night: #100e22; --card: #1c1a3a; --card-2: #262350; --rule: #3a3668; --ink: #f2eeff; --muted: #aaa4d4;
  --magenta: #ff5cb4; --cyan: #2fd8f0; --violet: #b994ff; --gold: #ffc840; --red: #ff7a7a;
  --display: "Barlow Semi Condensed", "Arial Narrow", "Helvetica Neue", Arial, sans-serif;
  --body: "Barlow", "Helvetica Neue", Helvetica, Arial, sans-serif;
  color-scheme: dark;
}
"""
HEADER_CSS = """
header.site { max-width: 46rem; margin: 0 auto; display: flex; flex-wrap: wrap; gap: .4rem 1.2rem; align-items: baseline;
              padding: 18px 0 14px; border-bottom: 1px solid var(--rule); }
header.site .home { font: 700 1.15rem/1 var(--display); letter-spacing: .08em; text-decoration: none; color: var(--ink); }
header.site nav { display: flex; flex-wrap: wrap; gap: .2rem 1rem; font: 500 .95rem/1.4 var(--display);
                  letter-spacing: .05em; text-transform: uppercase; }
header.site nav a { text-decoration: none; color: var(--muted); }
header.site nav a:hover, header.site nav a[aria-current] { color: var(--magenta); }
"""
THEME = TOKENS + """
body { background: var(--night); color: var(--ink); margin: 0; font: 400 17px/1.6 var(--body);
       padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px));
       padding-block: 0 64px; }
a { color: var(--ink); text-decoration-color: var(--magenta); text-decoration-thickness: 2px; text-underline-offset: 3px; }
a:hover { color: var(--magenta); }
a:focus-visible, button:focus-visible { outline: 2px solid var(--cyan); outline-offset: 2px; }
code { font: .88em ui-monospace, Menlo, Consolas, monospace; background: var(--card-2); padding: .08em .35em; border-radius: 3px; }
.foot { max-width: 46rem; margin: 3rem auto 0; color: var(--muted); font-size: .88rem; border-top: 1px solid var(--rule);
        padding-top: 1rem; }
""" + HEADER_CSS

NAV = [("emu/", "Try it"), ("install/", "Install"), ("manual/", "Manual"), ("quickstart/", "Quickstart"), ("mirror/", "Mirror")]


def mirror_pages(conf):
    """{"mirror/index.html": html} with mirror = true: tools/fm1base_web's page, filled in, with GIO-1's two changes
    (tools/fm1base_web/README.md)"""
    if not conf.get("mirror"):
        return {}
    web = SRC / "tools" / "fm1base_web"
    mh = (web / "mirror.html").read_text(encoding="utf-8")
    mh = mh.replace("{{NAME}}", "GIO-1").replace("{{PORTS_JS}}", strip_module((web / "fm1ports.js").read_text(encoding="utf-8")))
    for old, new in (
            ('navigator.permissions?.query({ name: "midi", sysex: true }).then((p) => { if (p.state === "granted") connect(); }).catch(() => {});',
             "// (GIO-1: no connecting by itself when the page opens: Connect does it)"),
            ('addEventListener("pagehide", () => { if (control) setControl(false); send(0); });',
             "// (GIO-1: nothing sent as the page closes; the FM-1 stops mirroring by itself, MR_TIMEOUT_MS)")):
        if mh.count(old) != 1:
            raise SystemExit(f"tools/fm1base_web/mirror.html: not found once: {old[:50]}...")
        mh = mh.replace(old, new)
    left = sorted(set(re.findall(r"\{\{[A-Z_]+\}\}", mh)))
    if left:
        raise SystemExit(f"tools/fm1base_web/mirror.html: {' '.join(left)} not filled in")
    return {"mirror/index.html": mh}


def header(here, depth):
    up = "../" * depth
    links = "".join('<a href="%s%sindex.html"%s>%s</a>' % (up, p, ' aria-current="page"' if p == here else "", t)
                    for p, t in NAV)
    return (f'<header class="site"><a class="home" href="{up}index.html">GIO-1</a><nav aria-label="GIO-1">{links}'
            f'<a href="{REPO}">Source</a></nav></header>')


FOOT = """<p class="foot">M-VAVE and FM-1 are trademarks of their owners. GIO-1 is independent firmware, not affiliated with
M-VAVE, Giorgio Moroder or any artist named here. To go back to the official firmware, use the installer's "Back to the
stock firmware", or M-VAVE's updater, M-UPGRADE, from <a href="https://www.m-vave.com/download">m-vave.com/download</a>.</p>"""


def page(title, desc, body, here, depth, css=""):
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>{title}</title>
<meta name="description" content="{html.escape(desc)}">
<meta name="theme-color" content="#100e22">
{FONTS}<style>{THEME}{css}</style>
</head>
<body>
{header(here, depth)}
{body}
{FOOT}
</body>
</html>
"""


def logo_svg():
    """GIO-1's start-up logo (app/gio_splash.c: the same shapes in its design units, 290 x 64, its chrome stops and its
    star at full size), as an SVG for the home page: the counters cut out (the page shows through)"""
    # (x, y, w, h, radius, 1 = the chrome, 0 = cut out), in drawing order, as gs_word draws them
    shapes = [(0, 0, 76, 64, 32, 1), (18, 18, 40, 28, 14, 0), (46, 0, 30, 32, 0, 0), (58, 32, 18, 32, 0, 1),
              (46, 46, 30, 18, 0, 1), (90, 0, 18, 64, 9, 1), (122, 0, 76, 64, 32, 1), (140, 18, 40, 28, 14, 0),
              (212, 23, 30, 18, 9, 1), (256, 0, 34, 18, 9, 1), (272, 0, 18, 64, 9, 1)]
    stops = [(0, "#CFE9FF"), (28, "#1E5FD0"), (29, "#E8F6FF"), (32, "#6FB4FF"), (63, "#0A2A7A")]   # (row, colour)
    cx, cy, r = 133, 8, 14                     # the star: on the O's rim; its arms' inner corners at r / 5
    k = r / 5
    star = " ".join(f"{cx + x:g},{cy + y:g}" for x, y in
                    [(0, -r), (k, -k), (r, 0), (k, k), (0, r), (-k, k), (-r, 0), (-k, -k)])
    mask = "".join(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rad}" fill="{"#fff" if on else "#000"}"/>'
                   for x, y, w, h, rad, on in shapes)
    grad = "".join(f'<stop offset="{row / 64:.4f}" stop-color="{c}"/>' for row, c in stops)
    return (f'<svg class="logo" viewBox="0 -8 290 72" role="img" aria-label="GIO-1" xmlns="http://www.w3.org/2000/svg">'
            f'<defs><linearGradient id="chrome" x1="0" y1="0" x2="0" y2="64" gradientUnits="userSpaceOnUse">{grad}'
            f'</linearGradient><mask id="word" maskUnits="userSpaceOnUse" x="0" y="0" width="290" height="64">{mask}</mask>'
            f'</defs><rect x="0" y="0" width="290" height="64" fill="url(#chrome)" mask="url(#word)"/>'
            f'<polygon points="{star}" fill="#fff"/></svg>')


LANDING_CSS = """
main { max-width: 46rem; margin: 0 auto; }
.hero { padding: 2.6rem 0 1.2rem; }
h1 { margin: 0; line-height: 0; }
h1 .logo { display: block; width: min(100%, 26rem); height: auto; }   /* (the start-up screen's logo: logo_svg) */
.sub { font: 500 1.15rem/1.3 var(--display); letter-spacing: .1em; text-transform: uppercase; color: var(--muted);
       margin: .7rem 0 0; }
/* the form strip, as on GIO-1's screen: a block a section, the one playing filling */
.strip { display: flex; gap: 4px; margin: 1.4rem 0 0; height: 10px; }
.strip i { flex: var(--w); border-radius: 5px; background: var(--card-2); }
.strip i.on { background: linear-gradient(90deg, var(--magenta) 60%, var(--card-2) 60%); }
@media (prefers-reduced-motion: no-preference) {
  .strip i.on { background: linear-gradient(90deg, var(--magenta) 50%, var(--card-2) 50%); background-size: 200% 100%;
                background-position: 70% 0; animation: fill 7.6s linear infinite; }
  @keyframes fill { from { background-position: 100% 0; } to { background-position: 0 0; } }
}
.lede { font-size: 1.18rem; margin: 1.6rem 0 1.2rem; max-width: 36rem; }
.shots { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin: 1.4rem 0 2rem; }
.shots figure { margin: 0; min-width: 0; }
.shots img { display: block; width: 100%; max-width: 100%; height: auto; image-rendering: pixelated; border-radius: 10px;
             border: 6px solid #07061a; box-shadow: 0 0 0 1px var(--rule); box-sizing: border-box; }
.shots figcaption { color: var(--muted); font-size: .85rem; margin-top: .3rem; }
.ways { display: grid; gap: 10px; margin: 0 0 1rem; }
.ways a, .ways .off { display: block; text-decoration: none; background: var(--card); border: 1px solid var(--rule);
                      border-radius: 10px; padding: 12px 16px; }
.ways .off { border-style: dashed; }
.ways a:hover { border-color: var(--magenta); color: var(--ink); }
.ways a.first { background: var(--card-2); border-color: var(--magenta); }
.ways strong { display: block; font: 700 1.2rem/1.3 var(--display); letter-spacing: .06em; text-transform: uppercase; }
.ways strong em { font-style: normal; color: var(--gold); font-weight: 500; letter-spacing: .04em; margin-left: .4em; }
.ways span { color: var(--muted); font-size: .95rem; }
h2 { font: 700 1.25rem/1.2 var(--display); letter-spacing: .08em; text-transform: uppercase; margin: 2.6rem 0 .8rem;
     padding-top: .8rem; border-top: 1px solid var(--rule); }
.parts { display: grid; gap: 10px; padding: 0; margin: 0 0 1rem; list-style: none; }
.parts li { border-left: 4px solid var(--c); padding: 2px 0 2px 14px; }
.parts b { color: var(--c); font-family: var(--display); letter-spacing: .06em; text-transform: uppercase; }
.songs { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 8px; margin: 0 0 1rem;
         padding: 0; list-style: none; }
.songs li { background: var(--card); border-radius: 8px; padding: 8px 12px; }
.songs b { font: 700 1.05rem/1.2 var(--display); letter-spacing: .06em; text-transform: uppercase; }
.songs span { display: block; color: var(--muted); font-size: .9rem; font-variant-numeric: tabular-nums; }
.credits p { max-width: 38rem; }
"""

LANDING_BODY = """<main>
<section class="hero">
  <h1>LOGO_SVG</h1>
  <p class="sub">A generative electronic disco firmware for the M-VAVE FM-1</p>
  <div class="strip" aria-hidden="true"><i style="--w:8"></i><i class="on" style="--w:16"></i><i style="--w:8"></i><i style="--w:8"></i><i style="--w:16"></i><i style="--w:8"></i><i style="--w:8"></i></div>
</section>

<p class="lede">It's time to relive the futuristic electronic disco sounds of the '70s, '80s and '90s with GIO-1, new
firmware for the M-VAVE FM-1. It is generative: it writes music in these styles as it plays. Pick a song, play along on
the keys and let the lead answer you, drop in effects, and nudge it in the direction your feelings take you. Feel love
and enjoy!</p>

<div class="shots">
  <figure><img src="img/screen-song.png" width="240" height="240" alt="GIO-1's screen: the song Feel, its verse; the bass's sixteen steps in a ring round the chord C; the form across the top"><figcaption>Feel, its verse</figcaption></figure>
  <figure><img src="img/screen-heaven.png" width="240" height="240" alt="GIO-1's screen: the song Heaven at full tempo, the chord A flat"><figcaption>Heaven at full tempo</figcaption></figure>
  <figure><img src="img/screen-grid.png" width="240" height="240" alt="GIO-1's effects grid: sixteen effects in four rows, Sweep and Roll held"><figcaption>The effects grid</figcaption></figure>
</div>

<nav class="ways" aria-label="Get GIO-1">
  <a class="first" href="emu/index.html"><strong>Try it in the browser</strong><span>The same code the FM-1 runs, with sound. Mouse, touch or keyboard; no FM-1 needed.</span></a>
  __INSTALL__
  <a href="manual/index.html"><strong>Manual</strong><span>Getting started in a few minutes, then everything else.</span></a>
  <a href="quickstart/index.html"><strong>Quickstart</strong><span>Every control, page, effect and song on one page.</span></a>
  __DOWNLOAD__
  <a href="__REPO__"><strong>Source</strong><span>GitHub, MPL-2.0. Built on fm1-base and felucca-bsp, from Felucca by Hügelton Instruments.</span></a>
</nav>

<h2>What plays</h2>
<ul class="parts">
  <li style="--c: var(--cyan)"><b>Bass.</b> An analog-style sequence out in front: two saws and a square through a four-pole ladder (or an FM voice), short decays, the dry note on one side and its echo on the other. Seven pattern shapes, written in chord degrees so they follow the changes.</li>
  <li style="--c: var(--violet)"><b>Pad.</b> Strings, a choir or detuned saws on the chord, voice-led, through a slow stereo phaser.</li>
  <li style="--c: var(--gold)"><b>Lead.</b> Ten voices, from Mini and Odyssey to Theremin, FM Brass and Robot. It writes its own phrases. Play the white keys and it follows your line, answers it, and makes it the song's theme.</li>
  <li style="--c: var(--red)"><b>Drums.</b> Four on the floor, claps on two and four, open hats off the beat, and syndrum fills.</li>
  <li style="--c: var(--magenta)"><b>The form.</b> Each song is a set of sections with their own chord changes. It loops for ever, repeating, skipping, dropping a layer, or moving the whole song up a key for a while. Sixteen drop-in effects are under your fingers.</li>
</ul>

<h2>Presets</h2>
<p>Nine presets, each lovingly influenced by ground-breaking electronic disco hits:</p>
<ul class="songs">
  <li><b>Feel</b><span>126 BPM · C</span></li>
  <li><b>Gio</b><span>113 BPM · A minor</span></li>
  <li><b>Mutant</b><span>123 BPM · F minor</span></li>
  <li><b>Heaven</b><span>130 BPM · A♭</span></li>
  <li><b>TickTock</b><span>126 BPM · C, then E♭</span></li>
  <li><b>Energy</b><span>123 BPM · C minor</span></li>
  <li><b>Fly</b><span>126 BPM · A minor</span></li>
  <li><b>Lover</b><span>130 BPM · C minor</span></li>
  <li><b>Express</b><span>128 BPM · D minor</span></li>
</ul>

<section class="credits">
<h2>Thanks</h2>
<p>Inspired by and modelled on <a href="https://charlesvestal.github.io/fm1-amb/">AMB-1</a> by Charles Vestal,
<a href="https://sbegueria.github.io/fm1-dub/">DUB-1</a> by Santiago Beguería and
<a href="https://fartypants.github.io/fm1-nachtfahrt/">NACHTFAHRT</a> by Cranky Man, and influenced by the music of
Giorgio Moroder, Patrick Cowley, Marc Cerrone, Space, Daft Punk and many others. You are all giants, and I stand on your
shoulders.</p>
<p>I'd like to give credit for sparking this idea to YouTube user @mateocarrion7776, who posted on a recent thread on
my channel those immortal words, "My name is Giovanni Giorgio…", and set off this train of thought.</p>
</section>
</main>"""

INSTALL_SOON = """<main style="max-width:46rem;margin:0 auto">
<h1 style="font:700 2.6rem/1 var(--display);letter-spacing:.05em;margin:2.4rem 0 .6rem">Install <span style="color:var(--gold);font-weight:500">alpha</span></h1>
<p>GIO-1 is in alpha. The installer will be here with the first firmware build, and test beta downloads are coming
soon. Until then, <a href="../emu/index.html">try it in the browser</a>: the same code the FM-1 will run.</p>
<p>When it's ready, installing works from Chrome or Edge with the FM-1 connected by a USB data cable, with nothing to
install on the computer, and the same page can put M-VAVE's own firmware back.</p>
</main>"""

QUICK_CSS = """
header.site, .foot { max-width: 62rem; }
main { max-width: 62rem; margin: 0 auto; }
h1 { font: 700 2.6rem/1 var(--display); letter-spacing: .05em; margin: 2rem 0 .3rem; }
.sub { color: var(--muted); margin: 0 0 1.4rem; }
.grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 28rem), 1fr)); gap: 14px; }
.card { background: var(--card); border-radius: 10px; padding: 14px 16px; min-width: 0; border-top: 4px solid var(--c, var(--magenta)); }
.card h2 { font: 700 1.1rem/1.2 var(--display); letter-spacing: .08em; text-transform: uppercase; margin: 0 0 .6rem; color: var(--c, var(--magenta)); }
.wrap { overflow-x: auto; }
table { border-collapse: collapse; width: 100%; font-size: .93rem; }
th, td { text-align: left; padding: 5px 10px 5px 0; border-bottom: 1px solid var(--rule); vertical-align: top; }
th { font: 500 .78rem/1.3 var(--display); letter-spacing: .08em; text-transform: uppercase; color: var(--muted); }
td:first-child { font-weight: 600; white-space: nowrap; }
.fx { display: grid; grid-template-columns: auto repeat(4, minmax(0, 1fr)); gap: 4px; font-size: .9rem; }
.fx span { background: var(--card-2); border-radius: 5px; padding: 4px 6px; min-width: 0; overflow-wrap: anywhere; }
.fx b { font: 700 .8rem/1.6 var(--display); letter-spacing: .06em; text-transform: uppercase; color: var(--muted); padding-right: 6px; }
.fx small { color: var(--muted); font-size: .72rem; margin-right: 4px; }
kbd { font: 600 .82em ui-monospace, Menlo, monospace; background: var(--card-2); border: 1px solid var(--rule); border-radius: 4px; padding: 0 .3em; }
ol.start { margin: 0; padding-left: 1.3rem; }
ol.start li { margin-bottom: .35rem; }
ol.start li::marker { color: var(--magenta); font-weight: 600; }
"""

QUICK_BODY = """<main>
<h1>Quickstart</h1>
<p class="sub">GIO-1 on one page: every control, page, effect and song.</p>
<div class="grid">

<section class="card" style="--c: var(--magenta)">
<h2>Start</h2>
<ol class="start">
  <li>Switch on: <b>Feel</b> plays from its intro.</li>
  <li><b>PRESETS</b>: another song, from the top.</li>
  <li><b>White keys</b>: play the lead, always in key. Three notes and a pause: it answers, and keeps your line as its theme.</li>
  <li><b>Black keys</b>: an effect while held. <b>FX</b>: each press the next instrument's echo and reverb (Bass, Pad, Lead, Drums, then Space), the white keys playing all sixteen effects meanwhile.</li>
  <li><b>SEL</b>: the next section. A tap on <b>REC</b>: stay in this one.</li>
  <li><b>EDIT</b>: the bassline (a white key picks a step, KNOB 2 its note); again: the section's chords; again: the drum grid (a white key a step on or off).</li>
  <li><b>ENV</b>, KNOB 2: open the bass filter.</li>
</ol>
</section>

<section class="card" style="--c: var(--cyan)">
<h2>Controls</h2>
<div class="wrap"><table>
<tr><td>White keys</td><td>The lead, a step of the scale each; the chord's notes lit</td></tr>
<tr><td>Black keys</td><td>Effects while held: Throw, Tape stop, Swell, Low-pass, High-pass, Sweep, Phaser, Freeze, Stutter, Drop, Roll</td></tr>
<tr><td>PLAY</td><td>Play / stop (the song from the top)</td></tr>
<tr><td>REC</td><td>Tap: hold the section. Held: the effects grid</td></tr>
<tr><td>EDIT</td><td>The edit pages: Bassline, Chords, Drum grid</td></tr>
<tr><td>SEL</td><td>The next section, on the next bar</td></tr>
<tr><td>FX</td><td>Each press pages through the echo and reverb settings of the Bass, Pad, Lead and Drums, then Space, then round again; on every one, the white keys play the 16-effect grid live</td></tr>
<tr><td>OCT− / OCT+</td><td>The bass's octave</td></tr>
<tr><td>ALGORITHM</td><td>The bass pattern: Song, Pump, Pump 16, Arpeggio, Gallop, Octaves, Slow</td></tr>
<tr><td>PRESETS</td><td>The song (each keeps its settings)</td></tr>
<tr><td>SELECT</td><td>Pages. With HOME held: tempo 110–130</td></tr>
<tr><td>SAVE</td><td>Save. Held 1.5 s: the song's recipe again</td></tr>
</table></div>
</section>

<section class="card" style="--c: var(--violet)">
<h2>Pages and knobs</h2>
<div class="wrap"><table>
<tr><th>Page</th><th>1</th><th>2</th><th>3</th><th>4</th></tr>
<tr><td>Song (HOME)</td><td>Tempo</td><td>Evolve</td><td>Motion</td><td>Arrange</td></tr>
<tr><td>Mixer (HOME ×2)</td><td>Bass</td><td>Pad</td><td>Lead</td><td>Drums</td></tr>
<tr><td>Bass (ENV)</td><td>Level</td><td>Cutoff</td><td>Resonance</td><td>Decay</td></tr>
<tr><td>Bass FM (ENV ×2)</td><td>Voice: Analog, FM</td><td>Cutoff / Ratio</td><td>Resonance / FM depth</td><td>Decay / FM decay</td></tr>
<tr><td>Lead (ARP)</td><td>Level</td><td>Voice</td><td>Busy</td><td>Glide</td></tr>
<tr><td>Lead 2 (ARP ×2)</td><td>Echo</td><td>Reverb</td><td>Octave</td><td>Tone</td></tr>
<tr><td>Pad (LFO)</td><td>Level</td><td>Colour</td><td>Bright</td><td>Phaser</td></tr>
<tr><td>Drums (SEQ)</td><td>Level</td><td>Fills</td><td>Hats</td><td>Swing</td></tr>
<tr><td>Bass FX (FX)</td><td>Echo</td><td>Reverb</td><td>Time*</td><td>Feedback*</td></tr>
<tr><td>Pad FX (FX ×2)</td><td>Echo</td><td>Reverb</td><td>Time*</td><td>Feedback*</td></tr>
<tr><td>Lead FX (FX ×3)</td><td>Echo</td><td>Reverb</td><td>Time*</td><td>Feedback*</td></tr>
<tr><td>Drums FX (FX ×4)</td><td>Echo</td><td>Reverb</td><td>Time*</td><td>Feedback*</td></tr>
<tr><td>Space (FX ×5)</td><td>Echo</td><td>Time*</td><td>Feedback*</td><td>Reverb</td></tr>
<tr><td>Setup (GLO)</td><td>Key</td><td>Tune</td><td>MIDI out</td><td>Lights</td></tr>
<tr><td>MIDI (GLO ×2)</td><td>MIDI out</td><td>Channels</td><td>Clock</td><td>Clock out</td></tr>
<tr><td>Mirror (GLO ×3)</td><td>Mirror on/off</td><td></td><td></td><td></td></tr>
<tr><td>Bassline (EDIT)</td><td>Step</td><td>Note</td><td>Accent</td><td>Length</td></tr>
<tr><td>Chords (EDIT again)</td><td>Chord</td><td>Root</td><td>Type</td><td>Bars</td></tr>
<tr><td>Drum grid (EDIT 3 times)</td><td>Drum</td><td>Step</td><td>Level</td><td></td></tr>
</table></div>
<p>* One echo for all: Time and Feedback are the same on every page. On the FX pages, Echo and Reverb are that
instrument's own.</p>
</section>

<section class="card" style="--c: var(--red)">
<h2>Effects</h2>
<p style="margin:0 0 .6rem;color:var(--muted);font-size:.9rem">FX or REC held: white keys 1–16, each while held.</p>
<div class="fx">
<b>Echo</b><span><small>1</small>Throw</span><span><small>2</small>Time x2</span><span><small>3</small>Tape stop</span><span><small>4</small>Swell</span>
<b>Filter</b><span><small>5</small>Low-pass</span><span><small>6</small>High-pass</span><span><small>7</small>Sweep</span><span><small>8</small>Phaser</span>
<b>Space</b><span><small>9</small>Reverb</span><span><small>10</small>Wash</span><span><small>11</small>Freeze</span><span><small>12</small>Stutter</span>
<b>Moves</b><span><small>13</small>Drop</span><span><small>14</small>Gate</span><span><small>15</small>Octave</span><span><small>16</small>Roll</span>
</div>
</section>

<section class="card" style="--c: var(--gold)">
<h2>Presets</h2>
<div class="wrap"><table>
<tr><th>Preset</th><th>BPM</th><th>Key</th><th>The form</th></tr>
<tr><td>1 Feel</td><td>126</td><td>C</td><td>Intro (bass, hats) · Verse (C) · Chorus (C–E♭–F–G) · Shift (on B♭) · Verse · Chorus · Break</td></tr>
<tr><td>2 Gio</td><td>113</td><td>A minor</td><td>Click (hats on the beat) · Arpeggio · Disco (Am–Em) · Strings (Am–F) · Fills · House</td></tr>
<tr><td>3 Mutant</td><td>123</td><td>F minor</td><td>Dawn (the pad alone) · Sequence · Groove · Theme (Fm–D♭) · Bridge (B♭m–Fm) · Theme</td></tr>
<tr><td>4 Heaven</td><td>130</td><td>A♭</td><td>Slow ×2 (half time, A♭–E♭–Fm11–E♭) · Lift · Fast (A♭–E♭–B♭–A♭) · Vamp (Fm11) · Fast</td></tr>
<tr><td>5 TickTock</td><td>126</td><td>C, then E♭</td><td>Intro · Verse (G–F) · Pre (Am–Dm–G–C) · Chorus (G–Em7–G) · Turn (C–E♭) · the same a minor third up · Outro</td></tr>
<tr><td>6 Energy</td><td>123</td><td>C minor</td><td>Intro · Groove (Cm) · Lift (A♭–B♭) · Groove (Cm–B♭) · Break · Build</td></tr>
<tr><td>7 Fly</td><td>126</td><td>A minor</td><td>Liftoff · Theme (Am–F–G) · Drift (Am–Dm) · Theme · Space</td></tr>
<tr><td>8 Lover</td><td>130</td><td>C minor</td><td>Intro · Verse (Cm–B♭) · Hook (A♭–B♭–Cm) · Robot · Hook</td></tr>
<tr><td>9 Express</td><td>128</td><td>D minor</td><td>Intro (the bass alone) · Drive · Strings · Theme (Dm–B♭–C) · Pursuit · Theme · Run</td></tr>
</table></div>
</section>

<section class="card" style="--c: var(--cyan)">
<h2>Handy to know</h2>
<div class="wrap"><table>
<tr><td>Help</td><td>Hold any button a second: a card says what it does</td></tr>
<tr><td>MIDI</td><td>Out on USB: bass, pad, lead, drums on 1, 2, 3, 10 (or 4, 5, 6, 11). Clock from Internal, USB or TRS</td></tr>
<tr><td>Update mode</td><td>REC held while switching on</td></tr>
<tr><td>Calibration</td><td>OCT− and OCT+ held while switching on</td></tr>
<tr><td>Emulator keys</td><td>White <kbd>Q</kbd>–<kbd>I</kbd> <kbd>A</kbd>–<kbd>K</kbd>, black <kbd>1</kbd>–<kbd>-</kbd>, <kbd>P</kbd> PLAY, <kbd>O</kbd> REC, <kbd>Esc</kbd> HOME, <kbd>Z</kbd> <kbd>X</kbd> OCT, <kbd>↑</kbd> <kbd>↓</kbd> SELECT</td></tr>
</table></div>
</section>

</div>
</main>"""

# the manual in the site's colours: fm1-base's renderer, its tokens given GIO-1's
MANUAL_CSS = """
:root:root { --paper: #100e22; --sheet: #1c1a3a; --rule: #3a3668; --ink: #f2eeff; --muted: #aaa4d4;
        --hot: #ff5cb4; --hot-t: #3a1d48; --cool: #ffc840; --cool-t: #2d2840; --hot-ink: #ff7cc4; color-scheme: dark; }
body { font-family: "Barlow", "Helvetica Neue", Helvetica, Arial, sans-serif; }
h1, h2, h3 { font-family: "Barlow Semi Condensed", "Arial Narrow", Arial, sans-serif; letter-spacing: .03em; }
img { image-rendering: pixelated; max-width: 100%; border-radius: 8px; }
"""


def main(out, pkg=None, version=None):
    out = Path(out)
    conf = read_conf(SRC / "app" / "fm1.toml")
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "manual", "quickstart"):
        (out / d).mkdir(parents=True)
    product = None
    # the installer, when there is a package; else a page saying it is coming
    if pkg:
        pkg = Path(pkg)
        raw = pkg.read_bytes()
        product = product_of(raw)
        test = os.environ.get("GIO_TEST_BUILD") == "1"   # a test build's own Install page (never the site's): a dev build
        if not re.fullmatch(r"FM-1_58\d{6}", product) and not (test and product == "FM-1_5800"):
            raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
        if b"FELUCCA-LOADER-1" not in raw:
            raise SystemExit(f"{pkg}: no update loader in it")
        name = f"gio-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
        (out / "firmware").mkdir()
        shutil.copy(pkg, out / "firmware" / name)
        page_i = (BASE / "web" / "installer.html").read_text(encoding="utf-8")
        lib = strip_module((BASE / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
            strip_module((BASE / "web" / "fm1ota.js").read_text(encoding="utf-8"))
        boot = BASE / "web" / "m-bootloader" / f"fm1-uboot-{MBOOT_VERSION}.fwsc"
        boot_name = f"m-bootloader-{MBOOT_VERSION}.fwsc"
        shutil.copy(boot, out / "firmware" / boot_name)
        shutil.copy(BASE / "web" / "m-bootloader" / "LICENSE", out / "firmware" / "m-bootloader-LICENSE.txt")
        meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name,
                           "backup": bool(conf.get("backup")),
                           "boot": {"pkg": "../firmware/" + boot_name, "version": MBOOT_VERSION,
                                    "sha256": hashlib.sha256(boot.read_bytes()).hexdigest()}})
        for mark in ("/*LIB*/", "/*META*/", "/*APP*/"):
            if page_i.count(mark) != 1:
                raise SystemExit(f"base/web/installer.html must contain {mark} once")
        for key, val in (("NAME", "GIO-1"), ("REPO", REPO), ("REPO_SHORT", re.sub(r"^https?://", "", REPO)),
                         ("LICENSE", conf.get("licence", "MPL-2.0")), ("CREDITS", " " + conf.get("credits", ""))):
            page_i = page_i.replace("{{" + key + "}}", val)
        # the release notes' warnings, over the Install button too
        caution = ('<div style="border:2px solid #b5451b;border-radius:10px;padding:.7rem 1rem;margin:1rem 0">'
                   '<p style="margin:.2rem 0"><strong>Disclaimer: installing this firmware could brick your FM-1.</strong> '
                   'GIO-1 is unofficial firmware, not made, endorsed or supported by M-VAVE, and this is an alpha. You install '
                   'it entirely at your own risk.</p>'
                   '<p style="margin:.4rem 0"><strong>Tested on one FM-1 only.</strong> It works there, but it has not yet been '
                   'tried on others.</p>'
                   '<p style="margin:.4rem 0 .2rem"><strong>The safety nets:</strong> a crash restarts it; two failed starts in '
                   'a row bring up SAFE MODE (no sound, USB on: install again, or go back to stock below); past that, the '
                   'chip\'s own update mode, from which the rescue tool puts M-VAVE\'s firmware back. Update mode by hand: hold '
                   'REC while switching the FM-1 on, until the screen says UPDATE MODE.</p></div>')
        if page_i.count("<h1>Install GIO-1</h1>") != 1:
            raise SystemExit("base/web/installer.html: no <h1>Install {{NAME}}</h1> for the warnings")
        page_i = page_i.replace("<h1>Install GIO-1</h1>", "<h1>Install GIO-1</h1>\n  " + caution)
        app_js = (SRC / "app" / conf["install_app"]).read_text(encoding="utf-8") if conf.get("install_app") else "null"
        (out / "install" / "index.html").write_text(page_i.replace("/*LIB*/", lib).replace("/*META*/", meta)
                                                    .replace("/*APP*/", app_js), encoding="utf-8")
        install = (f'<a href="install/index.html"><strong>Install <em>alpha {html.escape(version)}</em></strong><span>From Chrome '
                   f'or Edge, with the FM-1 connected by USB. Nothing to install on the computer.</span></a>')
        download = (f'<a href="firmware/{name}"><strong>Download <em>{name}</em></strong><span>For the command-line '
                    f'installer: <code>python3 base/tools/fm1_install.py {name}</code>. In alpha; test beta downloads '
                    f'coming soon.</span></a>')
    else:
        (out / "install" / "index.html").write_text(
            page("GIO-1 Install", "Installing GIO-1 on the M-VAVE FM-1: coming with the first build.", INSTALL_SOON,
                 "install/", 1), encoding="utf-8")
        install = ('<a href="install/index.html"><strong>Install <em>alpha</em></strong><span>From Chrome or Edge, with the FM-1 '
                   'connected by USB. Coming with the first firmware build.</span></a>')
        download = ('<div class="off"><strong>Download <em>alpha</em></strong><span>In alpha; test beta downloads coming '
                    'soon.</span></div>')
    # the landing page and its screenshots
    shutil.copytree(SRC / "docs" / "img", out / "img")
    (out / "index.html").write_text(
        page("GIO-1", "A generative electronic disco firmware for the M-VAVE FM-1.",
             LANDING_BODY.replace("LOGO_SVG", logo_svg()).replace("__INSTALL__", install).replace("__DOWNLOAD__", download).replace("__REPO__", REPO),
             "", 0, LANDING_CSS), encoding="utf-8")
    # the quickstart
    (out / "quickstart" / "index.html").write_text(
        page("GIO-1 Quickstart", "GIO-1 on one page: every control, page, effect and song.", QUICK_BODY,
             "quickstart/", 1, QUICK_CSS), encoding="utf-8")
    # the manual: fm1-base's renderer, then the site's colours, header and links
    man = out / "manual" / "index.html"
    subprocess.run([sys.executable, str(BASE / "tools" / "manual_html.py"), str(SRC / "docs" / "MANUAL.md"), str(man),
                    "--name", "GIO-1", "--repo", REPO, "--tagline", conf.get("tagline", "")],
                   check=True, capture_output=True)
    m = man.read_text(encoding="utf-8")
    m = re.sub(r'<header class="site">.*?</header>', lambda _: header("manual/", 1), m, count=1, flags=re.S)
    m = m.replace("</head>", FONTS + "<style>" + TOKENS + HEADER_CSS + MANUAL_CSS + "</style>\n</head>", 1)
    m = m.replace('href="../BUILDING.md"', f'href="{REPO}/blob/main/BUILDING.md"')
    m = m.replace('href="../LICENSING.md"', f'href="{REPO}/blob/main/LICENSING.md"')
    man.write_text(m, encoding="utf-8")
    shutil.copytree(SRC / "docs" / "img", out / "manual" / "img")
    # the emulator: fm1-base's page, dark, with GIO-1's guide under it and the site's header
    emu = SRC / "app" / "build" / "emu"
    if not (emu / "app.wasm").exists():
        raise SystemExit("no app/build/emu/app.wasm: run ./build.sh --web (needs Emscripten, or tools/emcc_wasi.py)")
    shutil.copytree(emu, out / "emu", ignore=shutil.ignore_patterns("o2"))
    e = (out / "emu" / "index.html").read_text(encoding="utf-8")
    guide = (SRC / "tools" / "emu_guide.html").read_text(encoding="utf-8")
    mark = "<!-- what the screen says, for the computer's screen reader -->"
    e = e.replace('<html lang="en">', '<html lang="en" data-theme="dark">', 1)
    e = e.replace("<title>GIO in the browser</title>", "<title>GIO-1 Emulator</title>").replace("GIO uses ", "GIO-1 uses ")
    e = re.sub(r"<header>\s*<h1>GIO</h1>\s*</header>", lambda _: header("emu/", 1), e, count=1)
    e = e.replace(mark, guide + mark, 1)
    e = e.replace("</head>", FONTS + "<style>" + TOKENS + HEADER_CSS.replace("max-width: 46rem", "max-width: 62rem") +
                  ':root[data-theme="dark"] { --ground: #100e22; --card: #1c1a3a; --rule: #3a3668; --ink: #f2eeff;'
                  ' --muted: #aaa4d4; --accent: #ff5cb4; --teal: #2fd8f0; color-scheme: dark; }\n'
                  'header.site { margin-bottom: 8px; }</style>\n</head>', 1)
    (out / "emu" / "index.html").write_text(e, encoding="utf-8")
    # the screen mirror (fm1.toml mirror = true): fm1-base's newer page (tools/fm1base_web: it talks only to the FM-1),
    # not connecting by itself and sending nothing as it closes; dark, with the site's header
    for rel, mh in mirror_pages(conf).items():
        mh = mh.replace('<html lang="en">', '<html lang="en" data-theme="dark">', 1)
        mh = mh.replace("<title>Mirror · GIO-1</title>", "<title>GIO-1 Mirror</title>", 1)
        if mh.count("<body>") != 1:
            raise SystemExit("base/web/mirror/index.html: <body> not found once")
        mh = mh.replace("<body>", "<body>\n" + header("mirror/", 1), 1)
        mh = mh.replace("</head>", FONTS + "<style>" + TOKENS + HEADER_CSS +
                        ':root[data-theme="dark"] { --ground: #100e22; --card: #1c1a3a; --rule: #3a3668; --ink: #f2eeff;'
                        ' --muted: #aaa4d4; --accent: #ff5cb4; --teal: #2fd8f0; color-scheme: dark; }\n'
                        'header.site { margin-bottom: 8px; }</style>\n</head>', 1)
        (out / "mirror").mkdir(parents=True, exist_ok=True)
        (out / rel).write_text(mh, encoding="utf-8")
    # the screen reader's words files (fm1-reader): every one ever made, for the devices that point at them
    shutil.copytree(SRC / "app" / "web" / "words", out / "words")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, quickstart/, manual/, emu/, mirror/, words/, install/ ({product or 'coming with the first build'})")


if __name__ == "__main__":
    if len(sys.argv) not in (2, 4):
        sys.exit(__doc__)
    main(*sys.argv[1:])
