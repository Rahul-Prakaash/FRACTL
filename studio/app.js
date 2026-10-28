(function () {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const fmt = (n) => Number(n).toLocaleString("en-US");
  const ms = (v) => (v == null ? "" : v >= 1000 ? (v / 1000).toFixed(2) + " s" : v >= 10 ? v.toFixed(0) + " ms" : v.toFixed(1) + " ms");

  const S = {
    dims: "3d",
    presets: [],
    preset: { "2d": null, "3d": null },
    source: { "2d": null, "3d": null },
    result: { "2d": null, "3d": null },
    t: { "2d": 0, "3d": 0 },
    playing: false,
    speed: 1,
    duration: 40,
    order: "together",
    building: false,
  };

  let r2 = null, r3 = null, glError = null;
  try { r3 = new Renderer3D($("gl3d")); } catch (e) { glError = e.message; }
  r2 = new Renderer2D($("cv2d"));
  const R = () => (S.dims === "3d" ? r3 : r2);

  const KW = /^(fractal|axiom|rules|angle|iterations|step|start|heading|seed|dimensions|width|taper|colors|scene|place|scatter|at|within|yaw|scale|clear)$/;
  const esc = (s) => s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  const TOKEN = /(\/\*[\s\S]*?(?:\*\/|$)|\/\/[^\n]*|#[^\n]*)|("(?:[^"\\\n]|\\.)*"?)|([A-Za-z_]\w*)|(-?\d*\.?\d+(?:[eE][-+]?\d+)?)|(->|\|)/g;
  function highlight(src) {
    let out = "", last = 0, m;
    TOKEN.lastIndex = 0;
    while ((m = TOKEN.exec(src))) {
      out += esc(src.slice(last, m.index));
      last = TOKEN.lastIndex;
      if (m[1]) out += `<span class="c">${esc(m[1])}</span>`;
      else if (m[2]) {
        let inner = "";
        for (const ch of m[2]) {
          if (ch === "[" || ch === "]") inner += `<span class="b">${ch}</span>`;
          else if ("+-&^|!'\\/".includes(ch)) inner += `<span class="t">${esc(ch)}</span>`;
          else inner += esc(ch);
        }
        out += `<span class="s">${inner}</span>`;
      } else if (m[3]) out += KW.test(m[3]) ? `<span class="k">${m[3]}</span>` : esc(m[3]);
      else if (m[4]) out += `<span class="n">${m[4]}</span>`;
      else out += `<span class="o">${esc(m[5])}</span>`;
    }
    return out + esc(src.slice(last)) + "\n";
  }
  const ta = $("source"), hl = $("highlight"), gutter = $("gutter");
  let errLine = null;
  function refreshEditor() {
    hl.innerHTML = highlight(ta.value);
    const n = ta.value.split("\n").length;
    let g = "";
    for (let i = 1; i <= n; i++) g += (i === errLine ? `<span class="err">${i}</span>` : i) + "\n";
    gutter.innerHTML = g;
    syncScroll();
    if (window.FRACTL_onSource) window.FRACTL_onSource();
  }
  function syncScroll() { hl.scrollTop = ta.scrollTop; hl.scrollLeft = ta.scrollLeft; gutter.scrollTop = ta.scrollTop; }
  ta.addEventListener("input", () => { S.source[S.dims] = ta.value; refreshEditor(); $("stagesBtn").hidden = true; });
  ta.addEventListener("scroll", syncScroll);
  ta.addEventListener("keyup", cursor);
  ta.addEventListener("click", cursor);
  function cursor() {
    const before = ta.value.slice(0, ta.selectionStart).split("\n");
    $("cursorPos").textContent = `Ln ${before.length}, Col ${before[before.length - 1].length + 1}`;
  }
  ta.addEventListener("keydown", (e) => {
    if (e.key === "Tab") {
      e.preventDefault();
      const s = ta.selectionStart;
      ta.setRangeText("    ", s, ta.selectionEnd, "end");
      ta.dispatchEvent(new Event("input"));
    }
    if (e.key === "Enter" && (e.ctrlKey || e.metaKey)) { e.preventDefault(); build(); }
  });

  const STAGES = [
    ["parse", "Lex · parse · check"],
    ["ir", "LLVM IR generation"],
    ["opt", "Optimizer"],
    ["back", "ORC JIT → machine code"],
    ["run", "Run compiled code"],
    ["gpu", "Upload to GPU"],
    ["grow", "Grow"],
  ];
  function pipelineReset(state) {
    const ol = $("pipeline");
    ol.innerHTML = "";
    for (const [id, name] of STAGES) {
      const li = document.createElement("li");
      li.id = "st-" + id;
      li.className = state || "";
      li.innerHTML = `<span class="dot"></span><span><span class="name">${name}</span> <span class="detail"></span></span><span class="ms"></span>` +
        (id === "grow" ? `<div class="bar"><i></i></div>` : "");
      ol.appendChild(li);
    }
    setStage("gpu", null, S.dims === "3d" ? "Upload to GPU" : "Prepare strokes");
  }
  function setStage(id, cls, name, detail, time) {
    const li = $("st-" + id);
    if (!li) return;
    if (cls !== null && cls !== undefined) li.className = cls;
    if (name) li.querySelector(".name").textContent = name;
    if (detail !== undefined) li.querySelector(".detail").textContent = detail || "";
    if (time !== undefined) li.querySelector(".ms").textContent = time;
  }

  async function loadPresets() {
    const res = await fetch("/api/presets");
    S.presets = await res.json();
    S.preset["3d"] = S.presets.find((p) => p.mode === "3d");
    S.preset["2d"] = S.presets.find((p) => p.mode === "2d");
  }
  function fillPresets() {
    const sel = $("preset");
    sel.innerHTML = "";
    for (const p of S.presets.filter((p) => p.mode === S.dims)) {
      const o = document.createElement("option");
      o.value = p.id; o.textContent = p.title;
      sel.appendChild(o);
    }
    if (S.preset[S.dims]) sel.value = S.preset[S.dims].id;
  }
  function showPresetInfo() {
    const p = S.preset[S.dims];
    $("stagesBtn").hidden = !(S.dims === "2d" && p && p.viewer && ta.value === p.source);
    $("presetTitle").textContent = p ? p.title : "Custom";
    $("presetAbout").textContent = p ? p.about : "";
    $("fileName").textContent = p ? p.file.split("/").pop() : "custom.fr";
  }
  function setDims(d, opts = {}) {
    if (S.dims === d && !opts.force) return;
    if (S.dims !== d) { S.source[S.dims] = ta.value; S.t[S.dims] = (R() && R().time) || 0; }
    S.dims = d;
    document.body.classList.toggle("is3d", d === "3d");
    document.querySelector(".dims").classList.toggle("is3d", d === "3d");
    $("tab2d").setAttribute("aria-selected", String(d === "2d"));
    $("tab3d").setAttribute("aria-selected", String(d === "3d"));
    $("gl3d").classList.toggle("off", d !== "3d");
    $("cv2d").classList.toggle("off", d !== "2d");
    fillPresets();
    if (S.source[d] == null) S.source[d] = S.preset[d] ? S.preset[d].source : "";
    ta.value = S.source[d];
    errLine = null;
    $("errorbar").hidden = true;
    refreshEditor();
    showPresetInfo();
    const res = S.result[d];
    $("empty").hidden = !!res;
    if (res) { renderResultInfo(res, true); if (R()) R().setTime(S.t[d]); }
    else { pipelineReset(); $("chips").innerHTML = ""; setStatus(d === "3d" && glError ? "WebGL2 unavailable: " + glError : "Ready - press Build"); }
    if (d === "3d" && glError) setStatus("WebGL2 unavailable: " + glError);
    S.playing = false;
    playUI();
  }
  $("tab2d").onclick = () => setDims("2d");
  $("tab3d").onclick = () => setDims("3d");
  $("preset").onchange = () => {
    const p = S.presets.find((x) => x.id === $("preset").value);
    S.preset[S.dims] = p;
    S.source[S.dims] = p.source;
    ta.value = p.source;
    errLine = null;
    refreshEditor();
    showPresetInfo();
    build();
  };

  const pop = $("settings");
  $("settingsBtn").onclick = (e) => { e.stopPropagation(); pop.hidden = !pop.hidden; };
  document.addEventListener("click", (e) => { if (!pop.hidden && !pop.contains(e.target) && e.target !== $("settingsBtn")) pop.hidden = true; });
  const opt = (name) => document.querySelector(`input[name="${name}"]:checked`).value;
  $("duration").oninput = () => {
    S.duration = +$("duration").value;
    $("durationOut").textContent = S.duration + " s";
    retimeAll();
  };
  for (const r of document.querySelectorAll('input[name="order"]')) r.onchange = () => { S.order = opt("order"); retimeAll(); };
  $("autoRotate").onchange = () => { if (r3) r3.autoRotate = $("autoRotate").checked; };
  $("quality").onchange = () => { if (r3) r3.quality = $("quality").checked; };
  function retimeAll() {
    for (const [d, r] of [["2d", r2], ["3d", r3]]) {
      if (!r || !S.result[d]) continue;
      const f = r.end ? r.time / r.end : 1;
      r.retime(S.order, S.duration);
      r.setTime(f * r.end);
    }
  }

  async function build() {
    if (S.building) return;
    S.building = true;
    $("buildBtn").disabled = true;
    $("errorbar").hidden = true;
    errLine = null;
    refreshEditor();
    pipelineReset();
    const backend = opt("backend"), unroll = opt("codegen") === "unroll", optimize = $("optFlag").checked;
    setStage("back", null, backend === "jit" ? "ORC JIT → machine code" : "Object file + link");
    for (const id of ["parse", "ir", "opt", "back", "run"]) setStage(id, "run");
    setStatus(`Compiling with LLVM (${unroll ? "unrolled" : "recursive"}, ${backend === "jit" ? "JIT" : "AOT"})…`);
    const t0 = performance.now();
    let res;
    try {
      const r = await fetch("/api/compile", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ source: ta.value, backend, unroll, opt: optimize }),
      });
      res = await r.json();
    } catch (e) {
      res = { ok: false, offline: true, error: "The studio server is not reachable. Is studio.py running?" };
    }
    const wall = performance.now() - t0;
    S.building = false;
    $("buildBtn").disabled = false;
    res.source = ta.value;
    res.wall = wall;
    if (window.FRACTL_onBuilt) window.FRACTL_onBuilt(res);
    if (!res.ok) return showError(res);

    const want = res.dims === 3 ? "3d" : "2d";
    if (want !== S.dims) {
      S.source[want] = ta.value;
      setDims(want);
    }
    if (want === "3d" && !r3) return showError({ error: "This browser has no WebGL2, so 3D output cannot be shown." });
    res.wall = wall;
    S.result[S.dims] = res;
    $("empty").hidden = true;
    const info = R().load(res.output, S.order, S.duration);
    res.info = info;
    res.gpuMs = r3 && S.dims === "3d" ? r3.uploadMs : 0;
    renderResultInfo(res, false);
    R().setTime(0);
    S.playing = true;
    if (S.speed === 0) R().setTime(R().end);
    playUI();
  }

  function showError(res) {
    pipelineReset();
    const msg = res.error || "build failed";
    if (res.offline) {
      for (const id of ["parse", "ir", "opt", "back", "run", "gpu", "grow"]) setStage(id, "skip");
      $("errorbar").hidden = false;
      $("errorbar").textContent = "The studio server is not running, so nothing was compiled.\n" +
        "Start it with \"Launch FRACTL Studio.bat\" (or: python3 studio.py), then press Build again.";
      setStatus("Studio server not running - start it and Build again");
      return;
    }
    const parseFail = /line \d+:|expected|unbalanced|duplicate|must be|unknown|error:/.test(msg);
    setStage(parseFail ? "parse" : "ir", "fail", null, "", "");
    for (const id of ["ir", "opt", "back", "run", "gpu", "grow"]) if (!(id === "ir" && !parseFail)) setStage(id, "skip");
    $("errorbar").hidden = false;
    $("errorbar").textContent = msg.replace(/^.*?input\.fr: /gm, "");
    errLine = res.line || null;
    refreshEditor();
    if (errLine) {
      const lh = parseFloat(getComputedStyle(ta).lineHeight) || 20;
      ta.scrollTop = Math.max(0, (errLine - 4) * lh);
    }
    setStatus("Compile error - fix the source and Build again");
  }

  function renderResultInfo(res, restoring) {
    const T = res.timings || {};
    const jit = res.backend === "jit";
    const unroll = T.mode === "unroll";
    const info = res.info || {};
    const stage = (id, detail, time, i) => {
      const go = () => setStage(id, "done", null, detail, time);
      restoring ? go() : setTimeout(go, 70 * i);
    };
    stage("parse", T.instances ? `${fmt(T.instances)} instance${T.instances > 1 ? "s" : ""} placed` : "", ms(T.parse_ms), 0);
    stage("ir", `${unroll ? "unrolled" : "recursive"} · ${fmt(T.functions)} fn · ${fmt(T.instructions)} instr`, ms(T.codegen_ms), 1);
    if (T.folds || $("optFlag").checked) stage("opt", `folded ${fmt(T.folds || 0)} rotations`, ms(T.opt_ms), 2);
    else setTimeout(() => setStage("opt", "skip", null, "off (--opt)", ""), restoring ? 0 : 140);
    setStage("back", null, jit ? "ORC JIT → machine code" : "Object file + link");
    stage("back", jit ? "in memory, no linker" : ".o + cc", ms(jit ? T.jit_ms : (T.emit_ms || 0) + (T.link_ms || 0)), 3);
    const segs = S.dims === "3d" ? `${fmt(info.tubes)} tubes · ${fmt(info.blades)} blades` : `${fmt(info.segments)} segments`;
    stage("run", segs, ms(T.run_ms), 4);
    stage("gpu", S.dims === "3d" ? `${fmt(info.total)} instanced segments` : `${fmt(info.strokes)} strokes`,
      S.dims === "3d" ? ms(res.gpuMs) : "", 5);
    const orderName = S.dims === "2d" || S.order === "emission" ? "emission order" : "together";
    setTimeout(() => setStage("grow", "run", null, `${orderName} · ${S.duration} s`, ""), restoring ? 0 : 420);
    $("totalTime").textContent = `round trip ${ms(res.wall || res.server_ms)}`;

    const chips = [];
    if (S.dims === "3d") {
      chips.push(["instances", fmt(info.instances)], ["tubes", fmt(info.tubes)], ["blades", fmt(info.blades)]);
    } else {
      chips.push(["segments", fmt(info.segments)], ["strokes", fmt(info.strokes)]);
    }
    chips.push(["LLVM fns", fmt(T.functions)], ["IR instr", fmt(T.instructions)]);
    if (S.dims === "3d" && r3 && r3.gpu) chips.push(["GPU", gpuName(r3.gpu)]);
    $("chips").innerHTML = chips.map(([k, v]) => `<span class="chip">${k} <b>${v}</b></span>`).join("");
  }

  function playUI() { $("playBtn").classList.toggle("playing", S.playing); if (r3) r3.playing = S.playing && S.dims === "3d"; }
  $("playBtn").onclick = () => {
    const r = R();
    if (!r || !S.result[S.dims]) return build();
    if (r.time >= r.end) r.setTime(0);
    S.playing = !S.playing;
    playUI();
  };
  $("restartBtn").onclick = () => { const r = R(); if (!r || !S.result[S.dims]) return; r.setTime(0); S.playing = true; playUI(); };
  for (const b of document.querySelectorAll(".speed button")) {
    b.onclick = () => {
      for (const x of document.querySelectorAll(".speed button")) x.classList.toggle("on", x === b);
      S.speed = +b.dataset.speed;
      const r = R();
      if (S.speed === 0 && r && S.result[S.dims]) { r.setTime(r.end); }
    };
  }
  const scrub = $("scrub");
  let scrubbing = false;
  scrub.addEventListener("input", () => {
    const r = R();
    if (!r || !S.result[S.dims]) return;
    scrubbing = true;
    r.setTime((scrub.value / 1000) * r.end);
  });
  scrub.addEventListener("change", () => (scrubbing = false));
  $("viewBtn").onclick = () => R() && R().resetView();
  $("irBtn").onclick = () => {
    const res = S.result[S.dims];
    const p = $("irPanel");
    p.hidden = !p.hidden;
    if (p.hidden) return;
    if (!res) { $("irText").textContent = "Build first - the IR is produced by the compiler on every build."; return; }
    const lines = res.ir.split("\n");
    const shown = lines.slice(0, 2500);
    $("irInfo").textContent = `${fmt(lines.length)} lines · ${fmt(res.timings.functions)} functions`;
    $("irText").innerHTML = shown.map((l) => {
      const e = esc(l);
      if (/^define/.test(l)) return `<span class="fn">${e}</span>`;
      if (/^\s*;/.test(l) || /^source_filename|^target/.test(l)) return `<span class="cmt">${e}</span>`;
      return e.replace(/(call (?:void|double) @[\w.]+)/, '<span class="call">$1</span>');
    }).join("\n") + (lines.length > shown.length ? `\n\n; … ${fmt(lines.length - shown.length)} more lines` : "");
  };
  $("irClose").onclick = () => ($("irPanel").hidden = true);
  $("stagesBtn").onclick = () => {
    const p = S.preset[S.dims];
    if (p && p.viewer) window.open(`/viewer/index.html#${p.viewer}`, "_blank");
  };
  $("exportBtn").onclick = () => {
    const res = S.result[S.dims];
    if (!res) return;
    const name = (S.preset[S.dims] ? S.preset[S.dims].id : "fractl");
    const [data, ext, type] = S.dims === "3d" ? [r3.exportOBJ(), "obj", "text/plain"] : [res.output, "svg", "image/svg+xml"];
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([data], { type }));
    a.download = `${name}.${ext}`;
    a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 2000);
  };
  document.addEventListener("keydown", (e) => {
    if (e.target === ta) return;
    if (e.code === "Space") { e.preventDefault(); $("playBtn").click(); }
    if (e.key === "Enter" && (e.ctrlKey || e.metaKey)) build();
  });
  $("buildBtn").onclick = build;

  function gpuName(g) {
    const m = /^ANGLE \((.*)\)$/.exec(g);
    const parts = m ? m[1].split(", ") : [g];
    return (parts[1] || parts[0]).replace(/\s*(Direct3D|vs_|ps_|\(0x).*$/, "").replace(/\(TM\)|\(R\)/g, "").slice(0, 40);
  }
  function setStatus(t) { $("statusText").textContent = t; }

  let lastFrame = performance.now();
  function frame(now) {
    const dt = Math.min(0.1, (now - lastFrame) / 1000);
    lastFrame = now;
    const r = R();
    const res = S.result[S.dims];
    if (r && res) {
      if (S.playing && !scrubbing) {
        const t = S.speed === 0 ? r.end : r.time + dt * S.speed;
        r.setTime(Math.min(t, r.end));
        if (r.time >= r.end) { S.playing = false; playUI(); }
      }
      const f = r.end ? r.time / r.end : 0;
      if (!scrubbing) scrub.value = Math.round(f * 1000);
      $("scrubFill").style.width = (f * 100).toFixed(2) + "%";
      const hd = r.head();
      const total = S.dims === "3d" ? res.info.total : res.info.segments;
      const bar = document.querySelector("#st-grow .bar i");
      if (bar) bar.style.width = (f * 100).toFixed(1) + "%";
      if (f >= 1) {
        setStage("grow", "done", null, `${fmt(total)} segments`, `${S.duration} s`);
        setStatus(`Built ${fmt(total)} segments - drag to ${S.dims === "3d" ? "orbit" : "pan"}, scroll to zoom, Build to recompile`);
        $("timeLabel").textContent = `100% · ${fmt(total)}`;
      } else {
        $("st-grow") && $("st-grow").classList.contains("done") && setStage("grow", "run", null, "", "");
        let where = "";
        if (S.dims === "3d" && hd && r3.scene) {
          const ins = r3.scene.inst[hd.inst];
          where = ins.name + (r3.scene.inst.length > 1 ? ` #${ins.index}` : "");
        }
        const n = S.dims === "3d" ? (S.order === "emission" && hd ? hd.order + 1 : Math.round(f * total)) : (hd ? hd.index + 1 : 0);
        $("timeLabel").textContent = `${Math.floor(f * 100)}% · ${fmt(Math.min(n, total))}`;
        const what = S.dims === "3d" ? `${where} · ${fmt(Math.min(n, total))} / ${fmt(total)} segments`
                                     : `segment ${fmt(Math.min(n, total))} of ${fmt(total)}`;
        if (!S.building) setStatus(S.playing ? (S.dims === "3d" ? "Growing " : "Drawing ") + what : `Paused at ${Math.floor(f * 100)}% · ${what}`);
      }
    }
    if (r) r.render(dt);
    if (showFps) {
      frames++;
      if (now - fpsT0 >= 1000) {
        const fps = (frames * 1000) / (now - fpsT0);
        frames = 0; fpsT0 = now;
        const gpu = S.dims === "3d" && r3 ? gpuName(r3.gpu) : "2D canvas";
        $("fpsChip").innerHTML = `fps <b>${fps.toFixed(0)}</b> · ${gpu}`;
      }
    }
    requestAnimationFrame(frame);
  }
  const showFps = new URLSearchParams(location.search).get("fps") === "1";
  let frames = 0, fpsT0 = performance.now();
  if (showFps) {
    const c = document.createElement("span");
    c.id = "fpsChip"; c.className = "chip";
    c.style.cssText = "position:absolute;right:16px;bottom:84px;z-index:6";
    $("stage").appendChild(c);
  }

  window.FRACTL = {
    source: () => ta.value,
    setSource: (text) => { ta.value = text; S.source[S.dims] = text; $("stagesBtn").hidden = true; refreshEditor(); },
    dims: () => S.dims,
    result: () => S.result[S.dims],
    esc, fmt, build,
  };

  (async function boot() {
    try { await loadPresets(); } catch (e) { setStatus("Cannot reach the studio server"); }
    const q = new URLSearchParams(location.search);
    for (const d of ["2d", "3d"]) {
      const p = S.presets.find((x) => x.id === q.get("preset") && x.mode === d);
      if (p) S.preset[d] = p;
    }
    setDims(q.get("mode") === "2d" || glError ? "2d" : "3d", { force: true });
    requestAnimationFrame(frame);
    if (q.get("edit") === "form" && window.FRACTL_setEditMode) window.FRACTL_setEditMode("form");
    if (q.get("view") && window.FRACTL_setView) window.FRACTL_setView(q.get("view"));
    if (q.get("autobuild") !== "0") {
      await build();
      const at = parseFloat(q.get("at"));
      if (at >= 0 && R() && S.result[S.dims]) { R().setTime(at * R().end); S.playing = false; playUI(); }
      const zoom = parseFloat(q.get("zoom"));
      if (zoom > 0 && r3 && S.dims === "3d") { r3.cam.dist /= zoom; r3.cam.pitch = Math.min(r3.cam.pitch, 0.22); }
    }
  })();
})();
