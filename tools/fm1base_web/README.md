# fm1-base's web pages, as GIO-1's site uses them

Copied unchanged from [fm1-base](https://github.com/charlesvestal/fm1-base) at `c8e0599`, the commit `base/` links
(MPL-2.0, Charles Vestal; `fm1ports.js` also Chris Farrell's, see its header). They were first copied (at `2d8ed74`)
because the fm1-base pinned then (`d8ff8f4`) had the older Mirror page; they are kept here so the site's three changes
below are made to a known copy. Copy them again when `base/` moves.

- `mirror.html`: fm1-base `web/mirror/index.html`. It only talks to the FM-1: a port named after the firmware or with
  an FM-1's usual name, the one the user picked, or one that answered the screen reader's HELLO. From `c8e0599` it
  reads mirror version 4's input report (the dim lights, as the browser emulator shows them) and still reads older ones.
- `fm1ports.js`: fm1-base `web/fm1ports.js`, inlined into the page for its `{{PORTS_JS}}`.
- `fm1panel.js`: fm1-base `web/fm1panel.js` (the panel drawn once, shared with the emulator), for its `{{PANEL_JS}}`.

`tools/make_pages.py` makes the site's Mirror page from them and changes three things (each checked to apply once):

1. It does not connect by itself when it opens, even with MIDI already allowed: it waits for **Connect**.
2. Nothing is sent as the page closes (the request to stop, and the remote's let-go): sending MIDI while Chrome
   tears a page down is the likeliest cause of a crash seen leaving the older page with an FM-1 plugged in. GIO-1
   stops mirroring by itself a few seconds after the requests stop (fm1-base `MR_TIMEOUT_MS`), and lets go of the
   computer's buttons and keys then.
3. A knob turns by dragging it (up or right: clockwise, a detent every 8 pixels), not only with the mouse wheel,
   which trackpads and touch screens often don't give over a knob; the help says so.

When GIO-1's fm1-base is updated past `2d8ed74`, use its own pages again (`make_site.tool_pages`) with the same
changes, and remove this folder.
