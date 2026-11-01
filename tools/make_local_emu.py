#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ian Guinan
"""MOR-1's browser emulator as one file you can double-click: the page, its sound worklet and the WebAssembly
build of MOR-1 all inside one HTML file, so it runs from the computer's own disk (file://), where a browser will not
fetch the separate files. Needs a web build first (./build.sh --web: app/build/emu/).

  python3 tools/make_local_emu.py [EMU_DIR] [OUT.html]     (defaults: app/build/emu, app/build/MOR-1-emulator.html)
"""
import base64, sys
from pathlib import Path

emu = Path(sys.argv[1] if len(sys.argv) > 1 else "app/build/emu")
out = Path(sys.argv[2] if len(sys.argv) > 2 else "app/build/MOR-1-emulator.html")
page = (emu / "index.html").read_text()
wasm = base64.b64encode((emu / "app.wasm").read_bytes()).decode()
worklet = (emu / "worklet.js").read_text()

fetch_wasm = 'fetch("app.wasm").then((r) => { if (!r.ok) throw new Error(`app.wasm: ${r.status}`); return r.arrayBuffer(); })'
add_app = 'ac.audioWorklet.addModule("app_worklet.js").catch(() => {})'
add_main = 'ac.audioWorklet.addModule("worklet.js")'
for s in (fetch_wasm, add_app, add_main):
    if page.count(s) != 1:
        raise SystemExit(f"make_local_emu: the page has changed; not found once: {s[:40]}...")
page = page.replace(fetch_wasm, "Promise.resolve(LOCAL_WASM())")
page = page.replace(add_app, "Promise.resolve()")
page = page.replace(add_main, "ac.audioWorklet.addModule(LOCAL_WORKLET)")

embed = ("<script>\n/* the files a page from disk cannot fetch, inside it (tools/make_local_emu.py) */\n"
         f"const LOCAL_WASM_B64 = \"{wasm}\";\n"
         "function LOCAL_WASM() { const s = atob(LOCAL_WASM_B64), b = new Uint8Array(s.length);"
         " for (let i = 0; i < s.length; i++) b[i] = s.charCodeAt(i); return b.buffer; }\n"
         f"const LOCAL_WORKLET = \"data:text/javascript;base64,{base64.b64encode(worklet.encode()).decode()}\";\n"
         "</script>\n")
i = page.find("<script")
if i < 0:
    raise SystemExit("make_local_emu: no script in the page")
page = page[:i] + embed + page[i:]
page = page.replace("<title>MOR in the browser</title>", "<title>MOR-1 Emulator</title>").replace("<h1>MOR</h1>", "<h1>MOR-1</h1>")
page = page.replace("MOR uses ", "MOR-1 uses ")
# the guide (tools/emu_guide.html) under the Screen Reader section
guide = (Path(__file__).parent / "emu_guide.html").read_text()
mark = "<!-- what the screen says, for the computer's screen reader -->"
if page.count(mark) == 1:
    page = page.replace(mark, guide + mark)
out.write_text(page)
print(f"make_local_emu: {out} ({out.stat().st_size // 1024} KB)")
