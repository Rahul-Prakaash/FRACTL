
const path = require("path");
const { JSDOM, VirtualConsole } = require("jsdom");

const VIEWER = process.argv[2] || path.join(__dirname, "..");

let fails = 0, ran = 0;
function check(name, cond, extra) {
  ran++;
  if (!cond) { fails++; console.log("  FAIL " + name + (extra ? "  [" + extra + "]" : "")); }
  else console.log("  ok   " + name + (extra ? "  (" + extra + ")" : ""));
}

const vc = new VirtualConsole();
const errors = [];
vc.on("jsdomError", (e) => errors.push("jsdomError: " + (e.detail || e.message)));
vc.on("error", (...a) => errors.push("console.error: " + a.join(" ")));
process.on("unhandledRejection", (e) => {
  console.log("UNHANDLED REJECTION:", e && (e.stack || e.message || e));
  process.exit(3);
});

const wait = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  const dom = await JSDOM.fromFile(path.join(VIEWER, "index.html"), {
    runScripts: "dangerously",
    resources: "usable",
    pretendToBeVisual: true,
    virtualConsole: vc,
  });
  const w = dom.window;
  const $ = (id) => w.document.getElementById(id);
  const click = (node) => node.dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
  await wait(900);

  console.log("\n-- load (file:// + bundle.js) --");
  check("bundle.js loaded", !!w.FRACTL_BUNDLE,
        w.FRACTL_BUNDLE ? Object.keys(w.FRACTL_BUNDLE).length + " entries" : "run pack.ps1");
  check("mode badge", $("modeBadge").textContent === "bundle.js", $("modeBadge").textContent);
  check("no warning banner", $("banner").hidden, $("banner").textContent.slice(0, 70));

  const tabs = [...w.document.querySelectorAll(".tab")];
  const tabFor = (n) => tabs.find((t) => t.dataset.name === n);
  check("tabs built from manifest", tabs.length > 0, tabs.map((t) => t.textContent).join(","));
  check("triangle present", !!tabFor("triangle"));
  check("koch present", !!tabFor("koch"));

  console.log("\n-- triangle (iterations 0: the integration smoke demo) --");
  click(tabFor("triangle"));
  await wait(350);
  check("source rendered", $("srcPre").textContent.includes("fractal Triangle"));
  check("ast fields", $("astBody").querySelectorAll("dd").length === 7);
  check("tokens listed", $("tokBody").querySelectorAll("tbody tr").length === 40);
  check("7 commands", $("irCount").textContent.startsWith("7 commands"), $("irCount").textContent);
  check("expanded == axiom", $("expPre").textContent === "F--F--F", $("expPre").textContent);
  check("no full-toggle for a short string", $("expToggle").hidden);
  check("stat bars drawn", $("statBody").querySelectorAll(".fill").length > 0);
  check("iteration slider hidden at 0 iterations", $("iterPanel").hidden);

  console.log("\n-- koch --");
  click(tabFor("koch"));
  await wait(450);
  check("1792 commands", $("irCount").textContent.startsWith("1,792 commands"), $("irCount").textContent);
  check("ir list chunked", $("irBody").querySelector("pre").textContent.split("\n").length === 400);
  check("expanded shows preview", $("expPre").textContent.length === 202);
  check("full-toggle labelled", $("expToggle").textContent === "show full (1,792 chars)",
        $("expToggle").textContent);
  click($("expToggle"));
  await wait(50);
  check("full string on demand", $("expPre").textContent.length === 1792);
  click($("irMore"));
  await wait(50);
  check("ir expands to all rows", $("irBody").querySelector("pre").textContent.split("\n").length === 1792);

  console.log("\n-- iteration slider --");
  check("slider shown", !$("iterPanel").hidden);
  check("slider max is the demo's iteration count", $("iterRange").max === "4", $("iterRange").max);
  const setFrame = async (k) => {
    $("iterRange").value = String(k);
    $("iterRange").dispatchEvent(new w.Event("input", { bubbles: true }));
    await wait(250);
  };
  await setFrame(0); check("n=0 is the axiom", $("expCount").textContent === "7 symbols", $("expCount").textContent);
  await setFrame(1); check("n=1 is 28 symbols", $("expCount").textContent === "28 symbols", $("expCount").textContent);
  await setFrame(2); check("n=2 is 112 symbols", $("expCount").textContent === "112 symbols", $("expCount").textContent);
  await setFrame(4); check("n=4 returns to the base demo", $("expCount").textContent === "1,792 symbols",
                           $("expCount").textContent);

  console.log("\n-- panels degrade, never blank --");
  check("svg panel shows a placeholder when M2's .svg is absent",
        !!$("svgStage").querySelector("svg") || $("svgStage").textContent.includes("No data/koch.svg"),
        $("svgStage").textContent.slice(0, 40));
  check("llvm panel shows something",
        $("llBody").textContent.trim().length > 0, $("llBody").textContent.slice(0, 40));

  console.log("\n-- controls --");
  const collapse = [...w.document.querySelectorAll("[data-collapse]")];
  for (const b of collapse) check("collapse target exists: " + b.dataset.collapse, !!$(b.dataset.collapse));
  click(collapse[0]);
  check("collapse hides its panel", $(collapse[0].dataset.collapse).hidden);
  click(collapse[0]);
  check("expand restores it", !$(collapse[0].dataset.collapse).hidden);

  click($("zoomIn"));
  check("zoom in", $("zoomReadout").textContent === "130%", $("zoomReadout").textContent);
  click($("zoomReset"));
  check("reset view", $("zoomReadout").textContent === "100%");
  const wheel = new w.WheelEvent("wheel",
    { deltaY: -240, clientX: 100, clientY: 100, bubbles: true, cancelable: true });
  $("svgViewport").dispatchEvent(wheel);
  check("wheel zooms", $("zoomReadout").textContent !== "100%", $("zoomReadout").textContent);
  check("wheel does not scroll the page", wheel.defaultPrevented);

  console.log("\n-- page errors --");
  check("none", errors.length === 0, errors.slice(0, 3).join(" | "));

  console.log("\n" + ran + " checks, " + fails + " failed");
  process.exit(fails ? 1 : 0);
})().catch((e) => { console.log("HARNESS ERROR:", e && (e.stack || e)); process.exit(1); });
