#!/bin/sh
set -u
cd "$(dirname "$0")/.." || exit 1

mode=check
[ "${1:-}" = "--update" ] && mode=update
fail=0

echo "golden"
for fr in *.fr; do
    n=${fr%.fr}
    if grep -q 'dimensions: *3' "$fr"; then out=f3d; else out=svg; fi
    if ! make -s "build/$n.$out" > /dev/null; then
        echo "FAIL $n: build/run failed"; fail=1; continue
    fi
    for ext in $out ll; do
        got="build/$n.$ext"
        gold="tests/golden/$n.$ext"
        if [ "$mode" = update ]; then
            mkdir -p tests/golden && cp "$got" "$gold" && echo "updated $gold"
        elif [ ! -f "$gold" ]; then
            echo "FAIL $n.$ext: no golden (run: make golden)"; fail=1
        elif cmp -s "$got" "$gold"; then
            echo "ok   $n.$ext"
        else
            echo "FAIL $n.$ext differs from golden:"; diff "$gold" "$got" | head -20; fail=1
        fi
    done
done
exit $fail
