# FRACTL

A compiler from L-system grammars to native code via LLVM.

A `.fr` file describes a fractal or a plant: an axiom, rewriting rules (optionally
weighted and seeded), and turtle-graphics parameters. `fractlc` lexes, parses and
checks it, lowers it to a turtle IR, generates LLVM IR (one function per rule
symbol), and either writes a native object file or runs the program in memory with
LLVM's ORC JIT. The compiled program draws the result as an SVG (2D) or a `.f3d`
segment stream (3D). **FRACTL Studio** is a local browser front end that compiles
what you type and grows the model live.

```
fractal Koch {
    axiom: "F--F--F";
    rules { F -> "F+F--F+F"; }
    angle: 60;  iterations: 4;  step: 4;
    start: (150, 350);  heading: 0;
}
```

| Document | What it covers |
| --- | --- |
| [STAGE1.md](STAGE1.md), [STAGE2.md](STAGE2.md), [STAGE3.md](STAGE3.md) | The language, the compiler and the results, stage by stage |
| [FRONTEND.md](FRONTEND.md) | Lexer, parser, rewriter and lowering; the frontend/backend interface |
| [PRESENTATION.md](PRESENTATION.md) | Demo script and presentation-laptop checklist |
| [docs/report/FRACTL_Project_Report.pdf](docs/report/FRACTL_Project_Report.pdf) | Lab project report, with screenshots of every preset |
| [docs/](docs/) | Per-member write-ups (PDF, with their HTML sources) |

## Requirements

The compiler and the Studio server run on **Linux**. On Windows, use **WSL** with
Ubuntu; the Windows launchers call into WSL for you.

Tested with Ubuntu 26.04, GCC 15.2, LLVM 20.1.8, Python 3.14 and GNU Make 4.4.

```bash
sudo apt update
sudo apt install build-essential llvm-20-dev zlib1g-dev libxml2-dev python3
```

`fractlc` links LLVM statically, so `llvm-20-dev` must provide the static libraries
(Ubuntu's package does). The Makefile finds `llvm-config-20`, or `llvm-config` if it
reports version 20; set `LLVM_CONFIG=/path/to/llvm-config` to override. On Ubuntu
releases without LLVM 20 in the archive, install it from <https://apt.llvm.org>.

## Build and test

From the repository root, in a Linux or WSL shell:

```bash
make            # builds fractlc, runtime.o and fr_frontend
make test       # frontend, runtime, pipeline, golden-file and mode-equivalence tests
```

Every suite should print its checks with `0 failed`, and the golden step should match
all 16 reference files in `tests/golden/`. `make clean` removes everything the build
created.

## Run

**Compile one program ahead of time** (Stage 1 path: object file, link, run):

```bash
./fractlc koch.fr -o build/koch.o --svg build/koch.svg --emit-ir build/koch.ll
cc build/koch.o runtime.o -lm -o build/koch && ./build/koch   # writes build/koch.svg
```

**Or JIT it in memory** (no object file, no linker):

```bash
./fractlc meadow.fr --jit --out build/meadow.f3d --timings
```

`make koch` (or any demo name) does the ahead-of-time steps for one demo.
`./fractlc` without arguments lists all the options (`--unroll`, `--opt`,
`--ir-only`, ...). `./fr_frontend koch.fr --all` prints every frontend stage: tokens,
AST, expanded string and turtle IR.

**FRACTL Studio** (needs `make` first):

- Windows: double-click **`Launch FRACTL Studio.bat`**. It starts the server in WSL
  and opens Edge at <http://127.0.0.1:8732>.
- Linux: `python3 studio.py` and open <http://127.0.0.1:8732>.

The Studio needs a browser with WebGL2 for 3D (Edge or Chrome); 2D works everywhere.
**`Launch FRACTL Classic UI.bat`** (or `python3 web_ui.py 8733`) starts the Stage 1
web form.

**Stage viewer:** open `viewer/index.html` directly. It reads the precomputed
artifacts in `viewer/data/`; see [viewer/README.md](viewer/README.md) to regenerate
them.

**Frontend only, on Windows without WSL:** `.\build.ps1 -Test` builds
`fr_frontend.exe` and runs the frontend tests with MinGW `g++`.

## Repository layout

| Path | Contents |
| --- | --- |
| `lexer.hpp`, `parser.hpp`, `ast.hpp` | Scanner, LL(1) recursive-descent parser, AST |
| `rewriter.hpp`, `rng.hpp`, `scene.hpp`, `lower.hpp`, `ir.hpp` | L-system expansion, seeded RNG, scene placement, turtle IR |
| `frontend.hpp`, `fr_frontend.cpp`, `json.hpp` | Frontend entry point and its debug driver |
| `codegen.cpp`, `emit_object.cpp`, `jit.cpp`, `fractlc.cpp` | LLVM code generation, object emission, ORC JIT, compiler driver |
| `runtime.c`, `runtime.h` | Turtle runtime linked into every compiled program (SVG and `.f3d` writers) |
| `*.fr`, `studio/presets/*.fr` | Demo programs and Studio presets |
| `tests/` | Test suites and golden outputs |
| `studio.py`, `studio/` | FRACTL Studio (server and single-page app) |
| `web_ui.py`, `viewer/` | Stage 1 web form; static stage viewer |
| `tools/` | Offline presentation bundle (`make bundle`), Studio screenshot script |
| `docs/` | Write-ups and the project report, with the script that builds the report PDF |

## Regenerating the report

```bash
node tools/screenshots/shoot_studio.mjs docs/report/shots tuft:3d field:3d shrub:3d tree3d:3d meadow:3d koch:2d ...
python docs/report/build_report.py   # needs reportlab; uses Calibri/Consolas from C:/Windows/Fonts (FRACTL_FONTS)
```

The screenshot script drives Edge through the DevTools protocol while the Studio is
running, and records the timings shown in each shot in `meta.json`. The report uses
JPEG copies of the screenshots, in `docs/report/img/`.
