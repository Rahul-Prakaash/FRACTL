#!/bin/sh
set -u
cd "$(dirname "$0")/.." || exit 1
mkdir -p build/pipeline
T=build/pipeline
fail=0
ran=0

ok()   { ran=$((ran + 1)); echo "ok   $1"; }
bad()  { ran=$((ran + 1)); fail=$((fail + 1)); echo "FAIL $1"; }

fr() {
    printf 'fractal T {\n    axiom: "%s";\n    rules { %s }\n    angle: %s;\n    iterations: %s;\n    step: 10;\n    heading: 0;\n}\n' \
        "$2" "$3" "$4" "$5" > "$T/$1.fr"
}

build() {
    ./fractlc "$T/$1.fr" -o "$T/$1.o" --svg "$T/$1.svg" --emit-ir "$T/$1.ll" > "$T/$1.log" 2>&1 &&
    cc "$T/$1.o" runtime.o -lm -o "$T/$1" &&
    "./$T/$1" >> "$T/$1.log" 2>&1
}

polylines() { grep -c '<polyline' "$T/$1.svg"; }

echo "pipeline"

fr branch "F[+F]F" "" 90 0
if build branch && [ "$(polylines branch)" = 2 ]; then ok "branch: '[' ']' splits strokes"; else bad "branch"; fi
if grep -q 'call void @turtle_push()' "$T/branch.ll" && grep -q 'call void @turtle_pop()' "$T/branch.ll"
then ok "branch: IR calls turtle_push/turtle_pop"; else bad "branch IR"; fi

fr move "FfF" "" 90 0
if build move && [ "$(polylines move)" = 2 ] && grep -q 'call void @turtle_move(double 1.000000e+01)' "$T/move.ll"
then ok "move: 'f' lifts the pen"; else bad "move"; fi

fr structural "FX" "X -> \"X+YF+\"; Y -> \"-FX-Y\";" 90 6
if build structural && [ "$(polylines structural)" = 1 ] && grep -q '64 segments' "$T/structural.log"
then ok "structural: X/Y draw nothing, 2^6 segments"; else bad "structural"; fi

fr unbal_close "F]" "" 90 0
if ./fractlc "$T/unbal_close.fr" -o "$T/x.o" --svg "$T/x.svg" > "$T/unbal_close.log" 2>&1; then
    bad "unbalanced ']' must be a compile-time error"
elif grep -q "line 2: unbalanced brackets in the axiom: ']' at position 2" "$T/unbal_close.log"; then
    ok "compile-time error: ']' without '['"
else bad "unbalanced ']' message: $(cat "$T/unbal_close.log")"; fi

fr unbal_open "X" "X -> \"F[+X\";" 90 2
if ./fractlc "$T/unbal_open.fr" -o "$T/x.o" --svg "$T/x.svg" > "$T/unbal_open.log" 2>&1; then
    bad "unbalanced '[' must be a compile-time error"
elif grep -q "line 3: unbalanced brackets in the rule for 'X': '\[' at position 2 is never closed" "$T/unbal_open.log"; then
    ok "compile-time error: '[' never closed"
else bad "unbalanced '[' message: $(cat "$T/unbal_open.log")"; fi

for n in triangle koch; do
    if [ ! -f "tests/phase1/$n.svg" ]; then bad "phase1 reference tests/phase1/$n.svg missing"; continue; fi
    make -s "build/$n.svg" > /dev/null 2>&1
    if cmp -s "build/$n.svg" "tests/phase1/$n.svg"; then ok "phase 1 regression: $n.svg byte-identical to Phase 1"
    else bad "phase 1 regression: $n.svg differs from Phase 1"; fi
done

echo "$ran pipeline checks, $fail failed"
[ "$fail" = 0 ]
