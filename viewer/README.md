# FRACTL viewer

A static page that makes the compiler's intermediate stages inspectable: source →
tokens → AST → expanded L-system string → turtle IR → rendered SVG → LLVM IR
before and after optimization.

**It is not a live compiler.** Every artifact it shows was produced ahead of time by
the real compiler; the page only navigates them. That is deliberate — the deliverable
is a compiler, and an in-browser recompiler would be scope creep with no academic
payoff and real failure risk on an unfamiliar machine.

No build step, no framework, no server, no CDN. One HTML file with inline CSS and JS.

---

## Running it

```powershell
.\build.ps1 -Viewer          # from the FRACTL root: regenerates data + bundle
```

then double-click `viewer\index.html`.

Piecemeal, if you prefer:

```powershell
.\viewer\build_demos.ps1     # compiles every .fr -> viewer/data/<name>.json (+ frames)
.\viewer\pack.ps1            # packs viewer/data/* into viewer/data/bundle.js
```

### Why `pack.ps1` exists

Browsers block `fetch()` on `file://` URLs, so a double-clicked page cannot read
`data/*.json` directly. A `<script>` tag *is* allowed, so `pack.ps1` inlines every
artifact into `data/bundle.js` and the page reads that instead. Serving the folder
over http makes the bundle unnecessary — the page falls back to `fetch()` on its own
and the badge in the top-right says which mode it is in:

```powershell
python -m http.server 8000     # then open http://localhost:8000/viewer/
```

Rule of thumb: **re-run `pack.ps1` whenever anything in `viewer/data/` changes**,
including Member 2's files. If the page shows a yellow banner, that is what it is
telling you.

Served without a bundle, devtools logs one `404 data/bundle.js`. That is expected —
the page probes for the bundle and falls back to `fetch()` when it is not there.

---

## Who owns which files

| File | Owner | Produced by |
| --- | --- | --- |
| `viewer/index.html` | M1 | hand-written |
| `viewer/manifest.json` | M1 | `build_demos.ps1` (regenerated, do not hand-edit) |
| `viewer/data/<name>.json` | M1 | `fr_frontend --json` |
| `viewer/data/<name>_it<K>.json` | M1 | `build_demos.ps1` (iteration-slider frames) |
| `viewer/data/<name>.svg` | **M2** | the renderer |
| `viewer/data/<name>.ll` | **M2** | `--emit-llvm` before the optimization pass |
| `viewer/data/<name>.opt.ll` | **M2** | after the optimization pass |
| `viewer/data/bundle.js` | shared | `pack.ps1`, run last by whoever commits |

`<name>` is the `.fr` file's stem: `triangle`, `koch`, `dragon`, `plant`,
`stochastic_plant`. Neither member regenerates the other's files. Whoever finishes a
demo last runs `pack.ps1` and commits all of `viewer/data/` together.

Every panel degrades on its own: a demo with no `.svg` yet still shows tokens, AST,
IR and counts, with a placeholder where the drawing goes. Nothing ever renders blank.

---

## The JSON contract

`viewer/data/<name>.json`, written by `json.hpp`:

```jsonc
{
  "name": "koch",                  // demo key (the .fr stem)
  "source": "fractal Koch { … }",  // the .fr text, verbatim
  "tokens": [ { "line": 6, "kind": "fractal", "text": "fractal" },
              { "line": 13, "kind": "number", "text": "60", "num": 60 } ],
  "ast": {
    "name": "Koch", "axiom": "F--F--F",
    "rules": [ { "symbol": "F", "productions": [ { "weight": 1, "text": "F+F--F+F" } ] } ],
    "angle": 60, "iterations": 4, "step": 4,
    "startX": 150, "startY": 350, "heading": 0
  },
  "expanded": {
    "preview": "F+F--F+F--…",      // first 200 characters
    "fullLength": 1792,
    "truncated": true,
    "full": "F+F--F+F--…"          // null when over 200 000 chars
  },
  "program": [ { "op": "FORWARD", "arg": 4 }, { "op": "TURN_LEFT", "arg": 60 } ],
  "programLength": 1792,
  "programTruncated": false,       // true when only a prefix is embedded
  "stats": {
    "symbolCounts": { "+": 510, "-": 514, "F": 768 },
    "opCounts": { "FORWARD": 768, "MOVE": 0, "TURN_LEFT": 510,
                  "TURN_RIGHT": 514, "PUSH": 0, "POP": 0 }
  }
}
```

`stats` is always computed over the **full** program even when `program` is
truncated, so the counters never lie. `productions` is an array because Stage 3
gives one symbol several weighted alternatives — the shape does not change then.

`manifest.json` just lists what exists:

```json
{ "generated": "…", "demos": [ { "name": "koch", "iterationFrames": 4 } ] }
```

`iterationFrames: N` means `<name>_it0.json … <name>_itN.json` exist and the
iteration slider is available for that demo.

---

## Testing the page

```
cd viewer\tests
npm install          # jsdom, only for the test; the viewer has no dependencies
npm test
```

Loads `index.html` headlessly in the `file://` + bundle mode and checks the panels
render, the toggles toggle, missing files produce placeholders, and nothing throws.
40 checks.
