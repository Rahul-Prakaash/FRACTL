# FRACTL frontend — Stage 3 (Member 1)

> **Stage 3:** weighted alternatives and `seed:` (`rewriter.hpp`, `rng.hpp`), the 3D
> symbols `& ^ \ / | ! '` and `G` as a blade (`lower.hpp`), the fields `dimensions`,
> `width`, `taper`, `colors`, and multi-fractal files with a `scene` (`parseFile`,
> `scene.hpp`). The full description is in [STAGE3.md](STAGE3.md); the Stage 2 notes
> below still apply unchanged.

`.fr` source text → tokens → AST → expanded L-system string → `std::vector<TurtleCmd>`.

Pure C++17, header-only, **no LLVM include anywhere in these files**.

## Files

| File | Role |
| --- | --- |
| `ast.hpp` | *frozen contract* — `FractalDecl`, `Production`, `RuleSet` |
| `ir.hpp` | *frozen contract* — `Op`, `TurtleCmd`, `Program` |
| `lexer.hpp` | hand-written scanner, one char of lookahead, line numbers on every token |
| `parser.hpp` | recursive descent, LL(1), `line N: expected …` errors, bracket-balance check (Stage 2) |
| `rewriter.hpp` | parallel (D0L) rewriting, identity fallback, blow-up guard |
| `lower.hpp` | expanded string → turtle IR, `maxBranchDepth` |
| `frontend.hpp` | all of the above behind `compileFrontend(source)` — **this is M2's entry point** |
| `json.hpp` | serializes one compiled `.fr` into the viewer's JSON artifact |
| `fr_frontend.cpp` | CLI driver that prints any stage (`--tokens --ast --expand --ir --stats`) |
| `tests/test_frontend.cpp` | frontend test suite (Stage 1 + 2), no framework |
| `triangle.fr`, `koch.fr` | Stage 1 demos |
| `dragon.fr`, `plant.fr`, `tree.fr`, `carpet.fr` | Stage 2 demos |
| `viewer/` | the stage viewer — see [viewer/README.md](viewer/README.md) |

## Build and run

```powershell
.\build.ps1                  # build fr_frontend.exe
.\build.ps1 -Test            # build + run the test suite
.\build.ps1 -Demo            # dump the AST and stats of every .fr demo
.\build.ps1 -Test -Viewer    # the usual before-you-commit command
```

`make`, `make test`, `make demo` do the same where GNU make is installed.

```
fr_frontend koch.fr --all
fr_frontend koch.fr --expand                      # the single most useful debug view
fr_frontend koch.fr --json viewer/data/koch.json  # viewer artifact
fr_frontend koch.fr --iterations 2 --expand       # override the file's iteration count
```

**Debugging rule of thumb:** if a fractal comes out wrong, print the *expanded string*
first. Wrong string → the bug is in the rewriter or the grammar. Right string, wrong
picture → the bug is in lowering, or downstream in the backend.

## The DSL

```
fractal Koch {
    axiom: "F--F--F";

    rules {
        F -> "F+F--F+F";
    }

    angle: 60;          // degrees per turn
    iterations: 4;      // rewrite passes
    step: 4;            // units per Forward
    start: (150, 350);  // initial turtle position
    heading: 0;         // initial heading, degrees, 0 = +x
}
```

Grammar (LL(1) — every field begins with a distinct keyword, so one token of
lookahead uniquely picks the production):

```
program     -> fractalDecl EOF
fractalDecl -> 'fractal' IDENT '{' field* '}'
field       -> 'axiom'      ':' STRING ';'
             | 'angle'      ':' NUMBER ';'
             | 'iterations' ':' NUMBER ';'
             | 'step'       ':' NUMBER ';'
             | 'heading'    ':' NUMBER ';'
             | 'start'      ':' '(' NUMBER ',' NUMBER ')' ';'
             | 'rules'      '{' ruleDecl* '}' [';']
ruleDecl    -> SYMBOL '->' STRING ';'        SYMBOL = one-character identifier
```

`//`, `#` and `/* … */` comments are skipped. Fields may appear in any order; each
may appear once. Numbers may be negative (`heading: -90`).

### Deliberate deviation: `rules { … }` is a block, not a `field: value`

Early spec sketches wrote the rules as `rules:` with a colon, like the scalar fields.
FRACTL uses a **block** instead, because a rule statement is not one value — Stage 3
turns it into `F -> "F+F" [0.5] | "F-F" [0.5];`, several weighted alternatives per
symbol. A block holds an arbitrary number of those statements without the grammar
changing shape again, and it keeps every scalar field on the uniform
`keyword : value ;` form. A colon before the brace (`rules: { … }`) is accepted so
older `.fr` files still parse, but the `.fr` demos, this document, the report and the
slides all use the block form.

Duplicate detection is keyed on the **symbol**, not on the (symbol, production) pair:
`F -> "A" | "B";` is one statement contributing two productions and is legal, while a
second `F -> …` statement anywhere in the block is the error.

## Stage 2 alphabet

| Symbol | Turtle op | Arg |
| --- | --- | --- |
| `F`, `G` | `Forward` | `step` |
| `f` | `Move` (pen up) | `step` |
| `+` | `TurnLeft` | `angle` |
| `-` | `TurnRight` | `angle` |
| `[` | `Push` | — |
| `]` | `Pop` | — |
| anything else (`X`, `Y`, …) | *structural, draws nothing* | — |

Stage 2 touched only `lower.hpp`, plus the bracket check in `parser.hpp`.
`ir.hpp` already had all six ops and the rewriter already copied structural
symbols through, so there was **no interface change**.

**Bracket rule:** every `[` in the axiom and in each production must close in the
same string. Otherwise the parser throws
`line N: unbalanced brackets in the rule for 'X': '[' at position 7 is never closed`.

## Interface for Member 2

```cpp
#include "frontend.hpp"

fractl::Frontend fe = fractl::compileFrontend(sourceText);
// fe.decl     — step, angle, startX, startY, heading  (drawing parameters)
// fe.expanded — the rewritten string (debug only)
// fe.program  — std::vector<TurtleCmd>, the thing codegen walks
```

Errors arrive as exceptions deriving from `std::runtime_error`
(`LexError`, `ParseError`, `RewriteError`); `what()` is already formatted as
`line N: message`, so the driver can print it straight to stderr.

Turn ops carry the **magnitude** of the angle; the sign is implied by
`TurnLeft` / `TurnRight`, and codegen applies it.

### Orientation and sign convention — agree this, don't debug it

The frontend has **no opinion** about which way the turtle turns on screen. It emits
a direction (`TurnLeft` / `TurnRight`) and a positive magnitude; the runtime owns the
rotation convention. This matters because SVG's y-axis points **down**, so the
mathematically-positive (counter-clockwise) direction renders clockwise unless the
backend compensates.

Settle it out loud before either side calls a render "wrong":

- `heading: 0` means **+x**, pointing right.
- `TurnLeft` means counter-clockwise **as the fractal is drawn on screen**, which in
  SVG coordinates means *subtracting* from the heading.
- Both sides use degrees; no radians cross the boundary.

A picture that comes out mirrored or rotated is a convention mismatch, not a bug in
the expansion — the expanded string will be identical either way.

### First integration test: `triangle.fr`

`triangle.fr` is `iterations: 0`, so the expansion is the axiom itself and the whole
frontend reduces to a 7-command program. Use it as the first end-to-end run, because
it has one unambiguous right answer:

```
axiom      F--F--F         (nothing to expand)
program    FORWARD 100 · TURN_RIGHT 60 · TURN_RIGHT 60 · FORWARD 100
           · TURN_RIGHT 60 · TURN_RIGHT 60 · FORWARD 100
expected   a closed equilateral triangle, side 100, starting at (100, 300)
```

Two `TURN_RIGHT 60` in a row is the 120° exterior angle of the triangle — that pair
is the thing to check first if the shape does not close. If this draws correctly and
`koch.fr` does not, the bug is in expansion or lowering (mine). If *this* is wrong,
it is the renderer or the sign convention (M2's).

## Golden numbers (used by the tests)

| Demo | iterations | expanded symbols | commands | `Forward` | `Move` | `Push` = `Pop` | max depth |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `triangle.fr` | 0 | 7 | 7 | 3 | 0 | 0 | 0 |
| `koch.fr` | 4 | 1792 | 1792 | 768 = 3·4⁴ | 0 | 0 | 0 |
| `dragon.fr` | 12 | 16382 | 12286 | 4096 = 2¹² | 0 | 0 | 0 |
| `plant.fr` | 6 | 25159 | 21063 | 6048 | 0 | 4095 | 12 |
| `tree.fr` | 7 | 13956 | 11769 | 4118 | 0 | 2186 | 7 |
| `carpet.fr` | 4 | 9579 | 9579 | 4096 = 8⁴ | 803 | 0 | 0 |

Koch after one pass: `F+F--F+F--F+F--F+F--F+F--F+F`.

## Portability

Built and tested here with MinGW `g++ 15.2 -std=c++17 -Wall -Wextra -O2`, warning
clean. The headers use nothing beyond the standard library, but the build that
matters is the Linux/LLVM one — **compile the frontend on the Ubuntu box in week 2,
not at the demo**:

```bash
g++ -std=c++17 -Wall -Wextra -O2 -o fr_frontend fr_frontend.cpp
g++ -std=c++17 -Wall -Wextra -O2 -I. -o tests/test_frontend tests/test_frontend.cpp
./tests/test_frontend .
```

The `.ps1` scripts are Windows-only conveniences; `make`, `make test` and `make demo`
cover the same ground on Linux. Note the scripts are deliberately pure ASCII —
PowerShell 5.1 reads `.ps1` files as ANSI, and a stray em dash inside a string is a
parse error.

## Sync with Member 2

- [x] **Stage 2 ops** — `MOVE`/`PUSH`/`POP` emitted by `lower.hpp`, implemented in
      `runtime.c`; covered by `tests/test_runtime.c` and `tests/test_pipeline.sh`.
- [ ] **Sign/orientation convention** — agree it explicitly (see above) before the
      first render is judged.
- [ ] **`triangle.fr` smoke test** — run it as the first integration, expected output
      above.
- [ ] **Linux build** — week 2, on the box where LLVM 20 lives.
- [ ] **`viewer/data/` naming** — `<name>.json` mine, `<name>.svg` / `<name>.ll` /
      `<name>.opt.ll` M2's; neither regenerates the other's. Whoever finishes a demo
      last runs `viewer\pack.ps1` and commits the folder together.
      Details in [viewer/README.md](viewer/README.md).
