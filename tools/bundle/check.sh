#!/bin/sh
cd "$(dirname "$0")/.." || exit 1
ok()   { echo "[ ok ] $*"; }
bad()  { echo "[FAIL] $*"; fail=1; }
fail=0
if command -v python3 >/dev/null 2>&1; then ok "python3 $(python3 -c 'import sys; print(sys.version.split()[0])')"
else bad "python3 is missing - inside Ubuntu run: sudo apt install python3"; fi
chmod +x fractlc 2>/dev/null
if ./fractlc --help >/dev/null 2>&1; then ok "fractlc starts (static LLVM build, no libraries needed)"
else bad "fractlc does not start on this system"; fi
t0=$(date +%s%N)
if ./fractlc meadow.fr --jit --out /tmp/fractl-check.f3d > /tmp/fractl-check.log 2>&1; then
    t1=$(date +%s%N)
    ok "compiled + ran the Meadow scene with the JIT in $(( (t1 - t0) / 1000000 )) ms"
    tail -1 /tmp/fractl-check.log | sed 's/^/       /'
else bad "JIT build failed: $(cat /tmp/fractl-check.log)"; fi
if command -v cc >/dev/null 2>&1; then ok "cc found - 'Object + link' mode is available too"
else echo "[info] no C compiler - fine, the default JIT mode does not need one"; fi
[ "$fail" = 0 ] && echo "All good: double-click 'Launch FRACTL Studio.bat'." || echo "Fix the [FAIL] lines above first."
