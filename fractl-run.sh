#!/bin/sh
set -e

if [ -z "$1" ]; then
    echo "usage: ./fractl-run.sh <grammar.fr>"
    exit 2
fi
if [ ! -f "$1" ]; then
    echo "fractl-run: no such file: $1"
    exit 1
fi
if [ ! -x ./fractlc ]; then
    echo "fractl-run: fractlc not built yet — run 'make' first"
    exit 1
fi

name=$(basename "$1" .fr)
mkdir -p build
if grep -q 'dimensions: *3' "$1"; then out="build/$name.f3d"; else out="build/$name.svg"; fi

echo "== compiling $1 =="
./fractlc "$1" -o "build/$name.o" --out "$out" --emit-ir "build/$name.ll"

echo "== linking =="
cc "build/$name.o" runtime.o -lm -o "build/$name"

echo "== running =="
"./build/$name"

case "$out" in
    *.f3d)
        echo "== $out is a 3D segment stream: open it in FRACTL Studio (Launch FRACTL Studio.bat) =="
        exit 0 ;;
esac

echo "== opening $out =="
if command -v explorer.exe >/dev/null 2>&1; then
    explorer.exe "$(wslpath -w "$out")" || true
elif command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$out" || true
else
    echo "(open $out yourself — no opener found on this system)"
fi
