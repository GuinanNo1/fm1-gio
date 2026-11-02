#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ian Guinan
"""GIO-1's browser emulator as one file you can double-click: the page, its sound worklet and the WebAssembly
build of GIO-1 all inside one HTML file, so it runs from the computer's own disk (file://), where a browser will not
fetch the separate files. Needs a web build first (./build.sh --web: app/build/emu/).

  python3 tools/make_local_emu.py [EMU_DIR] [OUT.html]     (defaults: app/build/emu, app/build/GIO-1-emulator.html)
"""
import base64, json, sys
from pathlib import Path

emu = Path(sys.argv[1] if len(sys.argv) > 1 else "app/build/emu")
out = Path(sys.argv[2] if len(sys.argv) > 2 else "app/build/GIO-1-emulator.html")
page = (emu / "index.html").read_text()
wasm = base64.b64encode((emu / "app.wasm").read_bytes()).decode()
worklet = (emu / "worklet.js").read_text()

fetch_wasm = 'fetch("app.wasm").then((r) => { if (!r.ok) throw new Error(`app.wasm: ${r.status}`); return r.arrayBuffer(); })'
add_app = 'ac.audioWorklet.addModule("app_worklet.js").catch(() => {})'
add_main = 'ac.audioWorklet.addModule("worklet.js")'
for s in (fetch_wasm, add_app, add_main):
    if page.count(s) != 1:
        raise SystemExit(f"make_local_emu: the page has changed; not found once: {s[:40]}...")
node_new = 'node = new AudioWorkletNode(ac, "fm1", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });'
if page.count(node_new) != 1:
    raise SystemExit("make_local_emu: the page has changed; the AudioWorkletNode not found once")
page = page.replace(fetch_wasm, "Promise.resolve(LOCAL_WASM())")
page = page.replace(add_app, "Promise.resolve()")
page = page.replace(add_main, "LOCAL_LOAD(ac)")
page = page.replace(node_new, "node = LOCAL_NODE(ac);")

# The sound: an AudioWorklet when the browser will load one from a page on disk (a data: URL, else a blob: URL);
# else the very same worklet code run on the page, driven by a ScriptProcessorNode, which every browser allows.
embed = ("<script>\n/* the files a page from disk cannot fetch, inside it (tools/make_local_emu.py) */\n"
         f"const LOCAL_WASM_B64 = \"{wasm}\";\n"
         f"const LOCAL_WORKLET_SRC = {json.dumps(worklet).replace('</', '<\\/')};\n"
         r"""function LOCAL_WASM() { const s = atob(LOCAL_WASM_B64), b = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) b[i] = s.charCodeAt(i); return b.buffer; }
let LOCAL_WORKLET_OK = false;
async function LOCAL_LOAD(ac) {
  if (location.hash === "#fallback" || !ac.audioWorklet) return;
  const urls = ["data:text/javascript;base64," + btoa(unescape(encodeURIComponent(LOCAL_WORKLET_SRC)))];
  try { urls.push(URL.createObjectURL(new Blob([LOCAL_WORKLET_SRC], { type: "text/javascript" }))); } catch (e) {}
  for (const u of urls) {
    try { await ac.audioWorklet.addModule(u); LOCAL_WORKLET_OK = true; return; } catch (e) { /* the next way */ }
  }
}
function LOCAL_NODE(ac) {
  if (LOCAL_WORKLET_OK) {
    try { return new AudioWorkletNode(ac, "fm1", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] }); }
    catch (e) { /* on to the page's own */ }
  }
  const ch = new MessageChannel();
  let Proc = null;
  class AWP { constructor() { this.port = ch.port2; } }
  const setTime = new Function("AudioWorkletProcessor", "registerProcessor",
    "var currentTime = 0;\n" + LOCAL_WORKLET_SRC + "\n;return (t) => { currentTime = t; };")(AWP, (n, c) => { Proc = c; });
  const proc = new Proc();
  const Q = 128, BUF = 2048, L = new Float32Array(Q), R = new Float32Array(Q), outs = [[L, R]];
  const sp = ac.createScriptProcessor(BUF, 1, 2);
  sp.onaudioprocess = (e) => {
    const ol = e.outputBuffer.getChannelData(0), or = e.outputBuffer.getChannelData(1);
    for (let i = 0; i < BUF; i += Q) {
      setTime(e.playbackTime + i / ac.sampleRate);
      proc.process([], outs);
      ol.set(L, i);
      or.set(R, i);
    }
  };
  const silent = ac.createConstantSource ? ac.createConstantSource() : null;   // something into its input, for Safari
  if (silent) { silent.offset.value = 0; silent.connect(sp); silent.start(); }
  return { port: ch.port1, connect: (d) => sp.connect(d), disconnect: () => { try { sp.disconnect(); if (silent) silent.stop(); } catch (e) {} } };
}
</script>
""")
i = page.find("<script")
if i < 0:
    raise SystemExit("make_local_emu: no script in the page")
page = page[:i] + embed + page[i:]
page = page.replace("<title>GIO in the browser</title>", "<title>GIO-1 Emulator</title>").replace("<h1>GIO</h1>", "<h1>GIO-1</h1>")
page = page.replace("GIO uses ", "GIO-1 uses ")
# the guide (tools/emu_guide.html) under the Screen Reader section
guide = (Path(__file__).parent / "emu_guide.html").read_text()
mark = "<!-- what the screen says, for the computer's screen reader -->"
if page.count(mark) == 1:
    page = page.replace(mark, guide + mark)
out.write_text(page)
print(f"make_local_emu: {out} ({out.stat().st_size // 1024} KB)")
