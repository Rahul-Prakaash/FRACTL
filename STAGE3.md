# FRACTL — Stage 3: stochastic grammars, recursive code, a JIT and a 3D world

Stage 3 builds on the final Stage 2 code (`..\FRACTL_Phase2`) in its own folder.
Every Stage 1 and Stage 2 program still produces **byte-identical** output, and a
test enforces it.

What Stage 3 adds, in one paragraph: grammars can now be **random** (weighted rules
plus a seed), the compiler emits **one LLVM function per rule symbol** instead of one
call per drawing command, LLVM's **ORC JIT** compiles and runs the result in memory,
the turtle works in **3D** (pitch, roll, tubes and flat blades), a file can describe a
**scene** (a tree among 1,300 independently grown grass tufts, gathered in patches), and
**FRACTL Studio** shows all of it live: edit, Build, and watch the compiled program
grow the model segment by segment, in 2D or 3D, fully offline.

```
 meadow.fr ─► lexer ─► parser ─► AST + scene ─► instance table (positions, yaws, seeds)
                                      │
                                      ▼
                      LLVM IR: Tree.A(i32 d), Tree.F(i32 d), Tuft.T(i32 d), Tuft.B(i32 d) …
                      fractl_main = loop over @instances, switch on fractal, call X.axiom()
                                      │
                 ┌────────────────────┴─────────────────────┐
                 ▼                                          ▼
        ORC LLJIT (in memory)                       native object (.o) + cc
                 │                                          │
                 └──────────────► runtime.c ◄───────────────┘
                                      │
                                      ▼
                     meadow.f3d  (one line per tube / blade, in drawing order)
                                      │
                                      ▼
            FRACTL Studio: WebGL2 instanced rendering, grown in emission order
```

---

## 1. What Stage 3 delivers

| Capability | Status |
| --- | --- |
| Everything in Stages 1–2, with unchanged output | ✅ (6 Stage 2 SVGs byte-identical, tested) |
| Stochastic rules `F -> "F+F" [0.6] \| "F-F" [0.4];` and `seed:` | ✅ |
| Recursive code generation: one LLVM function per rule symbol | ✅ (default; `--unroll` keeps Stage 2's) |
| LLVM ORC LLJIT: compile in memory and run, no object file, no linker | ✅ (`--jit`) |
| `--opt` peephole pass: fold adjacent constant rotations | ✅ (drawing unchanged, tested) |
| 3D turtle: `&` `^` pitch, `\` `/` roll, `\|` turn around, `!` thinner, `'` next colour | ✅ |
| `G` draws a flat **blade** (grass, leaves); `F` draws a **tube** (wood) | ✅ |
| New fields `dimensions`, `width`, `taper`, `colors` | ✅ |
| **Scenes**: several fractals per file, `place` and `scatter` | ✅ |
| FRACTL Studio: live build, 2D/3D toggle, growth replay, IR view, export | ✅ |
| Offline bundle for the presentation laptop | ✅ (`make bundle`) |

---

## 2. The language

### 2.1 Stochastic rules

```
fractal Wildflower {
    seed: 2026;
    axiom: "F";
    rules {
        F -> "F[+F]F[-F]F" [0.34]
           | "F[+F]F"      [0.33]
           | "F[-F]F"      [0.33];
    }
    angle: 25.7;  iterations: 6;  step: 3;  heading: 90;
}
```

A rule may list several productions separated by `|`, each with an optional
`[weight]` (default 1). On every rewrite pass each occurrence of the symbol picks one
production with probability proportional to its weight. `seed:` makes the choice
repeatable: the same seed always grows the same plant, a different seed a different
one. Rules with one production behave exactly as before and draw no random numbers.

The random numbers come from a counter-based generator (`rng.hpp`, mirrored
bit-for-bit in `runtime.c`): the *n*-th choice made on pass *k* uses
`mix64(seed ^ mix64(k·P ^ mix64(n)))`. Because the unrolled compiler makes its
choices left to right per pass, and the recursive program visits the same symbols
depth-first — which is still left to right within every pass — both make exactly the
same choices. The test suite checks this on every demo.

### 2.2 The 3D turtle

The 3D turtle carries a heading **H**, a left vector **L** and an up vector **U**
(the frame from *The Algorithmic Beauty of Plants*, ch. 1.5), plus a width and a
colour index. It starts at the origin pointing up.

| Symbol | Meaning | IR operation |
| --- | --- | --- |
| `F` | grow forward, drawing a **tube** of the current width | `FORWARD` |
| `G` | grow forward, drawing a flat **blade** (2D: a line, as before) | `BLADE` |
| `f` | move without drawing | `MOVE` |
| `+` / `-` | turn left / right about U | `TURN_LEFT` / `TURN_RIGHT` |
| `&` / `^` | pitch down / up about L | `PITCH_DOWN` / `PITCH_UP` |
| `\` / `/` | roll left / right about H | `ROLL_LEFT` / `ROLL_RIGHT` |
| `\|` | turn around (180°) | `TURN_LEFT 180` |
| `!` | multiply the width by `taper` | `THIN` |
| `'` | use the next colour in `colors` | `NEXT_COLOR` |
| `[` / `]` | save / restore the whole turtle state | `PUSH` / `POP` |

New fields: `dimensions: 3;`, `width:` (initial tube diameter / blade width),
`taper:` (factor for `!`, in (0, 1]), `colors: ("#5b3b24", "#3f8a2a", …);`.
In a 2D fractal the 3D-only symbols are ignored, so old grammars are unaffected.

### 2.3 Scenes

```
fractal Tree  { dimensions: 3; seed: 42; … }
fractal Tuft  { dimensions: 3; seed: 7;  … }

scene Meadow {
    place Tree at (0, 0, 0);
    scatter Tuft 800 within 30 clear 1.5 seed 11;
    scatter Tuft 350 within 46 clear 30 seed 12;
}
```

`place` puts one copy at a position (optional `yaw`, `scale`). `scatter` places
*count* copies uniformly in a ring between `clear` and the radius, each with a random
rotation, a size between `1 − vary` and `1 + vary` times `scale` (default `vary 0.2`),
and **its own seed**, so every tuft grows differently from the same grammar.
`clumps N` gathers the copies into N patches: each copy picks a patch and lands at a
Gaussian distance from its centre, and copies near a centre grow taller, which gives
the mounded look of real grass. All placements are computed at compile time, from
`seed`, so the scene is identical on every build.

### 2.4 New compile-time checks

`weights … must be > 0`, `'dimensions' must be 2 or 3`, `taper must be in (0, 1]`,
`colour "green" must look like "#4f8a2b"`, `a file with several fractals needs a
'scene'`, `scene places unknown fractal 'X'`, `fractal 'X' is used in a scene, so it
needs 'dimensions: 3;'`, `duplicate fractal name`, `'clear' must be between 0 and
the radius`, `scatter count must be between 1 and 100000`, `iterations must be <= 60`.
All carry the line number, like the Stage 1–2 errors.

---

## 3. Code generation

### 3.1 Recursive: the IR is the size of the grammar

Stage 2 expanded the grammar at compile time and emitted one call per drawing
command. Stage 3 instead emits one function per rule symbol, taking the number of
rewrite passes still to apply:

```llvm
define internal void @Plant.X(i32 %d) {
entry:
  %done = icmp eq i32 %d, 0
  br i1 %done, label %leaf, label %grow
leaf:                                  ; no passes left: X draws nothing
  ret void
grow:
  %next = sub i32 %d, 1
  call void @Plant.F(i32 %next)        ; X -> "F+[[X]-X]-F[-FX]+X"
  call void @turtle_turn(double 2.500000e+01)
  call void @turtle_push()
  call void @turtle_push()
  call void @Plant.X(i32 %next)
  …
}
```

A stochastic symbol draws `fractl_rand(level)`, multiplies by the total weight and
branches to the chosen production — the same arithmetic the unrolled compiler does at
compile time. For a scene, the instances become a constant table and `fractl_main`
is a loop: load the row, `switch` on the fractal index, `turtle3_begin(…)`,
`fractl_seed(seed)`, `call X.axiom()`, `turtle3_end()`.

| Program | Recursive IR | Unrolled IR | Recursive object | Unrolled object | Object emit (rec / unrolled) |
| --- | --- | --- | --- | --- | --- |
| `plant.fr` (Stage 2) | 70 lines | 21,086 lines | 2.7 KB | 1.0 MB | 0.08 s / 0.88 s |
| `dragon.fr` (Stage 2) | 60 lines | 12,309 lines | 2.5 KB | 751 KB | 0.17 s / 0.44 s |
| `tree3d.fr` | 225 lines | 21,793 lines | 7.9 KB | 1.1 MB | 0.10 s / 0.74 s |
| `meadow.fr` (1,301 instances) | **531 lines** | **201,463 lines** | 91 KB | 11.9 MB | **0.17 s / 4.58 s** |

The drawings are byte-identical in both modes. Compile time no longer grows with the
size of the picture: adding iterations or instances costs run time (microseconds per
segment), not LLVM time.

### 3.2 ORC JIT

`fractlc --jit` hands the module to LLVM's `LLJIT`, with the runtime (linked into
`fractlc` itself) provided as absolute symbols. `lookup("fractl_main")` compiles it to
machine code in memory, and `fractlc` calls it directly: no object file, no `cc`, no
executable on disk. The JIT uses the fast instruction selector (`-O0`), which suits
code made of call sequences with constant arguments.

### 3.3 `--opt`

A peephole pass over the IR merges strictly adjacent calls to the same rotation with
constant arguments (`turtle_turn`, `turtle3_yaw`, `turtle3_pitch`, `turtle3_roll`) and
deletes a run that adds up to a whole turn. Rotations about different axes are never
merged (3D rotations don't commute). Every demo's drawing is unchanged with `--opt`.

---

## 4. Runtime: the `.f3d` stream

The 2D runtime is unchanged. The 3D runtime writes one line per segment, in exactly
the order the compiled program drew them:

```
# FRACTL 3D segments v1
o Tree 0 #4a2f1c,#3f8a2a,#7cbd45            instance name, index, palette
t 0.000 0.000 0.000 0.000 0.900 0.000 1.000 0                    tube: from, to, radius, colour
b -0.62 3.1 0.4 -0.9 3.6 0.7  0.71 0.0 -0.71  0.45 0.35 1        blade: from, to, side, w0, w1, colour
```

Positions are in world space (instance position, yaw and scale applied). The stream
is plain text, so golden tests compare it byte for byte.

---

## 5. FRACTL Studio

`Launch FRACTL Studio.bat` (or `python3 studio.py`) serves a single-page app on
`http://127.0.0.1:8732`. Everything is local: Python standard library on the
server, plain HTML/CSS/JavaScript and a hand-written WebGL2 renderer in the browser —
no CDN, no web fonts, no packages, no internet.

* **Build** (Ctrl+Enter) sends the editor text to the server, which writes a real
  `.fr` file and runs `fractlc --jit`. The compiled program writes the `.svg` or
  `.f3d`, and the page draws it. Nothing is pre-rendered.
* **2D / 3D toggle** switches the canvas, the preset list and the theme; each mode
  keeps its own source and last result.
* **Compiler pipeline** lists every phase with its measured time: lex/parse/check,
  LLVM IR generation (functions, instructions), optimizer, ORC JIT (or object +
  link), run, GPU upload, growth.
* **Growth**: every segment gets a birth time from its position in the stream; the
  GPU grows it out of its base (vertex shader) and the newest segments glow. A light
  marks the turtle's current position. *Together* lets every instance grow at once
  (grass sprouts across the field while the tree climbs); *Emission order* replays
  the exact sequence the program drew. Duration 8–180 s, speed ½×–4×, scrubber.
* **3D**: instanced tubes and double-sided blades, hemisphere + sun lighting, fog,
  ground with a contact shadow, orbit / pan / zoom. The GPU in use is shown in the
  top-right corner.
* **LLVM IR** drawer shows the generated module; **Export** saves the drawing as
  `.svg` (2D) or `.obj` (3D, opens in Blender); **Stages ↗** opens the Stage 1–2
  viewer for the classic demos.
* **Rendering** (3D): blades are pointed and curved, shaded from a dark root to a
  lighter tip (a few dry, straw-coloured tips), and let light through when the sun is
  behind them; a 2048² shadow map with 3×3 filtered taps lets the tree shade the grass
  and the grass shade the ground; the grass sways in the wind, more at the tips. All of
  it can be switched off in the settings for a very slow GPU.

### Stage 1–2 features, kept in the studio

Nothing from the earlier stages was dropped; each is available live, for whatever is
in the editor:

| Stage 1–2 feature | In Stage 3 |
| --- | --- |
| Stage viewer: source, tokens, AST, expanded string, turtle IR, counts, iteration slider, LLVM IR (-O0 / optimized / side by side) | **Stages** tab — computed on demand by `fr_frontend --json` (now with `--fractal` for scene files) and `fractlc --ir-only`; IR shown recursive, unrolled, with `--opt`, or side by side |
| Web form: axiom, rules, angle, iterations, step, heading | **Form** switch in the editor (adds seed, and width/taper/colours in 3D); the original `web_ui.py` is also kept, as **Launch FRACTL Classic UI.bat** (port 8733) |
| Build log and generated `.fr` source | **Build log** tab: every command the build ran, its output and timing |
| Terminal workflow (`fractlc -o`, `cc`, `./out`, `fr_frontend --stats`, `fractl-run.sh`, `fractl-lsystem.sh`) | unchanged; the Stages tab lists the exact commands, and **Object + link** in the settings runs the Stage 1 path |
| Static stage viewer (`viewer/index.html`) | unchanged, rebuilt for the 2D demos, linked from **Stages ↗** |

Measured through the studio on the development laptop (static bundle, WSL):

| Preset | Instances | Segments | parse | IR | JIT | run | Server round trip |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Meadow | 1,301 | 39,588 | 0.6 ms | 4.4 ms | 6.7 ms | 55 ms | 136 ms |
| Tree | 1 | 4,846 | < 0.1 ms | 0.4 ms | 3 ms | 5 ms | 20 ms |

The meadow figures were measured in Edge on this laptop's integrated Intel GPU, which
then renders the full scene (shadows and wind on) at 60 fps.

The build itself is effectively instant; the time the audience sees is the growth
animation, which the presenter controls.

---

## 6. Files

| File | Owner | Stage 3 role |
| --- | --- | --- |
| `ast.hpp` | shared | + `dimensions`, `seed`, `width`, `taper`, `colors`; `Placement`, `SceneDecl`, `SourceFile` |
| `ir.hpp` | shared | + `BLADE`, `PITCH_*`, `ROLL_*`, `THIN`, `NEXT_COLOR` (appended; Stage 1–2 values unchanged) |
| `rng.hpp` | shared | counter-based random numbers, mirrored in `runtime.c` |
| `lexer.hpp` | M1 | + keywords |
| `parser.hpp` | M1 | + alternatives/weights, 3D fields, multiple fractals, `scene`, `parseFile` |
| `rewriter.hpp` | M1 | + stochastic expansion |
| `lower.hpp` | M1 | `terminalCmd`: one symbol → one op, shared with the recursive backend |
| `scene.hpp` | M1 | compile-time instance placement |
| `codegen.cpp` | M2 | recursive and unrolled modes, 3D calls, instance table, `--opt` |
| `jit.cpp` | M2 | ORC LLJIT |
| `runtime.c`, `runtime.h` | M2 | + RNG, 3D turtle, `.f3d` writer |
| `fractlc.cpp` | M2 | + `--jit --unroll --opt --out --timings` |
| `studio.py`, `studio/` | both | FRACTL Studio |
| `meadow.fr`, `tree3d.fr`, `studio/presets/*.fr` | both | demos |
| `tools/make_bundle.sh`, `tools/bundle/` | both | offline presentation bundle |

---

## 7. Verification

| Suite | Result |
| --- | --- |
| Clean build, `-Wall -Wextra` | no warnings |
| `tests/test_frontend.cpp` (Linux and Windows) | **79,580 checks, 0 failed** (Stage 2: 58,470). New: weights, seeds, distribution of choices, the RNG's reference value, 3D lowering, 3D fields, scenes, uniform and clumped placement, the JSON export's 3D fields, every new error message |
| `tests/test_runtime.c` | **36 checks, 0 failed** — Stage 2's 18 plus exact 3D geometry (yaw, pitch, push/pop, instance transform, blades) and RNG determinism |
| `tests/test_pipeline.sh` | **8 checks, 0 failed** (Stage 1 regression included) |
| `tests/test_modes.sh` | **59 checks, 0 failed** — for all 17 demos and presets: recursive = unrolled, object+link = JIT, `--opt` = no `--opt`, Stage 2 demos = Stage 2's SVGs; recursive IR ≪ unrolled IR |
| `tests/run_golden.sh` | **16 / 16** golden files (SVG or `.f3d`, plus IR) |
| Studio | every preset builds through the server; compile errors highlight the line |
| Bundle | `tests/check.sh` passes: static `fractlc` starts, JIT-builds the meadow, output = golden |

`make test` runs all of the above except the studio and bundle checks.

---

## 8. Building, running and presenting

```bash
make                 # fractlc (LLVM linked statically), runtime.o, fr_frontend
make test            # frontend, runtime, pipeline, golden and mode tests
make meadow          # build/meadow.f3d via object file + link
./fractlc meadow.fr --jit --out /tmp/m.f3d --timings      # the same, in memory
make bundle          # dist/FRACTL-Studio: copy this folder to the presentation laptop
python3 studio.py    # or double-click "Launch FRACTL Studio.bat"
```

See `PRESENTATION.md` for the laptop checklist and the demo script.

---

## 9. Known limits

* Parametric L-systems (`F(x)`) and context-sensitive rules are not supported.
* Leaves are blades, not arbitrary polygons (ABOP's `{ }`).
* Grammars can be large, but a 3D drawing beyond a few hundred thousand segments
  makes the browser slow to upload; the compiler itself is not the bottleneck.
