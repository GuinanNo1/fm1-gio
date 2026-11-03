# fm1-base's web pages, newer than GIO-1's pinned fm1-base

Copied unchanged from [fm1-base](https://github.com/charlesvestal/fm1-base) at `2d8ed74` (MPL-2.0, Charles Vestal;
`fm1ports.js` also Chris Farrell's, see its header), because the pinned fm1-base (`d8ff8f4`) has the older Mirror page.
The firmware side of the mirror (`device/mirror.c`) is the same in both, so the newer page speaks to GIO-1 as it is.

- `mirror.html`: fm1-base `web/mirror/index.html`. The newer page only talks to the FM-1: a port named after the
  firmware or with an FM-1's usual name, the one the user picked, or one that answered the screen reader's HELLO. The
  older one fell back to the first MIDI port it found and sent it the mirror's requests.
- `fm1ports.js`: fm1-base `web/fm1ports.js`, inlined into the page for its `{{PORTS_JS}}`.

`tools/make_pages.py` makes the site's Mirror page from them and changes two things (each checked to apply once):

1. It does not connect by itself when it opens, even with MIDI already allowed: it waits for **Connect**.
2. Nothing is sent as the page closes (the request to stop, and the remote's let-go): sending MIDI while Chrome
   tears a page down is the likeliest cause of a crash seen leaving the older page with an FM-1 plugged in. GIO-1
   stops mirroring by itself a few seconds after the requests stop (fm1-base `MR_TIMEOUT_MS`), and lets go of the
   computer's buttons and keys then.

When GIO-1's fm1-base is updated past `2d8ed74`, use its own pages again (`make_site.tool_pages`) with the same two
changes, and remove this folder.
