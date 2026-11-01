#!/usr/bin/env python3
# A stand-in for Emscripten's emcc when Emscripten cannot be installed: put it on PATH as "emcc" with clang 18+
# (wasm32 target, wasm-ld) and wasi-sdk 24's sysroot and builtins unpacked in ~/wasi (github.com/WebAssembly/wasi-sdk
# releases); copy libclang_rt.builtins-wasm32.a into clang's lib/wasi folder. Then ./build.sh --web.
# A stand-in for Emscripten's emcc, for fm1-base's --web build: clang 18 for wasm32-wasi with wasi-sdk 24's sysroot
import sys, subprocess, os
W = os.path.expanduser("~/wasi")
args, out, link = [], [], True
a = sys.argv[1:]
if "--version" in a:
    print("emcc (clang wasm32-wasi stand-in) 0.0.0"); sys.exit(0)
if "-c" in a:
    link = False
for x in a:
    if x == "--no-entry": out.append("-mexec-model=reactor"); continue
    if x.startswith("-sSTACK_SIZE="): out.append("-Wl,-z,stack-size=" + x.split("=")[1]); continue
    if x.startswith("-sINITIAL_MEMORY="): out.append("-Wl,--initial-memory=" + x.split("=")[1]); continue
    if x.startswith("-s"): continue
    out.append(x)
cmd = ["clang", "--target=wasm32-wasi", "--sysroot=" + W + "/wasi-sysroot-24.0", "-fno-exceptions"] + out
if link:
    cmd += ["-Wl,--export-dynamic", "-Wl,--no-entry", W + "/libclang_rt.builtins-wasm32-wasi-24.0/libclang_rt.builtins-wasm32.a"]
sys.exit(subprocess.call(cmd))
