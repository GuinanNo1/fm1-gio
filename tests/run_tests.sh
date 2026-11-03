#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal, Ian Guinan
# GIO-1's tests: the instrument (tests/host/gio_test.c), the start-up chime (tests/host/chime_test.c), the screen mirror (tests/host/mirror_check.py), the whole app in fm1-base's simulator (tests/scenarios/*.gio:
# UI, the form, the panel, persistence, the screen reader), the browser build, and the update path against GIO-1's
# package (./build.sh first). Screenshots and audio in app/build/scenarios/. fm1-base's own tests: base/tests/run_tests.sh.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
PY="${PYTHON:-python3}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"   # the package's SDK files, as ./build.sh
B=app/build
OUT=$B/tests
FAIL=0
mkdir -p "$OUT" $B/scenarios
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run gio sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Iapp/dsp -Ibase/include \
    -o $OUT/gio_test tests/host/gio_test.c app/dsp/gio.c -lm && $OUT/gio_test"
run chime sh -c "$CC -O2 -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -o $OUT/chime_test tests/host/chime_test.c && \
    $OUT/chime_test && $CC -O2 -std=c99 -Wall -Wextra -Werror -DGIO_CHIME_SR=48000 -o $OUT/chime_test48 \
    tests/host/chime_test.c && $OUT/chime_test48"
if [ -f $B/gio.fwsc ]; then
    run ota-entry sh -c "$CC -w -o $OUT/ota_test base/tests/ota_test.c && $OUT/ota_test $B/gio.fwsc"
    run update-loader sh -c "head -c 100000 $B/gio.bin > $OUT/old_app.bin && \
        $PY base/tools/fm1pkg_make.py $OUT/old_app.bin $B/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -w -o $OUT/ldr_test base/tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc $B/gio.fwsc"
else
    echo "  skip update path (no $B/gio.fwsc: ./build.sh)"
fi
run host-build sh build.sh --host
if command -v emcc >/dev/null 2>&1; then
    run emu sh -c "sh build.sh --web && node tests/host/emu_test.mjs $B/emu/app.wasm"
else
    echo "  skip emu (no emcc)"
fi
for s in tests/scenarios/*.gio; do
    n=$(basename "$s" .gio)
    mkdir -p "$B/scenarios/$n"
    run "scenario-$n" $B/host/gio_host "$s" "$B/scenarios/$n"
done
# the screen mirror (scenario mirror.gio's sessions): the viewer's picture, decoded by fm1-base's own tool, is the screen
M=$B/scenarios/mirror
run mirror-fast "$PY" tests/host/mirror_check.py $M/stream.syx $M/fb.raw 50
run mirror-recovers "$PY" tests/host/mirror_check.py $M/slow.syx $M/fb_slow.raw 10
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
