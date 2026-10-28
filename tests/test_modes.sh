#!/bin/sh
set -u
cd "$(dirname "$0")/.." || exit 1
T=${TMPDIR:-/tmp}/fractl-modes-$$
mkdir -p "$T"
fail=0
ran=0

check() {
    ran=$((ran + 1))
    if cmp -s "$1" "$2"; then echo "ok   $3"; else echo "FAIL $3"; fail=$((fail + 1)); fi
}

aot() {
    ./fractlc "$1" -o "$T/p.o" --out "$2" $3 > /dev/null 2>"$T/err" || { cat "$T/err"; return 1; }
    cc "$T/p.o" runtime.o -lm -o "$T/p" && "$T/p" > /dev/null
}
jit() {
    ./fractlc "$1" --jit --out "$2" $3 > /dev/null 2>"$T/err" || { cat "$T/err"; return 1; }
}

echo "modes"
for fr in *.fr studio/presets/*.fr; do
    n=$(basename "$fr" .fr)
    if grep -q 'dimensions: *3' "$fr"; then ext=f3d; else ext=svg; fi
    aot "$fr" "$T/$n.rec.$ext" ""           || { echo "FAIL $n: recursive AOT build"; fail=$((fail + 1)); continue; }
    aot "$fr" "$T/$n.unr.$ext" "--unroll"   || { echo "FAIL $n: unrolled AOT build"; fail=$((fail + 1)); continue; }
    jit "$fr" "$T/$n.jit.$ext" ""           || { echo "FAIL $n: JIT build"; fail=$((fail + 1)); continue; }
    jit "$fr" "$T/$n.opt.$ext" "--opt"      || { echo "FAIL $n: --opt build"; fail=$((fail + 1)); continue; }
    check "$T/$n.rec.$ext" "$T/$n.unr.$ext" "$n: recursive == unrolled"
    check "$T/$n.rec.$ext" "$T/$n.jit.$ext" "$n: object+link == JIT"
    check "$T/$n.rec.$ext" "$T/$n.opt.$ext" "$n: --opt leaves the drawing unchanged"
    if [ -f "tests/phase2/$n.svg" ]; then
        check "$T/$n.rec.svg" "tests/phase2/$n.svg" "$n: identical to Stage 2"
    fi
done

ir_lines() { ./fractlc "$1" --jit --out "$T/x" --emit-ir "$T/x.ll" $2 > /dev/null && grep -c . "$T/x.ll"; }
rec=$(ir_lines meadow.fr "")
unr=$(ir_lines meadow.fr "--unroll")
ran=$((ran + 1))
if [ "$rec" -lt 1000 ] && [ "$unr" -gt $((rec * 50)) ]; then
    echo "ok   meadow: recursive IR $rec lines vs unrolled $unr lines"
else
    echo "FAIL meadow IR size: recursive $rec, unrolled $unr"; fail=$((fail + 1))
fi
folds=$(./fractlc koch.fr --jit --out "$T/k.svg" --opt | grep -o 'folded [0-9]*' | grep -o '[0-9]*')
ran=$((ran + 1))
if [ "${folds:-0}" -gt 0 ]; then echo "ok   koch: --opt folded $folds rotations"; else echo "FAIL koch: --opt folded nothing"; fail=$((fail + 1)); fi

rm -rf "$T"
echo "$ran mode checks, $fail failed"
[ "$fail" = 0 ]
