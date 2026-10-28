"""FRACTL — a browser form in front of the real compiler.

Not a live in-browser compiler (the project deliberately doesn't build one —
see viewer/README.md) — this is a thin local server: it takes your L-system
as form fields, writes a genuine .fr file, and shells out to the actual
fractlc / cc / the compiled binary, exactly like fractl-run.sh does. The
picture you get back was produced by the real pipeline, not by this script.

    python3 web_ui.py [port]      # default port 8732

Then open http://localhost:8732 in a browser (WSL2 forwards this to Windows
automatically).
"""
import html
import http.server
import json
import os
import re
import subprocess
import sys
import urllib.parse

ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(ROOT, "build")
os.makedirs(BUILD, exist_ok=True)

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8732

PRESETS = {
    "square": {
        "label": "Square",
        "axiom": "F-F-F-F", "rules": "F=F",
        "angle": "90", "iterations": "0", "step": "100", "heading": "0",
        "note": "Four sides, turning 90 degrees at each corner. iterations 0 = draw the axiom as-is.",
    },
    "triangle": {
        "label": "Triangle",
        "axiom": "F--F--F", "rules": "F=F",
        "angle": "60", "iterations": "0", "step": "100", "heading": "0",
        "note": "Two 60-degree turns = the 120-degree exterior angle of an equilateral triangle.",
    },
    "hexagon": {
        "label": "Hexagon",
        "axiom": "F-F-F-F-F-F", "rules": "F=F",
        "angle": "60", "iterations": "0", "step": "80", "heading": "0",
        "note": "Six sides, exterior angle 360/6 = 60 degrees.",
    },
    "snowflake": {
        "label": "Koch snowflake",
        "axiom": "F--F--F", "rules": "F=F+F--F+F",
        "angle": "60", "iterations": "4", "step": "10", "heading": "0",
        "note": "A triangle whose every edge grows a bump, every pass.",
    },
    "koch_curve": {
        "label": "Koch curve (single edge)",
        "axiom": "F", "rules": "F=F+F--F+F",
        "angle": "60", "iterations": "4", "step": "10", "heading": "0",
        "note": "The same rule as the snowflake, but starting from one straight line.",
    },
    "dragon": {
        "label": "Heighway dragon",
        "axiom": "FX", "rules": "X=X+YF+\nY=-FX-Y",
        "angle": "90", "iterations": "12", "step": "5", "heading": "0",
        "note": "X and Y draw nothing — they only steer the rewriting.",
    },
    "sierpinski": {
        "label": "Sierpinski arrowhead",
        "axiom": "F", "rules": "F=G-F-G\nG=F+G+F",
        "angle": "60", "iterations": "6", "step": "6", "heading": "0",
        "note": "F and G both draw; alternating their turn direction builds the triangle.",
    },
    "hilbert": {
        "label": "Hilbert curve",
        "axiom": "A", "rules": "A=-BF+AFA+FB-\nB=+AF-BFB-FA+",
        "angle": "90", "iterations": "5", "step": "8", "heading": "0",
        "note": "A space-filling curve. A and B are placeholders, F does the drawing.",
    },
    "gosper": {
        "label": "Gosper curve (flowsnake)",
        "axiom": "F", "rules": "F=F-G--G+F++FF+G-\nG=+F-GG--G-F++F+G",
        "angle": "60", "iterations": "4", "step": "6", "heading": "0",
        "note": "Hexagonal space-filling curve.",
    },
    "vicsek": {
        "label": "Vicsek box fractal",
        "axiom": "F-F-F-F", "rules": "F=FF-F-F-F-FF",
        "angle": "90", "iterations": "3", "step": "6", "heading": "0",
        "note": "A square whose every edge becomes a plus-shaped detour.",
    },
    "circle": {
        "label": "Circle (approximated)",
        "axiom": "F", "rules": "F=F+F",
        "angle": "2", "iterations": "8", "step": "3", "heading": "0",
        "note": "No curves exist — a tiny turn after every short step just looks like one.",
    },
    "plant": {
        "label": "Fractal plant (branching)",
        "axiom": "X", "rules": "X=F+[[X]-X]-F[-FX]+X\nF=FF",
        "angle": "25", "iterations": "6", "step": "3", "heading": "90",
        "note": "[ saves the turtle, ] jumps back to it: draw a branch, return, draw the next.",
    },
    "tree": {
        "label": "Tree (branching)",
        "axiom": "X", "rules": "X=F[+X]F[-X]+X\nF=FF",
        "angle": "20", "iterations": "7", "step": "2", "heading": "90",
        "note": "Each X grows a trunk segment with one branch to each side.",
    },
    "bush": {
        "label": "Bush (branching)",
        "axiom": "X", "rules": "X=F[+X][-X]FX\nF=FF",
        "angle": "25.7", "iterations": "7", "step": "2", "heading": "90",
        "note": "Two side branches per node, then the stem keeps growing.",
    },
    "weed": {
        "label": "Weed (branching)",
        "axiom": "F", "rules": "F=FF-[-F+F+F]+[+F-F-F]",
        "angle": "22.5", "iterations": "4", "step": "8", "heading": "90",
        "note": "Every segment sprouts a left and a right twig, each drawn and abandoned.",
    },
    "carpet": {
        "label": "Sierpinski carpet (pen-up moves)",
        "axiom": "F", "rules": "F=F+F-F-F-f+F+F+F-F\nf=fff",
        "angle": "90", "iterations": "4", "step": "5", "heading": "0",
        "note": "Lowercase f moves without drawing — that is what leaves the holes.",
    },
}

PAGE_HEAD = """<!doctype html>
<html><head><meta charset="utf-8">
<title>FRACTL — L-system input</title>
<style>
  body { font-family: -apple-system, "Segoe UI", Arial, sans-serif; max-width: 900px;
         margin: 24px auto; padding: 0 16px; color: #1a1a1a; background: #fafbfc; }
  h1 { font-size: 20px; margin-bottom: 2px; }
  .sub { color: #666; font-size: 13px; margin-top: 0; margin-bottom: 20px; }
  form { background: #fff; border: 1px solid #dde3ea; border-radius: 8px; padding: 18px 20px; }
  label { display: block; font-weight: 600; font-size: 13px; margin: 12px 0 4px; }
  input[type=text], input[type=number], textarea, select {
    width: 100%; box-sizing: border-box; font-family: Consolas, monospace; font-size: 13px;
    padding: 7px 9px; border: 1px solid #c7d0da; border-radius: 5px; background: #fff; }
  textarea { height: 84px; resize: vertical; }
  .row { display: flex; gap: 14px; flex-wrap: wrap; }
  .row > div { flex: 1; min-width: 120px; }
  small { color: #888; }
  button { margin-top: 16px; background: #2b4c7e; color: #fff; border: none;
           padding: 9px 20px; border-radius: 5px; font-size: 14px; cursor: pointer; }
  button:hover { background: #1f3a63; }
  .presetbar { background: #eef3f9; border: 1px solid #cdd8e6; border-radius: 8px;
               padding: 14px 20px; margin-bottom: 14px; }
  .presetbar label { margin-top: 0; }
  #presetNote { display: block; margin-top: 7px; color: #44618c; font-size: 12.5px; min-height: 1em; }
  .result { margin-top: 22px; }
  .error { background: #fdecec; border: 1px solid #e3a9a9; color: #8a1f1f;
           padding: 12px 14px; border-radius: 6px; white-space: pre-wrap;
           font-family: Consolas, monospace; font-size: 13px; }
  .ok { background: #eaf6ec; border: 1px solid #a9d8b3; color: #1a5c2a;
        padding: 8px 14px; border-radius: 6px; font-size: 13px; margin-bottom: 10px; }
  .picture { border: 1px solid #dde3ea; border-radius: 8px; background: #fff; padding: 10px;
             overflow: auto; max-height: 80vh; }
  .picture svg { display: block; }   /* shown at its real size: step = pixels */
  .size { color: #44618c; font-size: 12.5px; margin: 0 0 8px; }
  pre.src { background: #f4f6f8; border: 1px solid #dde3ea; border-radius: 6px;
            padding: 10px 12px; font-size: 12px; overflow-x: auto; }
  details summary { cursor: pointer; font-size: 13px; color: #2b4c7e; margin-top: 12px; }
</style></head><body>
<h1>FRACTL</h1>
<p class="sub">Type a deterministic, context-free L-system below, or pick a ready-made one.
This form writes it as a real <code>.fr</code> program and runs it through the actual
compiler (<code>fractlc</code> &rarr; native <code>.o</code> &rarr; link &rarr; run) — the same
pipeline as every other demo in this project, no shortcuts.</p>
"""

FORM = """
<div class="presetbar">
  <label for="preset">Ready-made grammars</label>
  <select id="preset">
    <option value="">-- pick one to fill the form --</option>
    {options}
  </select>
  <small id="presetNote"></small>
</div>

<form method="POST" action="/compile">
  <label>Axiom</label>
  <input type="text" name="axiom" value="{axiom}" placeholder="F--F--F">

  <label>Rules &mdash; one per line, SYMBOL=replacement</label>
  <textarea name="rules" placeholder="F=F+F--F+F">{rules}</textarea>
  <small>F and G draw a line, f moves without drawing, + turns left, - turns right,
  [ saves the turtle's position and heading, ] jumps back to it. Any other single letter
  (X, Y, A, B&hellip;) draws nothing and only steers the rewriting.</small>

  <div class="row">
    <div><label>Angle (degrees)</label><input type="number" step="any" name="angle" value="{angle}"></div>
    <div><label>Iterations</label><input type="number" name="iterations" value="{iterations}"></div>
    <div><label>Step (pixels per line)</label><input type="number" step="any" name="step" value="{step}"></div>
    <div><label>Heading (degrees)</label><input type="number" step="any" name="heading" value="{heading}"></div>
  </div>

  <button type="submit">Compile &amp; draw</button>
</form>
"""

PAGE_TAIL = """
<script>
const PRESETS = %s;
const sel = document.getElementById("preset");
const note = document.getElementById("presetNote");
sel.addEventListener("change", () => {
  const p = PRESETS[sel.value];
  if (!p) { note.textContent = ""; return; }
  for (const k of ["axiom", "rules", "angle", "iterations", "step", "heading"]) {
    const el = document.querySelector("[name=" + k + "]");
    if (el) el.value = p[k];
  }
  note.textContent = p.note || "";
});
</script>
</body></html>
"""

DEFAULTS = dict(axiom="F--F--F", rules="F=F+F--F+F", angle="60", iterations="4",
                step="10", heading="0")


def preset_options() -> str:
    return "\n    ".join(
        f'<option value="{k}">{html.escape(v["label"])}</option>' for k, v in PRESETS.items()
    )


def page_tail() -> str:
    return PAGE_TAIL % json.dumps(PRESETS)


def esc_fr_string(s: str) -> str:
    """Escape a value for embedding inside a "..." string in the .fr DSL."""
    return s.replace("\\", "\\\\").replace('"', '\\"')


def run(cmd, cwd):
    return subprocess.run(cmd, cwd=cwd, shell=True, capture_output=True, text=True)


class Handler(http.server.BaseHTTPRequestHandler):
    def _send(self, body: str, code: int = 200):
        data = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/":
            fields = {k: html.escape(v) for k, v in DEFAULTS.items()}
            self._send(PAGE_HEAD + FORM.format(options=preset_options(), **fields) + page_tail())
        else:
            self._send("not found", 404)

    def do_POST(self):
        if self.path != "/compile":
            self._send("not found", 404)
            return
        length = int(self.headers.get("Content-Length", 0))
        fields = urllib.parse.parse_qs(self.rfile.read(length).decode("utf-8"))
        get = lambda k, d: (fields.get(k, [d]) or [d])[0].strip()

        axiom = get("axiom", "")
        rules_raw = get("rules", "")
        angle = get("angle", "60") or "60"
        iterations = get("iterations", "4") or "4"
        step = get("step", "10") or "10"
        heading = get("heading", "0") or "0"

        vals = dict(axiom=axiom, rules=rules_raw, angle=angle, iterations=iterations,
                    step=step, heading=heading)
        form_html = FORM.format(options=preset_options(),
                                **{k: html.escape(v) for k, v in vals.items()})

        errors = []
        if not axiom:
            errors.append("an axiom is required")
        rule_lines = []
        for line in rules_raw.splitlines():
            line = line.strip()
            if not line:
                continue
            if "=" not in line:
                errors.append(f"rule '{line}' — expected SYMBOL=replacement")
                continue
            sym, rhs = line.split("=", 1)
            sym = sym.strip()
            if len(sym) != 1:
                errors.append(f"rule '{line}' — symbol must be one character")
                continue
            rule_lines.append(f'    {sym} -> "{esc_fr_string(rhs.strip())}";')
        if not rule_lines and not errors:
            errors.append("at least one rule is required")

        if errors:
            body = (PAGE_HEAD + form_html +
                    '<div class="result"><div class="error">' +
                    html.escape("\n".join(errors)) + "</div></div>" + page_tail())
            self._send(body)
            return

        fr_text = (
            "// Generated by web_ui.py from form input — a real, ordinary .fr program.\n"
            "fractal WebInput {\n"
            f'    axiom: "{esc_fr_string(axiom)}";\n\n'
            "    rules {\n" + "\n".join(rule_lines) + "\n    }\n\n"
            f"    angle: {angle};\n"
            f"    iterations: {iterations};\n"
            f"    step: {step};\n"
            "    start: (400, 400);\n"
            f"    heading: {heading};\n"
            "}\n"
        )

        fr_path = os.path.join(BUILD, "web_input.fr")
        with open(fr_path, "w", encoding="utf-8") as f:
            f.write(fr_text)

        steps = [
            "./fractlc build/web_input.fr -o build/web_input.o --svg build/web_input.svg "
            "--emit-ir build/web_input.ll",
            "cc build/web_input.o runtime.o -lm -o build/web_input",
            "./build/web_input",
        ]
        log = []
        failed = False
        for cmd in steps:
            r = run(cmd, cwd=ROOT)
            log.append(f"$ {cmd}\n{r.stdout}{r.stderr}")
            if r.returncode != 0:
                failed = True
                break

        result_html = '<div class="result">'
        if failed:
            result_html += '<div class="error">' + html.escape("\n".join(log)) + "</div>"
        else:
            svg_path = os.path.join(BUILD, "web_input.svg")
            with open(svg_path, "r", encoding="utf-8") as f:
                svg = f.read()
            result_html += '<div class="ok">compiled, linked and ran successfully</div>'
            size = re.search(r'width="([\d.]+)" height="([\d.]+)"', svg)
            if size:
                w, h = (round(float(v)) for v in size.groups())
                result_html += (f'<p class="size">Picture size: {w} &times; {h} px '
                                f'(step {html.escape(step)} = {html.escape(step)} px per line)</p>')
            result_html += f'<div class="picture">{svg}</div>'
            result_html += (
                "<details><summary>Generated .fr source</summary>"
                f"<pre class='src'>{html.escape(fr_text)}</pre></details>"
            )
            result_html += (
                "<details><summary>Build log</summary>"
                f"<pre class='src'>{html.escape(chr(10).join(log))}</pre></details>"
            )
        result_html += "</div>"

        self._send(PAGE_HEAD + form_html + result_html + page_tail())

    def log_message(self, fmt, *args):
        pass


if __name__ == "__main__":
    srv = http.server.ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    print(f"FRACTL web UI: http://localhost:{PORT}")
    srv.serve_forever()
