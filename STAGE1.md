# FRACTL — Stage 1

**FRACTL** is a compiler for a small domain-specific language that describes
fractals as L-systems. It translates a `.fr` source file into a **native
executable** that draws the fractal as an SVG image.

It is a compiler, not an interpreter. Nothing in the generated program knows what
an L-system is: the grammar is expanded, turned into machine code, and the
resulting binary only issues drawing calls.

```
  koch.fr ──► lexer ──► parser ──► AST ──► L-system expansion
                                              │
                                              ▼
                                        turtle IR (TurtleCmd[])
                                              │
                                              ▼
                                        LLVM IR (fractl_main)
                                              │
                                              ▼
                                        native object file (.o)
                                              │
                                     link with runtime.c
                                              ▼
                                      ./koch  ──►  koch.svg
```

---

## 1. What Stage 1 delivers

A complete pipeline, end to end, for fractals drawn as a single continuous line.

| Capability | Status |
| --- | --- |
| Tokenise, parse and validate the `.fr` language | ✅ |
| Expand the L-system at compile time | ✅ |
| Generate LLVM IR, verified with `verifyFunction` | ✅ |
| Emit a real native object file for the host machine | ✅ |
| C runtime that renders the drawing to a scaled SVG | ✅ |
| Line-numbered error messages for bad input | ✅ |
| Branching fractals (plants, trees) | ✖ Stage 2 |
| Optimization pass over the generated IR | ✖ Stage 3 |

Working demos: `triangle.fr` (7 commands) and `koch.fr` (1792 commands, 768 line
segments).

---

## 2. The language

```
fractal Koch {
    axiom: "F--F--F";        // the starting string

    rules {
        F -> "F+F--F+F";     // every F is replaced by this, each pass
    }

    angle: 60;               // degrees per turn
    iterations: 4;           // number of rewrite passes
    step: 4;                 // units per forward move
    start: (150, 350);       // initial turtle position
    heading: 0;              // initial heading in degrees, 0 = +x
}
```

Grammar (LL(1) — one token of lookahead is enough, because every field begins
with a distinct keyword):

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
ruleDecl    -> SYMBOL '->' STRING ';'         SYMBOL = a one-character identifier
```

Fields may appear in any order, and each may appear only once. `//`, `#` and
`/* … */` comments are skipped. Numbers may be negative (`heading: -90`).

### Stage 1 alphabet

| Symbol | Turtle operation | Argument |
| --- | --- | --- |
| `F`, `G` | Forward, drawing a line | `step` |
| `+` | Turn left | `angle` |
| `-` | Turn right | `angle` |
| anything else | ignored | — |

`Move` (pen up), `Push` and `Pop` already exist in the shared IR definition but
nothing produces them yet; Stage 2 maps `f`, `[` and `]` onto them.

---

## 3. Compiler phases

| Phase | Where it lives | What it does in Stage 1 |
| --- | --- | --- |
| **Lexical analysis** | `lexer.hpp` | Hand-written scanner with one character of lookahead. Produces keywords, identifiers, numbers, strings, `->` and punctuation; skips comments; records a line number on every token. |
| **Syntax analysis** | `parser.hpp` | Recursive-descent LL(1) parser. Builds the AST (`FractalDecl`) and reports `line N: expected …` on malformed input. |
| **Semantic analysis** | checks inside `parser.hpp` and `rewriter.hpp` | Each field may appear once; each symbol may have one rule statement; rule symbols must be a single character. The rule table (`map<char, RuleSet>`) serves as the symbol table. The rewriter enforces an expansion limit. |
| **Compile-time evaluation** | `rewriter.hpp` | Rewrites the axiom `iterations` times, in parallel (a D0L-system: every symbol is replaced simultaneously). This is *partial evaluation*, not one of the six textbook phases. |
| **Intermediate code generation (1)** | `lower.hpp` | Expanded string → `std::vector<TurtleCmd>`, a flat list of drawing operations. This is the interface between the two halves of the project. |
| **Intermediate code generation (2)** | `codegen.cpp` | Turtle IR → LLVM IR. Builds a `Module`, declares the runtime as external C functions, and emits `void fractl_main()` with one call per command, then checks the result with `verifyFunction`. |
| **Code optimization** | — | No pass of our own in Stage 1. LLVM's backend still applies its own default machine-level optimizations while writing the object file. |
| **Code generation** | `emit_object.cpp` | Host triple → `TargetMachine` → instruction selection and register allocation → a native ELF object file. |
| **Runtime support** | `runtime.c` | Executes the drawing calls, tracks position and heading, then writes the SVG. |

---

## 4. Files

| File | Owner | Role |
| --- | --- | --- |
| `ast.hpp` | shared | **Frozen contract.** `FractalDecl`, `Production`, `RuleSet`. |
| `ir.hpp` | shared | **Frozen contract.** `Op`, `TurtleCmd`, `Program`. All six operations are declared from day one. |
| `lexer.hpp` | M1 | Scanner |
| `parser.hpp` | M1 | Recursive-descent parser and AST dump |
| `rewriter.hpp` | M1 | L-system expansion with a blow-up guard |
| `lower.hpp` | M1 | Expanded string → turtle IR |
| `frontend.hpp` | M1 | The whole frontend behind `compileFrontend(source)` |
| `json.hpp` | M1 | Serialises a compiled `.fr` for the stage viewer |
| `fr_frontend.cpp` | M1 | Frontend driver; can print any stage |
| `codegen.hpp` | M2 | Backend interface — deliberately contains **no LLVM headers** |
| `codegen.cpp` | M2 | LLVM IR generation |
| `emit_object.cpp` | M2 | Native object emission |
| `runtime.c` | M2 | C runtime and SVG writer |
| `fractlc.cpp` | M2 | The compiler driver |
| `Makefile`, `build.ps1` | shared | Builds |
| `tests/test_frontend.cpp` | M1 | Frontend test suite (1904 checks) |
| `viewer/` | M1 | Static page for inspecting each compiler stage |

**Design rule:** LLVM headers appear only in `codegen.cpp` and
`emit_object.cpp`. Everything else — the shared headers, the frontend, the
driver — compiles without LLVM. The `Module` is hidden behind a pointer in
`codegen.hpp`.

---

## 5. Each stage, with real output

Using `triangle.fr`, whose `iterations: 0` makes the expansion equal to the axiom.

### Tokens — `fr_frontend triangle.fr --tokens`

```
  line 6   'fractal'
  line 6   identifier    Triangle
  line 6   '{'
  line 7   'axiom'
  line 7   ':'
  line 7   string        "F--F--F"
  line 7   ';'
  …
  line 19  end of file
```

### AST — `--ast`

```
fractal Triangle {
  axiom      : "F--F--F"
  rules      :
      F -> "F"  [w=1]
  angle      : 60
  iterations : 0
  step       : 100
  start      : (100, 300)
  heading    : 0
}
```

### Expansion — `--expand`

Each pass replaces every symbol at once. For `koch.fr`:

| Iterations | Symbols | Expanded string |
| --- | --- | --- |
| 0 | 7 | `F--F--F` |
| 1 | 28 | `F+F--F+F--F+F--F+F--F+F--F+F` |
| 2 | 112 | `F+F--F+F+F+F--F+F--F+F--F+F+F+F--F+F…` |
| 4 | 1792 | (the demo's setting) |

### Turtle IR — `--ir`

```
0: FORWARD 100
1: TURN_RIGHT 60
2: TURN_RIGHT 60
3: FORWARD 100
4: TURN_RIGHT 60
5: TURN_RIGHT 60
6: FORWARD 100
```

Two consecutive 60° right turns make the 120° exterior angle of a triangle.
Turn operations carry only the **magnitude**; the direction is implied by
`TURN_LEFT` / `TURN_RIGHT`, and the backend applies the sign.

### Counts — `--stats`

```
  axiom symbols   : 7
  expanded symbols: 7
  turtle commands : 7
  forward=3 move=0 left=0 right=4 push=0 pop=0
```

### LLVM IR — `fractlc triangle.fr -o triangle.o --svg triangle.svg --emit-ir triangle.ll`

```llvm
; ModuleID = 'triangle'
target triple = "x86_64-pc-linux-gnu"

@svg_path = private unnamed_addr constant [19 x i8] c"build/triangle.svg\00", align 1

declare void @turtle_init(double, double, double, double, double, ptr)
declare void @turtle_forward(double)
declare void @turtle_move(double)
declare void @turtle_turn(double)
declare void @turtle_push()
declare void @turtle_pop()
declare void @turtle_finish()

define void @fractl_main() {
entry:
  call void @turtle_init(double 1.000000e+02, double 6.000000e+01, double 1.000000e+02,
                         double 3.000000e+02, double 0.000000e+00, ptr @svg_path)
  call void @turtle_forward(double 1.000000e+02)
  call void @turtle_turn(double -6.000000e+01)
  call void @turtle_turn(double -6.000000e+01)
  call void @turtle_forward(double 1.000000e+02)
  call void @turtle_turn(double -6.000000e+01)
  call void @turtle_turn(double -6.000000e+01)
  call void @turtle_forward(double 1.000000e+02)
  call void @turtle_finish()
  ret void
}

define i32 @main() {
entry:
  call void @fractl_main()
  ret i32 0
}
```

Every number is a literal constant. Right turns are emitted as negative angles.
`turtle_move`, `turtle_push` and `turtle_pop` are declared but never called, and
`runtime.c` does not even define them yet — an unused declaration costs nothing,
and keeping the full interface from the start means Stage 2 adds no new
interface.

---

## 6. Error reporting

Errors are exceptions carrying an already-formatted `line N: message`, which the
driver prints to standard error before exiting with a non-zero status.

| Input | Message |
| --- | --- |
| `angle: 60` with the `;` missing | `error: line 4: expected ';' after the 'angle' value, found 'iterations'` |
| `F -> "FF"; F -> "F+F";` | `error: line 3: duplicate rule for symbol 'F'` |
| `iterations: 40` on a rule that quadruples | `error: expansion exceeded 67108864 symbols — lower 'iterations'` |

---

## 7. Design decisions worth explaining

**The frozen interface.** `ir.hpp` declares all six turtle operations from day
one, even though Stage 1 produces only three of them. Both halves of the project
were written against that fixed definition, so later stages add cases rather
than changing an interface that other code depends on.

**Two intermediate representations.** The turtle IR is a small, target-agnostic
list of drawing operations — easy to print, test and reason about. LLVM IR is the
second, lower-level representation. Splitting them keeps the grammar work and
the code-generation work independent.

**Arguments as constants.** Angle, step and start position are known while
compiling, so they are emitted as literal constants rather than variables. The
shape of the fractal is fully determined by the compiler; the runtime only
executes calls. This is partial evaluation, the same idea behind C++
`constexpr`.

**The output path is compiled in.** `--svg` is baked into the program as a string
constant. That is why `fractlc` takes the flag but the *linked executable* is
what writes the file.

**Position-independent code.** Objects are emitted as PIC, because the system
compiler links position-independent executables by default and would otherwise
reject the object file.

**Orientation.** Internally the turtle uses mathematical coordinates: heading 0
is +x, and positive turns are counter-clockwise. SVG's y axis points down, so the
y axis is flipped only when the file is written. A left turn therefore appears
counter-clockwise on screen. Both halves of the project agreed on this
explicitly, because a mirrored picture is a convention mismatch, not a bug in the
expansion.

**Real-size output.** One unit of `step` is one pixel. The runtime records the
minimum and maximum of every point, shifts the drawing so it starts inside a
small margin (so negative coordinates never fall off the page), and sizes the
SVG to fit it exactly. It never rescales, so a bigger `step` gives a bigger
picture.

---

## 8. Building and running

Requires LLVM 20 (`llvm-20-dev`), a C++17 compiler and `make`. Verified on
Ubuntu with LLVM 20.1.8.

```bash
make                       # fractlc, runtime.o and the frontend driver
make triangle              # compile, link and run a demo -> build/triangle.svg
make koch
make test                  # frontend test suite
```

Or by hand, which shows each step:

```bash
./fractlc koch.fr -o koch.o --svg koch.svg --emit-ir koch.ll
gcc koch.o runtime.o -lm -o koch     # link with the runtime
./koch                               # -> writes koch.svg
```

On Windows the frontend alone builds with `.\build.ps1 -Test`, no LLVM needed.
`viewer/index.html` shows the source, tokens, AST, turtle IR, LLVM IR and the
rendered drawing for each demo on one page.

---

## 9. Verification

- **Frontend test suite:** 1904 checks, all passing. Covers the lexer, parser,
  error cases, parallel rewriting, lowering, the JSON export, and the command
  counts of both demo files.
- **Backend:** clean rebuild from scratch with `-Wall -Wextra`, no warnings.
  Both demos compile, link, run and produce the expected drawings — a closed
  equilateral triangle and a Koch snowflake with 768 segments.
- **Known limits:** the lexer rejects a file that begins with a UTF-8
  byte-order mark; bracket balance is not yet checked at compile time (Stage 2
  introduces brackets); there is no optimization pass yet (Stage 3).

---

## 10. What the later stages add

**Stage 2 — branching.** `f`, `[` and `]` become pen-up moves and a save/restore
stack, so the turtle can draw a branch and jump back to where it started. This is
what makes plants and trees possible. It touches `lower.hpp` and `runtime.c`; the
shared interface does not change.

**Stage 3 — optimization.** A peephole pass over the generated LLVM IR that folds
consecutive turns into one and deletes pairs that cancel out, with golden-file
tests proving the drawing is byte-for-byte unchanged.
