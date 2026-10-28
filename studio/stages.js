(function () {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const A = window.FRACTL;
  const esc = A.esc, fmt = A.fmt;

  async function post(url, body) {
    try {
      const r = await fetch(url, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
      return await r.json();
    } catch (e) {
      return { ok: false, error: "The studio server is not reachable." };
    }
  }

  let view = "render";
  function setView(v) {
    view = v;
    for (const b of document.querySelectorAll("#views button")) b.classList.toggle("on", b.dataset.view === v);
    document.body.classList.toggle("view-stages", v === "stages");
    document.body.classList.toggle("view-log", v === "log");
    $("stagesView").hidden = v !== "stages";
    $("logView").hidden = v !== "log";
    if (v === "stages") refreshStages();
    if (v === "log") renderLog();
  }
  for (const b of document.querySelectorAll("#views button")) b.onclick = () => setView(b.dataset.view);
  window.FRACTL_setView = setView;

  let inspected = null, declared = null, iterTimer = null;
  let tokAll = false, irShown = 300, expFull = false, llMode = "recursive";
  const llCache = new Map();

  async function refreshStages(opts = {}) {
    const source = A.source();
    const fractal = $("stFractal").value || undefined;
    const iterations = opts.iterations;
    const key = JSON.stringify([source, fractal, iterations]);
    if (inspected && inspected.key === key) return;
    if (!inspected || inspected.source !== source || inspected.fractal !== fractal) declared = null;
    $("stInfo").textContent = "running fr_frontend…";
    const res = await post("/api/inspect", { source, fractal, iterations });
    if (!res.ok) {
      $("stError").hidden = false;
      $("stError").textContent = res.error;
      $("stInfo").textContent = "";
      return;
    }
    $("stError").hidden = true;
    inspected = Object.assign({ key, source, fractal }, res);
    const d = res.data;

    const sel = $("stFractal");
    if ([...sel.options].map((o) => o.value).join() !== res.fractals.join()) {
      sel.innerHTML = res.fractals.map((n) => `<option>${esc(n)}</option>`).join("");
      if (fractal && res.fractals.includes(fractal)) sel.value = fractal;
    }
    $("stFractalWrap").hidden = res.fractals.length < 2;
    if (declared === null) declared = d.ast.iterations;
    const it = $("stIter");
    it.max = String(Math.max(declared, 0));
    it.value = String(d.ast.iterations);
    $("stIterOut").textContent = `n = ${d.ast.iterations}` + (d.ast.iterations !== declared ? ` (file: ${declared})` : "");
    $("stInfo").textContent = `fr_frontend · ${res.ms.toFixed(0)} ms`;
    renderStages(d, res);
    if (!opts.keepLl) loadLl();
  }
  $("stFractal").onchange = () => { tokAll = false; irShown = 300; refreshStages(); };
  $("stIter").oninput = () => {
    const k = +$("stIter").value;
    $("stIterOut").textContent = `n = ${k}`;
    clearTimeout(iterTimer);
    iterTimer = setTimeout(() => refreshStages({ iterations: k, keepLl: true }), 120);
  };

  function renderStages(d, res) {
    const src = d.source.replace(/\n$/, "").split("\n");
    $("stSource").innerHTML = src.map((l, i) => `<span class="ln">${i + 1}</span>${esc(l)}`).join("\n");
    $("stSrcInfo").textContent = `${src.length} lines`;

    const a = d.ast;
    const rows = [
      ["name", esc(a.name)], ["dimensions", a.dimensions], ["axiom", `"${esc(a.axiom)}"`],
      ["angle", a.angle + "°"], ["iterations", a.iterations], ["step", a.step],
      ["start", `(${a.startX}, ${a.startY})`], ["heading", a.heading + "°"],
    ];
    if (a.seed) rows.push(["seed", a.seed]);
    if (a.dimensions === 3) rows.push(["width", a.width], ["taper", a.taper]);
    if (a.colors && a.colors.length)
      rows.push(["colours", a.colors.map((c) => `<span class="swatch" style="background:${c}"></span>${c}`).join(" ")]);
    let rules = "";
    for (const r of a.rules)
      for (const p of r.productions)
        rules += `<tr><td class="op">${esc(r.symbol)}</td><td>→</td><td>${esc(p.text)}</td><td class="num">${p.weight}</td></tr>`;
    $("stAst").innerHTML = `<table class="kv">${rows.map(([k, v]) => `<tr><td>${k}</td><td>${v}</td></tr>`).join("")}</table>` +
      `<table class="grid" style="margin-top:10px"><tr><th>symbol</th><th></th><th>production</th><th>weight</th></tr>${rules}</table>`;
    $("stAstInfo").textContent = `FractalDecl · ${a.rules.length} rule${a.rules.length === 1 ? "" : "s"}`;

    const toks = tokAll ? d.tokens : d.tokens.slice(0, 80);
    $("stTokens").innerHTML = `<table class="grid"><tr><th>line</th><th>kind</th><th>text</th><th>value</th></tr>` +
      toks.map((t) => `<tr><td class="num">${t.line}</td><td class="op">${esc(t.kind)}</td><td>${esc(t.text)}</td><td class="num">${t.num !== undefined ? t.num : ""}</td></tr>`).join("") + "</table>";
    $("stTokInfo").textContent = `${fmt(d.tokens.length)} tokens`;
    $("stTokMore").hidden = d.tokens.length <= 80;
    $("stTokMore").textContent = tokAll ? "show fewer" : "show all";

    const prog = d.program.slice(0, irShown);
    $("stTurtle").innerHTML = `<table class="grid"><tr><th>#</th><th>op</th><th>arg</th></tr>` +
      prog.map((c, i) => `<tr><td class="num">${i}</td><td class="op">${c.op}</td><td class="num">${c.op === "PUSH" || c.op === "POP" || c.op === "NEXT_COLOR" ? "" : c.arg}</td></tr>`).join("") + "</table>";
    $("stIrInfo").textContent = `${fmt(d.programLength)} commands` + (d.programTruncated ? ` (first ${fmt(d.program.length)} in export)` : "");
    $("stIrMore").hidden = d.program.length <= irShown;

    const full = expFull && d.expanded.full ? d.expanded.full : d.expanded.preview;
    $("stExpanded").textContent = full + (full.length < d.expanded.fullLength ? " …" : "");
    $("stExpInfo").textContent = `${fmt(d.expanded.fullLength)} symbols`;
    $("stExpMore").hidden = !d.expanded.full || d.expanded.full.length <= d.expanded.preview.length;
    $("stExpMore").textContent = expFull ? "show preview" : "show full";

    const sym = Object.entries(d.stats.symbolCounts).sort((x, y) => y[1] - x[1]);
    const ops = Object.entries(d.stats.opCounts).filter(([, v]) => v > 0).sort((x, y) => y[1] - x[1]);
    const bars = (title, list) => {
      const max = Math.max(1, ...list.map(([, v]) => v));
      return `<h4>${title}</h4>` + list.map(([k, v]) =>
        `<span>${esc(k)}</span><span class="b" style="width:${(100 * v / max).toFixed(1)}%"></span><span class="num">${fmt(v)}</span>`).join("");
    };
    $("stCounts").innerHTML = `<div class="bars">${bars("symbols in expanded string", sym)}${bars("turtle ops", ops)}</div>` +
      `<p class="muted" style="margin:10px 0 0">max branch depth: ${d.stats.maxDepth}</p>`;
    $("stCountInfo").textContent = `n = ${a.iterations}`;

    const out = a.dimensions === 3 ? "out.f3d" : "out.svg";
    $("stCmds").innerHTML = `<div class="cmds">
      <div><small>what this panel ran</small><code>${esc(res.cmd)}</code></div>
      <div><small>every frontend stage as text (Stage 1)</small><code>./fr_frontend input.fr --tokens --ast --expand --ir --stats</code></div>
      <div><small>native object + link + run (Stages 1–2)</small><code>./fractlc input.fr -o out.o --out ${out} --emit-ir out.ll\ncc out.o runtime.o -lm -o out && ./out</code></div>
      <div><small>in-memory JIT, the studio's default (Stage 3)</small><code>./fractlc input.fr --jit --out ${out} --timings</code></div>
    </div>`;
  }
  $("stTokMore").onclick = () => { tokAll = !tokAll; inspected && renderStages(inspected.data, inspected); };
  $("stIrMore").onclick = () => { irShown += 1000; inspected && renderStages(inspected.data, inspected); };
  $("stExpMore").onclick = () => { expFull = !expFull; inspected && renderStages(inspected.data, inspected); };

  function hlIr(text, limit) {
    const lines = text.split("\n");
    const shown = lines.slice(0, limit).map((l) => {
      const e = esc(l);
      if (/^define/.test(l)) return `<span class="fn">${e}</span>`;
      if (/^\s*;/.test(l) || /^(source_filename|target)/.test(l)) return `<span class="cmt">${e}</span>`;
      return e.replace(/(call (?:void|double) @[\w.]+)/, '<span class="call">$1</span>');
    }).join("\n");
    return shown + (lines.length > limit ? `\n<span class="cmt">; … ${fmt(lines.length - limit)} more lines</span>` : "");
  }
  async function ir(mode) {
    const key = mode + "\n" + A.source();
    if (!llCache.has(key)) llCache.set(key, post("/api/ir", { source: A.source(), mode }));
    return llCache.get(key);
  }
  async function loadLl() {
    const wrap = $("stLl");
    const modes = llMode === "side" ? ["recursive", "unroll"] : [llMode];
    wrap.className = "ll-wrap" + (llMode === "side" ? " side" : "");
    wrap.innerHTML = modes.map(() => `<pre class="muted">generating…</pre>`).join("");
    const results = await Promise.all(modes.map(ir));
    const label = { recursive: "recursive (default)", unroll: "unrolled (Stage 2)", opt: "recursive + --opt" };
    wrap.innerHTML = results.map((r, i) => r.ok
      ? `<pre><span class="ll-head">${label[modes[i]]} · ${fmt(r.lines)} lines · ${fmt(r.timings.instructions || 0)} instructions · ${fmt(r.timings.functions || 0)} functions${r.timings.folds ? ` · ${r.timings.folds} folds` : ""}</span>${hlIr(r.ir, 1500)}</pre>`
      : `<pre class="err">${esc(r.error)}</pre>`).join("");
    const ok = results.filter((r) => r.ok);
    $("stLlInfo").textContent = ok.length ? `fractlc --ir-only · ${ok.map((r) => r.ms.toFixed(0) + " ms").join(" / ")}` : "";
  }
  for (const b of document.querySelectorAll("#stLlMode button")) {
    b.onclick = () => {
      llMode = b.dataset.ll;
      for (const x of document.querySelectorAll("#stLlMode button")) x.classList.toggle("on", x === b);
      loadLl();
    };
  }

  let lastBuild = null;
  function renderLog() {
    const res = lastBuild || A.result();
    const body = $("logBody");
    if (!res) return;
    const steps = res.steps || [];
    let html = `<details class="log-src"><summary>Generated .fr source (${fmt((res.source || "").split("\n").length)} lines, written to input.fr)</summary><pre>${esc(res.source || "")}</pre></details>`;
    for (const s of steps) {
      html += `<div class="log-step"><div class="cmd"><span>$ ${esc(s.cmd)}</span><span class="t">${s.ms.toFixed(0)} ms</span></div>` +
        (s.stdout ? `<pre>${esc(s.stdout.trim())}</pre>` : "") + (s.stderr ? `<pre class="err">${esc(s.stderr.trim())}</pre>` : "") + "</div>";
    }
    if (!res.ok && !steps.length) html += `<div class="log-step"><pre class="err">${esc(res.error || "build failed")}</pre></div>`;
    body.innerHTML = html;
    $("logInfo").textContent = res.ok
      ? `built OK · ${res.backend === "aot" ? "object + link" : "ORC JIT"} · round trip ${(res.wall || res.server_ms || 0).toFixed(0)} ms`
      : "build failed";
  }

  window.FRACTL_onBuilt = (res) => {
    lastBuild = res;
    if (view === "log") renderLog();
    if (view === "stages") { inspected = null; refreshStages(); }
  };

  const form = $("formView");
  let editMode = "code", fromForm = false;
  const F = (n) => form.elements[n];

  function setEditMode(m) {
    editMode = m;
    for (const b of document.querySelectorAll("#editMode button")) b.classList.toggle("on", b.dataset.mode === m);
    form.hidden = m !== "form";
    $("editor").hidden = m === "form";
    if (m === "form") fillForm();
  }
  for (const b of document.querySelectorAll("#editMode button")) b.onclick = () => setEditMode(b.dataset.mode);
  window.FRACTL_setEditMode = setEditMode;

  const unq = (s) => s.replace(/\\(["\\])/g, "$1");
  const q = (s) => '"' + s.replace(/\\/g, "\\\\").replace(/"/g, '\\"') + '"';
  const num = (src, key) => { const m = new RegExp("\\b" + key + "\\s*:\\s*(-?[\\d.]+(?:[eE][-+]?\\d+)?)\\s*;").exec(src); return m ? m[1] : ""; };

  function parseSource(src) {
    const names = src.match(/^\s*fractal\s+\w+/gm) || [];
    if (names.length !== 1 || /^\s*scene\s+\w+/m.test(src)) return null;
    const out = {
      name: (/fractal\s+(\w+)/.exec(src) || [])[1] || "MyFractal",
      axiom: unq((/axiom\s*:\s*"((?:[^"\\]|\\.)*)"/.exec(src) || [, ""])[1]),
      seed: num(src, "seed"), angle: num(src, "angle"), iterations: num(src, "iterations"),
      step: num(src, "step"), heading: num(src, "heading"), width: num(src, "width"), taper: num(src, "taper"),
      start: (/start\s*:\s*\(([^)]*)\)/.exec(src) || [])[1] || "",
      colors: ((/colors\s*:\s*\(([^)]*)\)/.exec(src) || [, ""])[1].match(/#[0-9a-fA-F]{6}/g) || []).join(", "),
      rules: [],
    };
    const block = /rules\s*:?\s*\{([\s\S]*?)\}\s*;?/.exec(src);
    if (block) {
      const stmt = /(\S)\s*->\s*((?:\s*\|?\s*"(?:[^"\\]|\\.)*"\s*(?:\[\s*[-\d.eE+]+\s*\])?)+)\s*;/g;
      let m;
      while ((m = stmt.exec(block[1]))) {
        const alts = [...m[2].matchAll(/"((?:[^"\\]|\\.)*)"\s*(?:\[\s*([-\d.eE+]+)\s*\])?/g)];
        const weighted = alts.length > 1 && alts.some((x) => x[2] !== undefined);
        for (const x of alts) out.rules.push(`${m[1]}=${unq(x[1])}${weighted ? "@" + (x[2] || "1") : ""}`);
      }
    }
    return out;
  }

  function fillForm() {
    if (fromForm) return;
    const p = parseSource(A.source());
    form.classList.toggle("locked", !p);
    $("formNote").textContent = p ? "" : "This file holds several fractals and a scene — edit it in Code. The form edits one fractal, like the Stage 1 web form.";
    if (!p) return;
    for (const k of ["name", "axiom", "seed", "angle", "iterations", "step", "heading", "width", "taper", "colors"]) F(k).value = p[k];
    F("rules").value = p.rules.join("\n");
    form.dataset.start = p.start;
  }

  function generate() {
    const v = (k) => F(k).value.trim();
    const is3d = A.dims() === "3d";
    const groups = new Map();
    for (const line of F("rules").value.split("\n")) {
      const t = line.trim();
      if (!t || !t.includes("=")) continue;
      const sym = t.slice(0, t.indexOf("=")).trim();
      if (sym.length !== 1) continue;
      let rhs = t.slice(t.indexOf("=") + 1).trim(), w = null;
      const at = /@\s*([\d.]+)\s*$/.exec(rhs);
      if (at) { w = at[1]; rhs = rhs.slice(0, at.index).trim(); }
      if (!groups.has(sym)) groups.set(sym, []);
      groups.get(sym).push([rhs, w]);
    }
    const rules = [...groups].map(([s, alts]) => {
      const weighted = alts.length > 1 && alts.some(([, w]) => w !== null);
      return `        ${s} -> ` + alts.map(([r, w]) => q(r) + (weighted ? ` [${w || 1}]` : "")).join("\n           | ") + ";";
    });
    const L = [`fractal ${/^[A-Za-z_]\w*$/.test(v("name")) ? v("name") : "MyFractal"} {`];
    if (is3d) L.push("    dimensions: 3;");
    if (v("seed")) L.push(`    seed: ${v("seed")};`);
    L.push(`    axiom: ${q(v("axiom"))};`, "", "    rules {", ...rules, "    }", "");
    for (const k of ["angle", "iterations", "step"]) if (v(k)) L.push(`    ${k}: ${v(k)};`);
    if (form.dataset.start) L.push(`    start: (${form.dataset.start});`);
    if (v("heading")) L.push(`    heading: ${v("heading")};`);
    if (is3d) {
      if (v("width")) L.push(`    width: ${v("width")};`);
      if (v("taper")) L.push(`    taper: ${v("taper")};`);
      const cols = v("colors").match(/#[0-9a-fA-F]{6}/g);
      if (cols) L.push(`    colors: (${cols.map((c) => `"${c}"`).join(", ")});`);
    }
    L.push("}");
    return L.join("\n") + "\n";
  }

  form.addEventListener("input", () => {
    if (form.classList.contains("locked")) return;
    fromForm = true;
    A.setSource(generate());
    fromForm = false;
  });
  form.addEventListener("submit", (e) => { e.preventDefault(); A.build(); });
  form.addEventListener("keydown", (e) => { if (e.key === "Enter" && (e.ctrlKey || e.metaKey)) { e.preventDefault(); A.build(); } });

  window.FRACTL_onSource = () => {
    if (editMode === "form") fillForm();
    if (view === "stages" && inspected && inspected.source !== A.source()) {
      clearTimeout(iterTimer);
      iterTimer = setTimeout(() => refreshStages(), 400);
    }
  };
})();
