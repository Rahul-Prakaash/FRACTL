# FRACTL — presenting Stage 3 on the RTX 3050 laptop

## The day before (10 minutes)

1. Copy `dist\FRACTL-Studio` (or unzip `FRACTL-Studio.zip`) to the laptop, anywhere.
2. Double-click **Check FRACTL Setup.bat**. Every line should say `[ ok ]`.
   * If WSL is missing: in an administrator PowerShell run `wsl --install -d Ubuntu`,
     restart, open Ubuntu once to create a user, then run the check again. This is
     the only step that needs internet, and only once.
3. Put the browser on the RTX GPU: **Settings → System → Display → Graphics →**
   add **Microsoft Edge** → **High performance**. (The launcher opens Edge as an app
   window.)
4. Double-click **Launch FRACTL Studio.bat**. The studio opens and builds the Meadow.
   The chip in the top-right of the 3D view should name the NVIDIA GPU.
5. Plug the laptop in (battery saver throttles the GPU) and close other heavy apps.

Nothing in the bundle needs the internet: `fractlc` is one static binary with LLVM
inside, the server is Python's standard library, and the page has no external files.

## On the day

Open the studio before you start talking, so WSL is already warm. In the settings
button (sliders icon) set **Duration** to 60–90 s for the main demo; the build itself
takes well under a second, so all of that time is the model visibly being assembled.

## Demo script (≈ 6 minutes)

1. **Meadow (3D), press Build.** Point at the pipeline as it fills in: the grammar was
   parsed, 1,301 instances were placed, LLVM generated 8 functions, the ORC JIT
   compiled them in memory, and the compiled code wrote about 40,000 segments — all
   in well under a second. Then let it grow: grass sprouts across the field while
   the tree climbs; the light is the turtle.
2. **Open "LLVM IR".** Show `Tree.A(i32 %d)` — one function per rule symbol, the
   `fractl_rand` branch for the weighted choice, and `fractl_main` looping over the
   `@instances` table. *"The IR is the size of the grammar, not of the picture."*
3. **Settings → Unrolled, Build.** Same meadow, but now 200,000 lines of IR and a
   noticeably longer compile. Switch back to Recursive.
4. **Change `seed: 42` to `seed: 7` in the Tree, Build.** A different tree from the
   same grammar — stochastic rules, but every build with seed 7 is identical.
5. **Settings → Emission order, scrub the timeline.** This is exactly the order the
   compiled program drew: trunk first, then each branch depth-first, with the leaves
   on each limb before it returns ( `[` … `]` ).
6. **Toggle to 2D, Fractal plant.** Stage 2's plant, now compiled to 5 functions and
   39 instructions instead of 21,063 calls; the colour runs from teal (first stroke)
   to amber (last). Optional: **Stages ↗** opens the Stage 1–2 viewer.
7. **Make a mistake on purpose** (delete a `]`), Build: the line turns red with
   `line N: unbalanced brackets …`, and the pipeline stops at the parser.
8. **Export** the 3D model as `.obj` if someone asks whether it is "real geometry".
9. **Stages tab** (Stage 1 & 2 features, live): tokens, AST, expanded string, turtle
   IR and counts for the current grammar; drag the iteration slider; LLVM IR **side by
   side** shows recursive vs unrolled code for the same grammar.
10. **Form** (editor switch): the Stage 1 web form — type an axiom and rules, Build.
    **Build log** shows the exact `fractlc` command and its output.

## If something goes wrong

| Symptom | Fix |
| --- | --- |
| The launcher window says WSL is not installed | Run the setup check; use your own laptop as plan B |
| Page loads but 3D says WebGL2 unavailable | Use Edge or Chrome; check the GPU setting above. 2D mode still works |
| Build is slow the very first time | WSL was cold — build once more; later builds take < 1 s |
| Port 8732 is busy | Close other studio windows, or run `python3 studio.py 8740` in Ubuntu and open `http://127.0.0.1:8740` |

## Key numbers to quote

* Meadow: 2 grammars, 1,301 instances, 39,588 segments; recursive IR 531 lines vs
  201,463 unrolled; object 91 KB vs 11.9 MB; build round trip ≈ 0.14 s; 60 fps on
  integrated Intel graphics with shadows and wind.
* Stage 2 plant: 70 lines of IR and a 2.7 KB object (was 21,086 lines and 1 MB).
* Tests: 79,580 frontend checks, 36 runtime, 8 pipeline, 59 mode-equivalence,
  16 golden files — all passing; Stage 1 and 2 drawings byte-identical.
