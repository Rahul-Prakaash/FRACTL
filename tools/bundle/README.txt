FRACTL Studio - presentation bundle
===================================

Double-click "Launch FRACTL Studio.bat". A window opens with the studio; pick a
preset or edit the grammar and press Build (Ctrl+Enter). Toggle 2D / 3D at the top.

Every Build runs the real compiler on this laptop: the grammar is parsed, turned
into LLVM IR (one function per rule symbol), compiled to machine code by LLVM's ORC
JIT and executed. The compiled program emits the model segment by segment and the
viewer grows it on screen in that exact order. Nothing is pre-rendered.

Needs: Windows 10/11 with WSL (any Ubuntu) and python3 - both are standard.
No internet, no LLVM install, no packages: fractlc is a single static binary.

Before presenting, run "Check FRACTL Setup.bat" once. For the 3D view on the RTX
GPU, set Microsoft Edge to "High performance" in Settings > System > Display >
Graphics. The GPU in use is shown in the top-right corner of the 3D view.

Also inside the studio: the Stages tab (tokens, AST, turtle IR, expanded string,
counts, iteration slider, LLVM IR side by side - the Stage 1/2 stage viewer, live),
a Form switch in the editor (the Stage 1 web form), and a Build log tab.
"Launch FRACTL Classic UI.bat" still opens the original Stage 1 web form (port 8733).

Controls: drag = orbit, right-drag = pan, wheel = zoom, double-click = reset,
Space = play/pause, settings button = codegen mode, JIT/AOT, --opt, growth speed.
