# FRACTL — Stage 2: Branching

Stage 1 compiled fractals that are drawn as **one continuous line**. Stage 2 lets
the turtle **lift its pen** and **save and return to a position**, so it can
draw a branch, jump back to the stem and draw the next one. That is what makes
plants and trees possible.

Stage 2 is built directly on the final Stage 1 code (`..\FRACTL`, 23 Sep). Every
Stage 1 program still compiles to the same output: `triangle.svg` and
`koch.svg` are **byte-identical** to the Stage 1 output, and a test enforces this.

```
  plant.fr ──► lexer ──► parser ──► AST ──► L-system expansion
                           │                       │
              bracket balance check                ▼
              (compile-time error)          turtle IR  (now with MOVE / PUSH / POP)
                                                   │
                                                   ▼
                                       LLVM IR: turtle_push / turtle_pop / turtle_move calls
                                                   │
                                                   ▼
                                       native object file (.o)
                                                   │
                                       link with runtime.c  (state stack + strokes)
                                                   ▼
                                       ./plant  ──►  plant.svg  (2048 separate strokes)
```

---

## 1. What Stage 2 delivers

| Capability | Status |
| --- | --- |
| Everything in Stage 1, with unchanged output for Stage 1 programs | ✅ |
| `f`: move forward **without drawing** | ✅ |
| `[` / `]`: save / restore position and heading (branching) | ✅ |
| Structural symbols (`X`, `Y`, `A`, …) that steer rewriting but draw nothing | ✅ |
| Runtime state stack with no fixed depth limit; one `<polyline>` per stroke | ✅ |
| **Unbalanced brackets rejected at compile time**, with the line number | ✅ (new) |
| Files saved with a UTF-8 byte-order mark (Windows editors) are accepted | ✅ (new) |
| Maximum branch depth reported by `--stats`, the JSON export and the viewer | ✅ (new) |
| Runtime, pipeline and golden-file test suites | ✅ (new) |
| Stochastic rules, optimization pass, 3D | ✖ Stage 3 (see `PHASE3_PLAN.md`) |

Demos:

| File | Shows | Iterations | Commands | Segments | Strokes | Push = Pop | Max depth | Size (px) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `triangle.fr` | Stage 1 | 0 | 7 | 3 | 1 | 0 | 0 | 120 × 107 |
| `koch.fr` | Stage 1 | 4 | 1,792 | 768 | 1 | 0 | 0 | 344 × 394 |
| `dragon.fr` | structural `X`, `Y` | 12 | 12,286 | 4,096 = 2¹² | 1 | 0 | 0 | 495 × 335 |
| `plant.fr` | branching `[ ]` (ABOP fig. 1.24f) | 6 | 21,063 | 6,048 | 2,048 | 4,095 | 12 | 383 × 502 |
| `tree.fr` | branching `[ ]` (ABOP fig. 1.24d) | 7 | 11,769 | 4,118 | 1,458 | 2,186 | 7 | 300 × 518 |
| `carpet.fr` | pen-up `f` (Sierpinski carpet) | 4 | 9,579 | 4,096 | 586 | 0 | 0 | 425 × 420 |

ABOP = Prusinkiewicz & Lindenmayer, *The Algorithmic Beauty of Plants*.

---

## 2. The language

The `.fr` syntax itself is unchanged. Stage 2 gives meaning to more symbols
*inside* the axiom and rule strings:

```
fractal Plant {
    axiom: "X";

    rules {
        X -> "F+[[X]-X]-F[-FX]+X";   // X draws nothing; it only grows the plant
        F -> "FF";                   // every stem segment doubles in length
    }

    angle: 25;
    iterations: 6;
    step: 3;
    start: (0, 0);
    heading: 90;                     // grow upward
}
```

### Stage 2 alphabet

| Symbol | Turtle operation | Argument | Stage |
| --- | --- | --- | --- |
| `F`, `G` | Forward, drawing a line | `step` | 1 |
| `+` | Turn left | `angle` | 1 |
| `-` | Turn right | `angle` | 1 |
| **`f`** | **Move forward without drawing (pen up)** | `step` | **2** |
| **`[`** | **Push: save position and heading** | — | **2** |
| **`]`** | **Pop: return to the last saved state** | — | **2** |
| any other character (`X`, `Y`, `A`, …) | nothing (structural; it only steers rewriting) | — | 1/2 |

### New semantic rule

Every `[` in the axiom and in each rule must have a matching `]` **in the same
string**. If every string is balanced, every expansion is balanced too, because
rewriting replaces a symbol with a balanced string. The compiler checks this
while parsing, so the error carries the line number of the offending string.

---

## 3. What changed in each compiler phase

| Phase | File | Stage 2 change |
| --- | --- | --- |
| Lexical analysis | `lexer.hpp` | Skips a leading UTF-8 byte-order mark (Stage 1 "known limit" fixed). `[` and `]` inside strings are just string characters. |
| Syntax + semantic analysis | `parser.hpp` | **New check `checkBrackets`** on the axiom and every rule production → `line N: unbalanced brackets in the rule for 'X': '[' at position 7 is never closed`. |
| Compile-time evaluation | `rewriter.hpp` | Unchanged. It already copied structural symbols and brackets through. |
| Intermediate code (1) | `lower.hpp` | `f → MOVE`, `[ → PUSH`, `] → POP`. New `maxBranchDepth(program)` helper. |
| Intermediate code (2) | `codegen.cpp` | **Unchanged.** The Stage 1 code generator already emitted `turtle_move`, `turtle_push` and `turtle_pop` for those ops. The frozen interface paid off. |
| Code generation | `emit_object.cpp` | Unchanged. |
| Runtime support | `runtime.c` | Turtle state is now a struct `{x, y, heading}`. **Growable state stack** (no fixed depth limit). **Multiple strokes**: a pen-up move or a pop starts a new `<polyline>`. The bounding box covers drawn strokes only. Same real-size output and number format as Stage 1. A stray pop (only possible if the compile-time check were bypassed) stops with `state stack underflow`. |
| Shared interface | `ir.hpp`, `ast.hpp` | **Unchanged.** All six operations existed from Stage 1. |

---

## 4. Each stage, with real output

Using the smallest branching program, `F[+F]F` with `angle: 90` and `step: 10`.

### Turtle IR — `fr_frontend branch.fr --ir --stats`

```
0: FORWARD 10
1: PUSH 0
2: TURN_LEFT 90
3: FORWARD 10
4: POP 0
5: FORWARD 10
--- stats ---
  forward=3 move=0 left=1 right=0 push=1 pop=1
  max branch depth: 1
```

### LLVM IR — `fractlc branch.fr … --emit-ir branch.ll`

```llvm
define void @fractl_main() {
entry:
  call void @turtle_init(double 1.000000e+01, double 9.000000e+01, double 0.000000e+00,
                         double 0.000000e+00, double 0.000000e+00, ptr @svg_path)
  call void @turtle_forward(double 1.000000e+01)
  call void @turtle_push()
  call void @turtle_turn(double 9.000000e+01)
  call void @turtle_forward(double 1.000000e+01)
  call void @turtle_pop()
  call void @turtle_forward(double 1.000000e+01)
  call void @turtle_finish()
  ret void
}
```

### SVG written by the linked program

```xml
<polyline points="10.00,20.00 20.00,20.00 20.00,10.00"/>   <!-- stem, then the branch going up -->
<polyline points="20.00,20.00 30.00,20.00"/>               <!-- after ']': back at the fork, new stroke -->
```

A `[` does **not** lift the pen: the first branch continues the current line. A
`]` does, because the turtle jumps back to the fork.

### Growth of the plant, `--stats` per iteration

| Iterations | Symbols | Commands | Forward | Push = Pop | Max depth |
| --- | --- | --- | --- | --- | --- |
| 1 | 18 | 14 | 3 | 3 | 2 |
| 2 | 89 | 73 | 18 | 15 | 4 |
| 6 | 25,159 | 21,063 | 6,048 | 4,095 | 12 |

---

## 5. Error reporting

| Input | Message |
| --- | --- |
| `axiom: "F]";` on line 2 | `error: line 2: unbalanced brackets in the axiom: ']' at position 2 has no matching '['` |
| `X -> "F[+X]F[-X";` on line 5 | `error: line 5: unbalanced brackets in the rule for 'X': '[' at position 7 is never closed` |
| *(Stage 1 errors are unchanged)* | `line N: expected ';' …`, `duplicate rule for symbol 'F'`, `expansion exceeded 67108864 symbols` |

Before Stage 2's check, `F]` compiled without complaint and the **compiled
program** crashed when it ran. Now `fractlc` refuses it and exits with status 1.

---

## 6. Design decisions

**The frozen interface paid off.** `ir.hpp` was fixed in Stage 1 with all six
operations. Stage 2 needed no change to `ir.hpp`, `ast.hpp` or `codegen.cpp`:
the frontend started *producing* `MOVE`/`PUSH`/`POP`, and the runtime started
*implementing* them.

**Bracket balance per string, not per expansion.** It would also be possible to
check the fully expanded string, but that string can hold 60+ million symbols and
has no line numbers. Checking each source string is cheaper, gives precise
errors, and is sufficient (balanced strings rewrite to balanced strings).

**Push keeps the pen down; pop and move lift it.** That way a branch that starts
at a fork continues the current polyline. Only a jump (pop or pen-up move) starts
a new one. The plant draws 6,048 segments in 2,048 polylines instead of 6,048.

**No fixed stack limit.** The stack doubles as needed, like the point buffer.
Depth is bounded by the expansion limit anyway, and `maxBranchDepth` reports it
at compile time.

**Stage 1 output preserved exactly.** The runtime still writes Stage 1 programs
as a single `<polyline>` with the same bounding box and number format, and the
Stage 1 console message is unchanged. Only multi-stroke drawings get the longer
message (`… strokes, max depth N, …`).

---

## 7. Building and running

Requires LLVM 20, a C++17 compiler and `make` (WSL/Ubuntu). The Windows-only
frontend build needs MinGW `g++`.

```bash
make                 # fractlc, runtime.o, fr_frontend
make plant           # compile, link, run -> build/plant.svg   (any <name>.fr works)
make test            # frontend + runtime + pipeline + golden tests
make golden          # regenerate tests/golden/ after an intended output change
make demo            # all demos -> viewer/data (.svg, .ll)
./fractl-run.sh tree.fr          # compile, link, run and open the SVG
```

```powershell
.\build.ps1 -Test -Viewer        # Windows: frontend tests + viewer data/bundle
.\Launch FRACTL UI.bat           # browser front end at http://localhost:8732
```

The browser front end has five new presets: plant, tree, bush and weed (all
branching), and the Sierpinski carpet (pen-up moves).

---

## 8. Verification

All results below are from a clean build of this folder.

| Suite | Result |
| --- | --- |
| Clean build, `-Wall -Wextra` | no warnings, no errors |
| `tests/test_frontend.cpp` (Linux and Windows) | **58,470 checks, 0 failed** (Stage 1: 1,904). New: brackets, BOM, lowering of `f [ ]`, branch depth, JSON stats, counts for every demo, and a running-depth check on every command of every demo. |
| `tests/test_runtime.c` | **18 checks, 0 failed**: strokes, pen-up gaps, push/pop restore, bounding box, a 5,000-deep stack, underflow error |
| `tests/test_pipeline.sh` | **8 checks, 0 failed**: `.fr` → `fractlc` → link → run, compile-time bracket errors, and **Stage 1 regression** (`triangle.svg` and `koch.svg` byte-identical to Stage 1) |
| `tests/run_golden.sh` | **12 / 12** golden files (SVG + LLVM IR for all 6 demos) |
| `viewer/tests/smoke_viewer.js` | **40 checks, 0 failed** |
| Web front end | all 16 presets compile, link and run; an unbalanced grammar shows the compile-time error |

Compile and run time on the development laptop (WSL, LLVM 20):

| Demo | `fractlc` | Running the program | Object file |
| --- | --- | --- | --- |
| koch | 0.09 s | 0.004 s | 111 KB |
| tree | 0.29 s | 0.005 s | 580 KB |
| dragon | 0.37 s | 0.005 s | 751 KB |
| plant | 0.63 s | 0.007 s | 1.0 MB |

Compile time and object size grow with the number of commands, because every
command becomes one call instruction. This is the motivation for Stage 3's
recursive code generator.

---

## 9. Known limits

- One `fractal` per file.
- Rules are deterministic: one production per symbol. The AST already stores
  weighted productions for Stage 3.
- 2D only.
- No optimization pass of our own. The `--opt` turn-folding pass exists in the
  older `..\FRACTL_complete` snapshot and is scheduled for Stage 3.

See `PHASE3_PLAN.md` for what comes next.
