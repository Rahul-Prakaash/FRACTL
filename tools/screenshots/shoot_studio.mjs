// Captures FRACTL Studio screenshots through the Chrome DevTools protocol.
// Start the studio first (Launch FRACTL Studio.bat, or `python3 studio.py 8732`), then:
//   node tools/screenshots/shoot_studio.mjs <outdir> tuft:3d field:3d shrub:3d tree3d:3d meadow:3d koch:2d ...
// Each argument is <preset id>:<2d|3d> from studio/presets.json. Writes <id>_<mode>.png and meta.json
// (the pipeline timings and counts shown in each screenshot). Needs Node 22+ and Microsoft Edge;
// set EDGE to another Chromium-based browser if needed, HEADED=1 to watch it work.
import { spawn } from "node:child_process";
import { writeFileSync, mkdirSync } from "node:fs";

const [outDir, ...shots] = process.argv.slice(2);
mkdirSync(outDir, { recursive: true });
const EDGE = process.env.EDGE || "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe";
const PORT = 9333;
const headless = process.env.HEADED ? [] : ["--headless=new"];
const edge = spawn(EDGE, [...headless, `--remote-debugging-port=${PORT}`, `--user-data-dir=${outDir}/../edge-profile`,
  "--window-size=1920,1080", "--force-device-scale-factor=1", "--ignore-gpu-blocklist", "--enable-gpu",
  "--enable-unsafe-swiftshader", "--no-first-run", "--hide-scrollbars", "about:blank"], { stdio: "ignore" });

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let ws, id = 0;
const pending = new Map();
function send(method, params = {}) {
  return new Promise((res, rej) => {
    const i = ++id;
    pending.set(i, { res, rej });
    ws.send(JSON.stringify({ id: i, method, params }));
  });
}
async function evaluate(expr) {
  const r = await send("Runtime.evaluate", { expression: expr, returnByValue: true, awaitPromise: true });
  return r.result && r.result.value;
}

try {
  let target;
  for (let i = 0; i < 50 && !target; i++) {
    try {
      const list = await (await fetch(`http://127.0.0.1:${PORT}/json`)).json();
      target = list.find((t) => t.type === "page");
    } catch { await sleep(200); }
  }
  ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((r) => (ws.onopen = r));
  ws.onmessage = (m) => {
    const d = JSON.parse(m.data);
    if (d.id && pending.has(d.id)) { const p = pending.get(d.id); pending.delete(d.id); d.error ? p.rej(new Error(d.error.message)) : p.res(d.result); }
  };
  await send("Page.enable");
  await send("Runtime.enable");
  await send("Emulation.setDeviceMetricsOverride", { width: 1920, height: 1080, deviceScaleFactor: 1, mobile: false });

  const meta = {};
  for (const spec of shots) {
    const [pid, mode, extra = ""] = spec.split(":");
    const url = `http://127.0.0.1:8732/?preset=${pid}&mode=${mode}&at=1${extra ? "&" + extra : ""}`;
    await send("Page.navigate", { url });
    let ok = false;
    for (let i = 0; i < 240; i++) {
      await sleep(250);
      ok = await evaluate(`!!(window.FRACTL && window.FRACTL.result())`).catch(() => false);
      if (ok) break;
    }
    await sleep(2500); // let the renderer settle (shadows, last frames)
    const info = await evaluate(`JSON.stringify({dims: window.FRACTL && window.FRACTL.dims(), title: document.getElementById('presetTitle')?.textContent, chips: [...document.querySelectorAll('.chip')].map(c=>c.textContent.trim()), total: document.getElementById('totalTime')?.textContent, pipe: [...document.querySelectorAll('#pipeline li')].map(li=>li.innerText.replace(/\\s+/g,' ').trim())})`);
    const shot = await send("Page.captureScreenshot", { format: "png" });
    const file = `${outDir}/${pid}_${mode}.png`;
    writeFileSync(file, Buffer.from(shot.data, "base64"));
    console.log(ok ? "ok  " : "TIMEOUT", file); meta[pid + "_" + mode] = JSON.parse(info);
  }
  writeFileSync(`${outDir}/meta.json`, JSON.stringify(meta, null, 1));
} finally {
  try { await send("Browser.close"); } catch {}
  setTimeout(() => { edge.kill(); process.exit(0); }, 500);
}
