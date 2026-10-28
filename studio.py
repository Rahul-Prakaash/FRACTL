"""FRACTL Studio — local server for the live compiler front end.

Everything runs on this machine with the Python standard library only: no
internet, no CDN, no packages. Each Build request writes the editor's text to a
real .fr file and runs the real compiler on it:

    fractlc input.fr --jit --out out.f3d|out.svg --emit-ir out.ll --timings

so the picture you see is produced by LLVM-compiled code that ran a moment ago,
never by a pre-made file.

    python3 studio.py [port]          # default 8732, then open http://127.0.0.1:8732
"""
import hashlib
import http.server
import json
import os
import re
import shutil
import socket
import subprocess
import tempfile
import sys
import threading
import time
import urllib.parse

ROOT = os.path.dirname(os.path.abspath(__file__))
STUDIO = os.path.join(ROOT, "studio")
WORK = os.path.join(tempfile.gettempdir(), "fractl-studio-%d-%s" % (
    os.getuid(), hashlib.sha1(ROOT.encode()).hexdigest()[:8])) if hasattr(os, "getuid")     else os.path.join(ROOT, "build", "studio")
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8732
MAX_OUTPUT = 60 * 1024 * 1024
TIMEOUT_S = 170

os.makedirs(WORK, exist_ok=True)
LOCK = threading.Lock()


def local_copy(name):
    """Run binaries from the Linux filesystem: loading a 35 MB executable through
    /mnt/<drive> costs seconds per build, a local copy costs nothing."""
    src = os.path.join(ROOT, name)
    if not os.path.exists(src) or WORK.startswith(ROOT):
        return src
    dst = os.path.join(WORK, name)
    st = os.stat(src)
    if not os.path.exists(dst) or os.path.getsize(dst) != st.st_size or os.path.getmtime(dst) < st.st_mtime:
        shutil.copy2(src, dst)
        os.chmod(dst, 0o755)
    return dst


FRACTLC = local_copy("fractlc")
RUNTIME_O = local_copy("runtime.o")
FR_FRONTEND = local_copy("fr_frontend")

TYPES = {".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8",
         ".css": "text/css; charset=utf-8", ".json": "application/json",
         ".svg": "image/svg+xml", ".png": "image/png", ".ico": "image/x-icon",
         ".fr": "text/plain; charset=utf-8", ".ll": "text/plain; charset=utf-8"}


def load_presets():
    with open(os.path.join(STUDIO, "presets.json"), encoding="utf-8") as f:
        meta = json.load(f)
    out = []
    for p in meta:
        path = os.path.join(ROOT, p["file"])
        try:
            with open(path, encoding="utf-8") as f:
                p = dict(p, source=f.read())
        except OSError:
            continue
        out.append(p)
    return out


def shown(c):
    return os.path.basename(c) if c.startswith(WORK) or c.startswith(ROOT) else c


def run(cmd, timeout):
    t = time.perf_counter()
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=timeout)
    return r, (time.perf_counter() - t) * 1000.0


def fractal_names(source):
    return re.findall(r"^\s*fractal\s+([A-Za-z_]\w*)", source, re.M)


def inspect_source(req):
    """Stage 1's stage viewer, live: the real frontend's --json export (tokens, AST,
    expanded string, turtle IR, counts) for the text in the editor."""
    source = req.get("source", "")
    if len(source) > 200_000:
        return {"ok": False, "error": "source is too large"}
    if not os.path.exists(FR_FRONTEND):
        return {"ok": False, "error": "fr_frontend is missing - run 'make'"}
    fr = os.path.join(WORK, "inspect.fr")
    out = os.path.join(WORK, "inspect.json")
    with open(fr, "w", encoding="utf-8") as f:
        f.write(source)
    if os.path.exists(out):
        os.remove(out)
    names = fractal_names(source)
    cmd = [FR_FRONTEND, fr, "--json", out]
    if req.get("fractal") in names:
        cmd += ["--fractal", req["fractal"]]
    it = req.get("iterations")
    if isinstance(it, int) and 0 <= it <= 60:
        cmd += ["--iterations", str(it)]
    try:
        r, ms = run(cmd, 60)
    except subprocess.TimeoutExpired:
        return {"ok": False, "error": "the frontend took too long"}
    if r.returncode != 0 or not os.path.exists(out):
        msg = (r.stderr or r.stdout).strip()
        m = re.search(r"line (\d+):", msg)
        return {"ok": False, "error": msg, "line": int(m.group(1)) if m else None}
    with open(out, encoding="utf-8") as f:
        data = json.load(f)
    return {"ok": True, "data": data, "fractals": names, "ms": ms,
            "cmd": " ".join(shown(c) for c in cmd)}


def ir_source(req):
    """LLVM IR for one code generation mode, without running anything."""
    source = req.get("source", "")
    if len(source) > 200_000:
        return {"ok": False, "error": "source is too large"}
    mode = req.get("mode", "recursive")
    fr = os.path.join(WORK, "ir_" + re.sub(r"\W", "", mode) + ".fr")
    ll = fr[:-3] + ".ll"
    with open(fr, "w", encoding="utf-8") as f:
        f.write(source)
    if os.path.exists(ll):
        os.remove(ll)
    cmd = [FRACTLC, fr, "--ir-only", "--emit-ir", ll, "--timings"]
    if mode in ("unroll", "unroll-opt"):
        cmd.append("--unroll")
    if mode in ("opt", "unroll-opt"):
        cmd.append("--opt")
    try:
        r, ms = run(cmd, TIMEOUT_S)
    except subprocess.TimeoutExpired:
        return {"ok": False, "error": "code generation took too long"}
    if r.returncode != 0 or not os.path.exists(ll):
        return {"ok": False, "error": (r.stderr or r.stdout).strip()}
    lines = 0
    with open(ll, encoding="utf-8") as f:
        text = f.read(600_000)
        lines = text.count("\n")
        for chunk in iter(lambda: f.read(1 << 20), ""):
            lines += chunk.count("\n")
    m = re.search(r"fractlc-timings: (\{.*\})", r.stdout)
    return {"ok": True, "ir": text, "lines": lines, "truncated": lines > text.count("\n"),
            "timings": json.loads(m.group(1)) if m else {}, "ms": ms,
            "cmd": " ".join(shown(c) for c in cmd)}


def compile_source(req):
    source = req.get("source", "")
    if len(source) > 200_000:
        return {"ok": False, "error": "source is too large"}
    dims = 3 if re.search(r"dimensions\s*:\s*3", source) else 2
    jit = req.get("backend", "jit") != "aot"
    unroll = bool(req.get("unroll"))
    opt = bool(req.get("opt"))

    fr = os.path.join(WORK, "input.fr")
    out = os.path.join(WORK, "out.f3d" if dims == 3 else "out.svg")
    ll = os.path.join(WORK, "out.ll")
    obj = os.path.join(WORK, "out.o")
    exe = os.path.join(WORK, "out")
    for p in (out, ll, obj, exe):
        if os.path.exists(p):
            os.remove(p)
    with open(fr, "w", encoding="utf-8") as f:
        f.write(source)

    cmd = [FRACTLC, fr, "--out", out, "--emit-ir", ll, "--timings"]
    cmd += ["--jit"] if jit else ["-o", obj]
    if unroll:
        cmd.append("--unroll")
    if opt:
        cmd.append("--opt")

    steps = []
    try:
        r, ms = run(cmd, TIMEOUT_S)
    except subprocess.TimeoutExpired:
        return {"ok": False, "error": f"compile took longer than {TIMEOUT_S} s"}
    steps.append({"cmd": " ".join(shown(c) for c in cmd),
                  "ms": ms, "stdout": r.stdout, "stderr": r.stderr})
    if r.returncode != 0:
        msg = (r.stderr or r.stdout).strip()
        m = re.search(r"line (\d+):", msg)
        return {"ok": False, "error": msg, "line": int(m.group(1)) if m else None, "steps": steps}

    if not jit:
        cc = shutil.which("cc") or shutil.which("gcc")
        if not cc:
            return {"ok": False, "error": "AOT mode needs a C compiler (cc) to link; use JIT", "steps": steps}
        for cmd2 in ([cc, obj, RUNTIME_O, "-lm", "-o", exe], [exe]):
            r2, ms2 = run(cmd2, TIMEOUT_S)
            steps.append({"cmd": " ".join(shown(c) for c in cmd2),
                          "ms": ms2, "stdout": r2.stdout, "stderr": r2.stderr})
            if r2.returncode != 0:
                return {"ok": False, "error": (r2.stderr or r2.stdout).strip(), "steps": steps}

    timings = {}
    for s in steps:
        m = re.search(r"fractlc-timings: (\{.*\})", s["stdout"])
        if m:
            timings = json.loads(m.group(1))
    if not jit:
        timings["link_ms"] = steps[1]["ms"]
        timings["run_ms"] = steps[2]["ms"]

    if not os.path.exists(out) or os.path.getsize(out) > MAX_OUTPUT:
        return {"ok": False, "error": "the compiled program produced no drawing", "steps": steps}
    with open(out, encoding="utf-8") as f:
        output = f.read()
    ir = ""
    if os.path.exists(ll):
        with open(ll, encoding="utf-8") as f:
            ir = f.read(400_000)
    report = "\n".join((s["stdout"] + s["stderr"]).strip() for s in steps if (s["stdout"] + s["stderr"]).strip())
    return {"ok": True, "dims": dims, "output": output, "ir": ir, "timings": timings,
            "steps": steps, "report": report, "backend": "jit" if jit else "aot"}


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def _send(self, code, body, ctype):
        data = body if isinstance(body, bytes) else body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def _json(self, obj, code=200):
        self._send(code, json.dumps(obj), "application/json")

    def do_GET(self):
        path = urllib.parse.urlparse(self.path).path
        if path == "/api/presets":
            return self._json(load_presets())
        if path == "/api/health":
            return self._json({"ok": True, "fractlc": os.path.exists(os.path.join(ROOT, "fractlc"))})
        if path == "/":
            path = "/studio/index.html"
        if path.startswith("/viewer/") or path.startswith("/studio/"):
            full = os.path.normpath(os.path.join(ROOT, path.lstrip("/")))
            if full.startswith(ROOT) and os.path.isfile(full):
                with open(full, "rb") as f:
                    return self._send(200, f.read(), TYPES.get(os.path.splitext(full)[1], "application/octet-stream"))
        self._send(404, "not found", "text/plain")

    def do_POST(self):
        handler = {"/api/compile": compile_source, "/api/inspect": inspect_source,
                   "/api/ir": ir_source}.get(urllib.parse.urlparse(self.path).path)
        if not handler:
            return self._send(404, "not found", "text/plain")
        n = int(self.headers.get("Content-Length", 0))
        try:
            req = json.loads(self.rfile.read(n).decode("utf-8"))
        except ValueError:
            return self._json({"ok": False, "error": "bad request"}, 400)
        t = time.perf_counter()
        with LOCK:
            res = handler(req)
        res["server_ms"] = (time.perf_counter() - t) * 1000.0
        self._json(res)

    def log_message(self, fmt, *args):
        sys.stderr.write("studio: " + (fmt % args) + "\n")


class V6Server(http.server.ThreadingHTTPServer):
    address_family = socket.AF_INET6
    daemon_threads = True

    def server_bind(self):
        self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
        super().server_bind()


if __name__ == "__main__":
    if not os.path.exists(os.path.join(ROOT, "fractlc")):
        sys.exit("fractlc is missing - run 'make' first (or use the bundle)")
    srv = http.server.ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    srv.daemon_threads = True
    try:
        v6 = V6Server(("::", PORT), Handler)
        threading.Thread(target=v6.serve_forever, daemon=True).start()
    except OSError:
        pass
    print(f"FRACTL Studio on http://127.0.0.1:{PORT}  (Ctrl+C to stop)", flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
