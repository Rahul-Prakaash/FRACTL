#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
OUT=dist/FRACTL-Studio
LLVM_CONFIG=${LLVM_CONFIG:-$(command -v llvm-config-20 || command -v llvm-config)}

make -s fractlc runtime.o
rm -rf "$OUT"
mkdir -p "$OUT/studio/presets" "$OUT/tests"

cc -O2 -c tools/zstd_stub.c -o build/zstd_stub.o
g++ build/fractlc.o build/codegen.o build/emit_object.o build/jit.o runtime.o build/zstd_stub.o \
    -static $($LLVM_CONFIG --link-static --ldflags --libs core support target native orcjit) \
    -l:libz.a -lrt -ldl -lm -lpthread -o "$OUT/fractlc" 2>&1 | grep -E "error|undefined" || true
strip "$OUT/fractlc"
g++ -std=c++17 -O2 -static fr_frontend.cpp -o "$OUT/fr_frontend"
strip "$OUT/fr_frontend"

cp runtime.o runtime.h runtime.c studio.py web_ui.py fractl-run.sh fractl-lsystem.sh "$OUT/"
cp "Launch FRACTL Classic UI.bat" "$OUT/"
cp studio/index.html studio/style.css studio/app.js studio/stages.js studio/render2d.js studio/render3d.js studio/presets.json "$OUT/studio/"
cp studio/presets/*.fr "$OUT/studio/presets/"
cp meadow.fr tree3d.fr plant.fr tree.fr koch.fr dragon.fr carpet.fr triangle.fr "$OUT/"
mkdir -p "$OUT/viewer/data"
cp viewer/index.html viewer/manifest.json "$OUT/viewer/"
cp viewer/data/bundle.js "$OUT/viewer/data/"
cp tools/bundle/*.bat tools/bundle/README.txt PRESENTATION.md "$OUT/"
mkdir -p "$OUT/docs" && cp STAGE3.md docs/*.pdf "$OUT/docs/"
cp tools/bundle/check.sh "$OUT/tests/"
sed -i 's/\r*$/\r/' "$OUT"/*.bat "$OUT/README.txt"
chmod +x "$OUT/fractlc" "$OUT/fr_frontend" "$OUT/tests/check.sh" "$OUT"/*.sh

./"$OUT/fractlc" meadow.fr --jit --out /tmp/fractl-bundle-check.f3d > /dev/null
cmp -s /tmp/fractl-bundle-check.f3d tests/golden/meadow.f3d && echo "bundle fractlc: output matches golden"
du -sh "$OUT"
echo "bundle ready: $OUT"
