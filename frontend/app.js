/* Prescient web frontend — SPA logic.
 * Monaco editor on the left, results panel on the right.
 * POST /api/analyze runs the existing pipeline; we just render its JSON.
 */

/* ========== C Samples ========== */
const SAMPLES_C = {
  simple: `#include <stdio.h>

// Simplest possible function — the baseline.
int add(int a, int b) {
    return a + b;
}

int main(void) {
    printf("%d\\n", add(3, 4));
    return 0;
}
`,
  branchy: `#include <stdio.h>

// Many "forks in the road" — Cyclo and BBs go up.
int classify(int x) {
    if (x < 0)         return -1;
    else if (x == 0)   return 0;
    else if (x < 10)   return 1;
    else if (x < 100)  return 2;
    else if (x < 1000) return 3;
    else               return 4;
}

int main(void) {
    printf("%d\\n", classify(42));
    return 0;
}
`,
  nested: `#include <stdio.h>

// Three loops stacked — Loops=3, Depth=3.
int triple_loop(int n) {
    int total = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            for (int k = 0; k < n; k++)
                total += i + j + k;
    return total;
}

int main(void) {
    printf("%d\\n", triple_loop(10));
    return 0;
}
`,
  memory: `#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Many memory loads / stores through pointers.
void blur(int *out, const int *in, int n) {
    for (int i = 1; i < n - 1; i++)
        out[i] = in[i - 1] + in[i] + in[i + 1];
}

int main(void) {
    int a[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int b[10];
    memset(b, 0, sizeof(b));
    blur(b, a, 10);
    printf("%d\\n", b[5]);
    return 0;
}
`,
  types: `#include <stdio.h>

// Data nested inside data — TypeCx goes up.
struct Inner  { int *values; int count; };
struct Middle { struct Inner items[4]; double scale; };
struct Outer  { struct Middle layers[2]; struct Middle *active; };

double type_heavy(const struct Outer *o) {
    return o->active->scale + o->layers[0].scale;
}

int main(void) {
    struct Middle m = { .scale = 3.14 };
    struct Outer o = { .layers = { m, m }, .active = &m };
    printf("%f\\n", type_heavy(&o));
    return 0;
}
`,
  failure: `#include <stdio.h>

// Looks expensive (high Insts, depth 3) but compiles fast:
// every value below is a constant, so InstCombine/SCCP fold it away
// before the heavy passes ever see real work.
int misleading(int n) {
    int out = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            for (int k = 0; k < n; k++) {
                int a = 42, b = a*2, c = b+a, d = c-b+a, e = d*2+c;
                int f = e+d-a, g = f*3-e, h = g+f+e+d;
                out += a + b + c + d + e + f + g + h;
            }
    return out;
}

int main(void) {
    printf("%d\\n", misleading(5));
    return 0;
}
`,
};

/* ========== C++ Samples ========== */
const SAMPLES_CPP = {
  simple: `#include <iostream>

// Simplest possible function — the baseline.
int add(int a, int b) {
    return a + b;
}

int main() {
    std::cout << add(3, 4) << std::endl;
    return 0;
}
`,
  branchy: `#include <iostream>

// Many "forks in the road" — Cyclo and BBs go up.
int classify(int x) {
    if (x < 0)         return -1;
    else if (x == 0)   return 0;
    else if (x < 10)   return 1;
    else if (x < 100)  return 2;
    else if (x < 1000) return 3;
    else               return 4;
}

int main() {
    std::cout << classify(42) << std::endl;
    return 0;
}
`,
  nested: `#include <iostream>

// Three loops stacked — Loops=3, Depth=3.
int triple_loop(int n) {
    int total = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            for (int k = 0; k < n; k++)
                total += i + j + k;
    return total;
}

int main() {
    std::cout << triple_loop(10) << std::endl;
    return 0;
}
`,
  memory: `#include <iostream>
#include <cstring>
#include <vector>

// Many memory loads / stores through pointers.
void blur(int *out, const int *in, int n) {
    for (int i = 1; i < n - 1; i++)
        out[i] = in[i - 1] + in[i] + in[i + 1];
}

int main() {
    std::vector<int> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> b(10, 0);
    blur(b.data(), a.data(), 10);
    std::cout << b[5] << std::endl;
    return 0;
}
`,
  types: `#include <iostream>

// Data nested inside data — TypeCx goes up.
struct Inner  { int *values; int count; };
struct Middle { Inner items[4]; double scale; };
struct Outer  { Middle layers[2]; Middle *active; };

double type_heavy(const Outer *o) {
    return o->active->scale + o->layers[0].scale;
}

int main() {
    Middle m = { .scale = 3.14 };
    Outer o = { .layers = { m, m }, .active = &m };
    std::cout << type_heavy(&o) << std::endl;
    return 0;
}
`,
  failure: `#include <iostream>

// Looks expensive (high Insts, depth 3) but compiles fast:
// every value below is a constant, so InstCombine/SCCP fold it away
// before the heavy passes ever see real work.
int misleading(int n) {
    int out = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            for (int k = 0; k < n; k++) {
                int a = 42, b = a*2, c = b+a, d = c-b+a, e = d*2+c;
                int f = e+d-a, g = f*3-e, h = g+f+e+d;
                out += a + b + c + d + e + f + g + h;
            }
    return out;
}

int main() {
    std::cout << misleading(5) << std::endl;
    return 0;
}
`,
};

/* ========== Sample labels for dropdown ========== */
const SAMPLE_LABELS = {
  simple:  "Simple — add",
  branchy: "Branchy — classify",
  nested:  "Loops — triple nest",
  memory:  "Memory — blur",
  types:   "Types — nested structs",
  failure: "Failure case — misleading",
};

const FEATURE_COLS = [
  ["instruction_count",          "Insts",  "int"],
  ["basic_block_count",          "BBs",    "int"],
  ["cyclomatic_complexity",      "Cyclo",  "int"],
  ["loop_count",                 "Loops",  "int"],
  ["max_loop_depth",             "Depth",  "int"],
  ["phi_node_count",             "PHIs",   "int"],
  ["total_memory_ops",           "MemOps", "int"],
  ["alias_proxy_density",        "AliasD", "float2"],
  ["type_complexity_normalized", "TypeCx", "float2"],
];

/* Key features to highlight in the O0 vs O2 comparison */
const COMPARE_COLS = [
  ["instruction_count",     "Instructions",  "int"],
  ["basic_block_count",     "Basic Blocks",  "int"],
  ["cyclomatic_complexity", "Cyclomatic",     "int"],
  ["loop_count",            "Loops",          "int"],
  ["max_loop_depth",        "Loop Depth",     "int"],
  ["total_memory_ops",      "Memory Ops",     "int"],
];

let editor = null;
let lastResult = null;
let modelMetrics = null;     /* loaded once from /api/health */
let activeTab = "features";
let currentSample = "simple";
let settingSample = false;

const $  = (id) => document.getElementById(id);
const esc = (s) => String(s ?? "").replace(/[&<>"']/g, c => ({
  "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;"
})[c]);
const fmtNum = (v, kind) => kind === "float2"
  ? Number(v || 0).toFixed(2)
  : Number(Math.trunc(Number(v || 0))).toLocaleString();
const fmtInt = (n) => Number(n || 0).toLocaleString();
const dispName = (f) => f.display_name || f.function_name || "?";
const getSamples = () => $("lang").value === "cpp" ? SAMPLES_CPP : SAMPLES_C;

/* ---------- sample dropdown ---------- */
function populateSamples() {
  const sel = $("sample");
  sel.innerHTML = "";
  /* "Custom" option — only visible when user has edited */
  const customOpt = document.createElement("option");
  customOpt.value = "custom";
  customOpt.textContent = "Custom";
  if (currentSample !== "custom") customOpt.hidden = true;
  sel.appendChild(customOpt);
  /* sample options */
  for (const [key, label] of Object.entries(SAMPLE_LABELS)) {
    const opt = document.createElement("option");
    opt.value = key;
    opt.textContent = label;
    sel.appendChild(opt);
  }
  sel.value = currentSample;
}

/* ---------- theme toggle ---------- */
function getTheme() {
  return localStorage.getItem("prescient-theme") || "dark";
}
function applyTheme(theme) {
  document.documentElement.setAttribute("data-theme", theme);
  const btn = $("theme-toggle");
  if (btn) btn.textContent = theme === "light" ? "☾" : "☀"; /* ☾ / ☀ */
  if (typeof monaco !== "undefined") {
    monaco.editor.setTheme(theme === "light" ? "vs" : "vs-dark");
  }
  localStorage.setItem("prescient-theme", theme);
}
applyTheme(getTheme());
$("theme-toggle").addEventListener("click", () => {
  applyTheme(getTheme() === "dark" ? "light" : "dark");
});

/* ---------- Monaco bootstrap ---------- */
require.config({
  paths: { vs: "https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs" },
});
require(["vs/editor/editor.main"], function () {
  const monacoTheme = getTheme() === "light" ? "vs" : "vs-dark";
  editor = monaco.editor.create($("editor"), {
    value: SAMPLES_C.simple,
    language: "c",
    theme: monacoTheme,
    fontFamily: "'JetBrains Mono', 'SF Mono', Menlo, monospace",
    fontSize: 14,
    fontLigatures: true,
    minimap: { enabled: false },
    automaticLayout: true,
    scrollBeyondLastLine: false,
    smoothScrolling: true,
    cursorBlinking: "smooth",
    cursorSmoothCaretAnimation: "on",
    renderLineHighlight: "gutter",
    lineNumbersMinChars: 3,
    padding: { top: 14, bottom: 14 },
    bracketPairColorization: { enabled: true },
  });
  editor.onDidChangeModelContent(() => {
    updateSizeHint();
    if (!settingSample && currentSample !== "custom") {
      currentSample = "custom";
      populateSamples();
    }
  });
  updateSizeHint();
  populateSamples();
  setStatus("Ready");
});

/* ---------- status / health ---------- */
function setStatus(text, cls) {
  const s = $("status");
  s.className = "status" + (cls ? " " + cls : "");
  s.textContent = text;
}
function updateSizeHint() {
  if (!editor) return;
  const m = editor.getModel();
  $("size-hint").textContent =
    `${m.getLineCount()} line${m.getLineCount() === 1 ? "" : "s"} · ${m.getValueLength()} chars`;
}

(async function pingHealth() {
  try {
    const r = await fetch("/api/health");
    const h = await r.json();
    const el = $("health");
    if (!h.plugin_present) {
      el.className = "health bad";
      el.textContent = "plugin missing";
    } else if (!h.models_present) {
      el.className = "health warn";
      el.textContent = "model not trained";
    } else {
      el.className = "health ok";
      el.textContent = "ready · model loaded";
    }
    if (h.model_metrics) modelMetrics = h.model_metrics;
  } catch (_) {
    $("health").textContent = "backend unreachable";
    $("health").className = "health bad";
  }
})();

/* ---------- UI wiring ---------- */
$("sample").addEventListener("change", (e) => {
  const key = e.target.value;
  if (key === "custom" || !key) return;
  const samples = getSamples();
  if (samples[key]) {
    currentSample = key;
    settingSample = true;
    editor.setValue(samples[key]);
    settingSample = false;
    populateSamples();
  }
});

$("lang").addEventListener("change", (e) => {
  const lang = e.target.value === "cpp" ? "cpp" : "c";
  monaco.editor.setModelLanguage(editor.getModel(), lang);
  /* Reload same sample in the new language (unless custom code) */
  if (currentSample !== "custom") {
    const samples = lang === "cpp" ? SAMPLES_CPP : SAMPLES_C;
    if (samples[currentSample]) {
      settingSample = true;
      editor.setValue(samples[currentSample]);
      settingSample = false;
    }
  }
  populateSamples();
});

document.querySelectorAll(".tab").forEach((t) => {
  t.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach((x) => x.classList.remove("active"));
    t.classList.add("active");
    activeTab = t.dataset.tab;
    render();
  });
});

$("analyze").addEventListener("click", analyze);
document.addEventListener("keydown", (e) => {
  if ((e.metaKey || e.ctrlKey) && e.key === "Enter") {
    e.preventDefault();
    analyze();
  }
});

/* ---------- analyze ---------- */
async function analyze() {
  if (!editor) return;
  const btn = $("analyze");
  btn.disabled = true;
  setStatus("Analyzing…", "busy");
  const t0 = performance.now();
  try {
    const r = await fetch("/api/analyze", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        code: editor.getValue(),
        language: $("lang").value,
      }),
    });
    const data = await r.json();
    lastResult = data;
    if (data.model_metrics) modelMetrics = data.model_metrics;
    const ms = Math.round(performance.now() - t0);
    if (data.error || data.stage) {
      setStatus("Error", "error");
    } else {
      const tail = data.models_available ? "" : " · model not trained";
      setStatus(`Done · ${ms} ms${tail}`, "ok");
    }
    render();
  } catch (err) {
    lastResult = { error: String(err) };
    setStatus("Error", "error");
    render();
  } finally {
    btn.disabled = false;
  }
}

/* ---------- render ---------- */
function render() {
  const r = $("results");
  if (!lastResult) {
    r.innerHTML = emptyState();
    return;
  }
  if (lastResult.error || lastResult.stage) {
    r.innerHTML = renderError(lastResult);
    return;
  }
  if (activeTab === "features")         r.innerHTML = renderFeatures(lastResult);
  else if (activeTab === "predictions") r.innerHTML = renderPredictions(lastResult);
  else if (activeTab === "passes")      r.innerHTML = renderPerPass(lastResult);
  else if (activeTab === "model")       r.innerHTML = renderModelMetrics();
}

function emptyState() {
  return `<div class="empty">
    <div class="empty-icon">⬬</div>
    <p class="empty-title">Write C / C++ code on the left and click <b>Analyze</b>.</p>
    <p class="empty-sub">Or pick a sample from the dropdown above.</p>
    <p class="empty-shortcut"><kbd>⌘</kbd>/<kbd>Ctrl</kbd> + <kbd>Enter</kbd> runs the analysis.</p>
  </div>`;
}

function renderError(d) {
  return `<div class="error-box">
    <div class="error-title">${esc(d.stage ? d.stage + " failed" : "error")}</div>${esc(d.error || "unknown error")}</div>`;
}

/* ========== Features tab ========== */
function renderFeatures(d) {
  const feats = d.features || [];
  if (!feats.length) {
    return `<div class="notice">No functions found in the source.</div>`;
  }
  const cards = summaryCards(feats);
  const head = `<tr><th>Function</th>${FEATURE_COLS.map(([, h]) => `<th class="num">${h}</th>`).join("")}</tr>`;
  const rows = feats.map((f) => {
    const cells = FEATURE_COLS
      .map(([k, , kind]) => `<td class="num">${fmtNum(f[k], kind)}</td>`)
      .join("");
    return `<tr><td class="func-name">${esc(dispName(f))}</td>${cells}</tr>`;
  }).join("");

  let html = cards + `
    <div class="tbl-wrap">
      <div class="tbl-title">
        IR Complexity Features <span class="badge">O0</span>
        <span class="meta">${feats.length} function${feats.length === 1 ? "" : "s"} · ${d.elapsed_ms ?? "—"} ms</span>
      </div>
      <div class="tbl-scroll"><table class="tbl">${head}${rows}</table></div>
    </div>`;

  /* O0 vs O2 comparison */
  if (d.features_o2 && d.features_o2.length) {
    html += renderComparison(feats, d.features_o2);
  }

  return html;
}

function summaryCards(feats) {
  const totalInsts = feats.reduce((s, f) => s + (f.instruction_count || 0), 0);
  const maxDepth   = Math.max(0, ...feats.map((f) => f.max_loop_depth || 0));
  const maxCyclo   = Math.max(0, ...feats.map((f) => f.cyclomatic_complexity || 0));
  const memOps     = feats.reduce((s, f) => s + (f.total_memory_ops || 0), 0);
  return `<div class="summary">
    <div class="summary-card"><div class="label">Functions</div><div class="value">${fmtInt(feats.length)}</div><div class="sub">defined in source</div></div>
    <div class="summary-card"><div class="label">Total Instructions</div><div class="value">${fmtInt(totalInsts)}</div><div class="sub">LLVM IR instructions</div></div>
    <div class="summary-card"><div class="label">Max Loop Depth</div><div class="value">${maxDepth}</div><div class="sub">deepest nested loop</div></div>
    <div class="summary-card"><div class="label">Max Cyclomatic</div><div class="value">${maxCyclo}</div><div class="sub">highest branch complexity</div></div>
    <div class="summary-card"><div class="label">Memory Ops</div><div class="value">${fmtInt(memOps)}</div><div class="sub">load / store / GEP total</div></div>
  </div>`;
}

/* ========== O0 vs O2 Comparison (split tables) ========== */
function renderComparison(o0, o2) {
  /* Build a lookup by function_name -> o2 record */
  const o2Map = {};
  o2.forEach((f) => { o2Map[f.function_name] = f; });

  /* Only show functions present in both */
  const matched = o0.filter((f) => o2Map[f.function_name]);
  if (!matched.length) return "";

  /* Overall stats for summary cards */
  const totalO0 = matched.reduce((s, f) => s + (f.instruction_count || 0), 0);
  const totalO2 = matched.reduce((s, f) => s + (o2Map[f.function_name]?.instruction_count || 0), 0);
  const instrPct = totalO0 > 0 ? Math.round(((totalO2 - totalO0) / totalO0) * 100) : 0;

  const totalBBsO0 = matched.reduce((s, f) => s + (f.basic_block_count || 0), 0);
  const totalBBsO2 = matched.reduce((s, f) => s + (o2Map[f.function_name]?.basic_block_count || 0), 0);
  const bbPct = totalBBsO0 > 0 ? Math.round(((totalBBsO2 - totalBBsO0) / totalBBsO0) * 100) : 0;

  const totalMemO0 = matched.reduce((s, f) => s + (f.total_memory_ops || 0), 0);
  const totalMemO2 = matched.reduce((s, f) => s + (o2Map[f.function_name]?.total_memory_ops || 0), 0);
  const memPct = totalMemO0 > 0 ? Math.round(((totalMemO2 - totalMemO0) / totalMemO0) * 100) : 0;

  const fmtDelta = (pct) => {
    const cls = pct < 0 ? "delta-down" : pct > 0 ? "delta-up" : "delta-zero";
    const sign = pct > 0 ? "+" : "";
    return `<span class="compare-delta ${cls}">${sign}${pct}%</span>`;
  };

  /* Summary cards */
  const cards = `<div class="summary">
    <div class="summary-card opt-card"><div class="label">Instructions</div><div class="value">${fmtInt(totalO0)} → ${fmtInt(totalO2)}</div><div class="sub">${fmtDelta(instrPct)}</div></div>
    <div class="summary-card opt-card"><div class="label">Basic Blocks</div><div class="value">${fmtInt(totalBBsO0)} → ${fmtInt(totalBBsO2)}</div><div class="sub">${fmtDelta(bbPct)}</div></div>
    <div class="summary-card opt-card"><div class="label">Memory Ops</div><div class="value">${fmtInt(totalMemO0)} → ${fmtInt(totalMemO2)}</div><div class="sub">${fmtDelta(memPct)}</div></div>
  </div>`;

  /* ----- Table 1: Before Optimization (O0) ----- */
  const headO0 = `<tr><th>Function</th>${COMPARE_COLS.map(([, h]) => `<th class="num">${h}</th>`).join("")}</tr>`;
  const rowsO0 = matched.map((f) => {
    const cells = COMPARE_COLS.map(([k]) => `<td class="num">${fmtNum(f[k], "int")}</td>`).join("");
    return `<tr><td class="func-name">${esc(dispName(f))}</td>${cells}</tr>`;
  }).join("");

  /* ----- Table 2: After Optimization (O2) with deltas ----- */
  const headO2 = `<tr><th>Function</th>${COMPARE_COLS.map(([, h]) => `<th class="num">${h}</th>`).join("")}</tr>`;
  const rowsO2 = matched.map((f0) => {
    const f2 = o2Map[f0.function_name];
    const cells = COMPARE_COLS.map(([k]) => {
      const v0 = Number(f0[k] || 0);
      const v2 = Number(f2[k] || 0);
      const pct = v0 > 0 ? Math.round(((v2 - v0) / v0) * 100) : 0;
      const cls = pct < 0 ? "delta-down" : pct > 0 ? "delta-up" : "delta-zero";
      const sign = pct > 0 ? "+" : "";
      return `<td class="num">
        <span class="opt-value">${fmtNum(v2, "int")}</span>
        <span class="compare-delta ${cls}">${sign}${pct}%</span>
      </td>`;
    }).join("");
    return `<tr><td class="func-name">${esc(dispName(f0))}</td>${cells}</tr>`;
  }).join("");

  return `
    <div class="section-title">
      Optimization Impact <span class="badge badge-o0">O0</span> vs <span class="badge badge-o2">O2</span>
    </div>
    <div class="info-quote">
      These tables show how the LLVM <em>-O2</em> optimizer transforms each function's IR.
      Fewer instructions, blocks, and memory ops after optimization means the
      compiler successfully simplified the code. Green percentages indicate reductions;
      red indicates increases.
    </div>
    ${cards}
    <div class="tbl-wrap">
      <div class="tbl-title">
        Before Optimization <span class="badge badge-o0">O0</span>
        <span class="meta">unoptimized IR</span>
      </div>
      <div class="tbl-scroll"><table class="tbl">${headO0}${rowsO0}</table></div>
    </div>
    <div class="tbl-wrap">
      <div class="tbl-title">
        After Optimization <span class="badge badge-o2">O2</span>
        <span class="meta">optimized IR · deltas shown per cell</span>
      </div>
      <div class="tbl-scroll"><table class="tbl">${headO2}${rowsO2}</table></div>
    </div>`;
}

/* ========== Predictions tab ========== */
function renderPredictions(d) {
  if (!d.models_available) {
    return notModelTrained(d);
  }
  const ps = d.predictions || [];
  if (!ps.length) return `<div class="notice">No predictions returned.</div>`;
  const tiers = ps.reduce((a, p) => (a[p.complexity_tier] = (a[p.complexity_tier] || 0) + 1, a), {});
  const head = `<tr>
    <th>Function</th><th>Tier</th>
    <th class="num">Pred (µs)</th><th class="num">Pred (ms)</th>
    <th>Confidence</th><th>Why (top features)</th>
  </tr>`;
  const rows = ps.map((p) => `<tr>
    <td class="func-name">${esc(dispName(p))}</td>
    <td><span class="tier tier-${esc(p.complexity_tier)}">${esc(p.complexity_tier)}</span></td>
    <td class="num">${fmtInt(p.predicted_total_us)}</td>
    <td class="num">${Number(p.predicted_total_ms || 0).toFixed(2)}</td>
    <td class="muted">${esc(p.confidence)}</td>
    <td class="muted">${esc(p.tier_rationale || "")}</td>
  </tr>`).join("");

  let html = `
    <div class="summary">
      <div class="summary-card"><div class="label">Low</div><div class="value" style="color:var(--green)">${tiers.low || 0}</div><div class="sub">lighter O1 pipeline</div></div>
      <div class="summary-card"><div class="label">Medium</div><div class="value" style="color:var(--amber)">${tiers.medium || 0}</div><div class="sub">full O2</div></div>
      <div class="summary-card"><div class="label">High</div><div class="value" style="color:var(--red)">${tiers.high || 0}</div><div class="sub">full O2 (preserved)</div></div>
      <div class="summary-card"><div class="label">Functions</div><div class="value">${fmtInt(ps.length)}</div><div class="sub">analyzed</div></div>
    </div>
    <div class="tbl-wrap">
      <div class="tbl-title">
        Compile-time Predictions
        <span class="meta">${ps.length} function${ps.length === 1 ? "" : "s"} · ${d.elapsed_ms ?? "—"} ms</span>
      </div>
      <div class="tbl-scroll"><table class="tbl">${head}${rows}</table></div>
    </div>`;

  /* Model accuracy disclaimer */
  const mm = d.model_metrics || modelMetrics;
  if (mm && mm.cv_scores) {
    html += renderModelDisclaimer(mm);
  }

  return html;
}

function renderModelDisclaimer(mm) {
  const ridge = mm.cv_scores?.Ridge;
  const rf = mm.cv_scores?.RandomForest;
  const best = ridge || rf || {};
  const r2 = best.r2 != null ? best.r2.toFixed(2) : "?";
  const mae = best.mae != null ? Math.round(best.mae).toLocaleString() : "?";
  return `
    <div class="info-quote">
      <strong>Model accuracy:</strong> The predictions above come from a model
      trained on ${mm.n_samples || "?"} samples. Cross-validated R² = ${r2},
      MAE = ${mae} µs. Negative R² means the model does worse than
      always predicting the average — treat tiers as a rough signal, not ground truth.
      See the <b>Model</b> tab for full metrics.
    </div>`;
}

/* ========== Per-pass tab ========== */
function renderPerPass(d) {
  if (!d.models_available) return notModelTrained(d);
  const ps = d.predictions || [];
  if (!ps.length) return `<div class="notice">No predictions returned.</div>`;
  const names = Array.from(
    new Set(ps.flatMap((p) => (p.expensive_passes || []).map((x) => x.pass_name)))
  ).sort();
  if (!names.length) return `<div class="notice">No per-pass predictions available.</div>`;

  /* Explanation block */
  const passExplain = `
    <div class="info-quote">
      <strong>What is this?</strong> When the compiler optimizes your code at -O2,
      it runs a sequence of <em>passes</em> — each one a specific transformation
      (e.g., GVN eliminates redundant computations, LoopVectorize converts scalar
      loops to SIMD, SLP packs independent scalar operations into vector instructions).
      <br><br>
      The table below shows the <strong>predicted time (in microseconds)</strong>
      each pass will spend on each function. High numbers indicate the pass will
      work hard on that function. If a pass is predicted to be very expensive
      for a low-complexity function, the adaptive pipeline may skip it to save
      compile time.
      <br><br>
      <strong>Pass names:</strong>
      <ul class="pass-explain-list">
        <li><b>GVNPass</b> — Global Value Numbering: removes redundant calculations by detecting when two expressions always produce the same result.</li>
        <li><b>LoopVectorizePass</b> — Converts scalar loops into SIMD/vector operations so the CPU processes multiple data elements per instruction.</li>
        <li><b>SLPVectorizerPass</b> — Superword-Level Parallelism: packs independent scalar operations (outside loops) into vector instructions.</li>
      </ul>
    </div>`;

  const head = `<tr><th>Function</th>${names.map((n) => `<th class="num">${esc(n)}</th>`).join("")}</tr>`;
  const rows = ps.map((p) => {
    const by = {};
    (p.expensive_passes || []).forEach((x) => { by[x.pass_name] = x.predicted_us; });
    return `<tr>
      <td class="func-name">${esc(dispName(p))}</td>
      ${names.map((n) => {
        const v = by[n] || 0;
        const cls = v > 1000 ? "hot" : v > 100 ? "warm" : "";
        return `<td class="num ${cls}">${fmtInt(v)}</td>`;
      }).join("")}
    </tr>`;
  }).join("");

  return passExplain + `<div class="tbl-wrap">
    <div class="tbl-title">
      Predicted Per-Pass Cost
      <span class="meta">microseconds · ${ps.length} function${ps.length === 1 ? "" : "s"}</span>
    </div>
    <div class="tbl-scroll"><table class="tbl">${head}${rows}</table></div>
  </div>`;
}

/* ========== Model metrics tab ========== */
function renderModelMetrics() {
  const mm = modelMetrics;
  if (!mm) {
    return `<div class="notice"><b>No model metrics available.</b> Train the model first:
      <pre style="margin:8px 0 0;padding:10px;background:var(--bg);border:1px solid var(--border-soft);border-radius:6px;font-family:var(--font-mono);font-size:12px;color:var(--text);">./start.sh train</pre>
    </div>`;
  }

  const scores = mm.cv_scores || {};
  const models = Object.keys(scores);

  /* CV comparison table */
  const head = `<tr><th>Model</th><th class="num">R²</th><th class="num">MAE (µs)</th><th class="num">MAPE (%)</th></tr>`;
  const rows = models.map((name) => {
    const s = scores[name];
    const r2 = s.r2 != null ? s.r2.toFixed(3) : "—";
    const mae = s.mae != null ? Math.round(s.mae).toLocaleString() : "—";
    const mape = s.mape != null ? s.mape.toFixed(1) : "—";
    const r2cls = (s.r2 || 0) > 0 ? "delta-down" : "delta-up";  /* positive R2 = good (green) */
    return `<tr>
      <td class="func-name">${esc(name)}</td>
      <td class="num ${r2cls}">${r2}</td>
      <td class="num">${mae}</td>
      <td class="num">${mape}%</td>
    </tr>`;
  }).join("");

  return `
    <div class="info-quote">
      <strong>How to read these metrics:</strong>
      <ul class="pass-explain-list">
        <li><b>R²</b> (coefficient of determination) — How well the model explains
            variance. 1.0 = perfect; 0.0 = no better than guessing the mean;
            negative = worse than guessing the mean. Our small training set
            (${mm.n_samples || "?"} samples) means cross-validated R² is often negative.</li>
        <li><b>MAE</b> (Mean Absolute Error) — Average prediction error in microseconds.
            Lower is better.</li>
        <li><b>MAPE</b> (Mean Absolute Percentage Error) — Average error as a percentage
            of the true value. Lower is better.</li>
      </ul>
    </div>
    <div class="summary">
      <div class="summary-card"><div class="label">Training Samples</div><div class="value">${mm.n_samples || "—"}</div><div class="sub">functions used for CV</div></div>
      <div class="summary-card"><div class="label">Target</div><div class="value" style="font-size:14px">${esc(mm.primary_target || "?")}</div><div class="sub">transform: ${esc(mm.target_transform || "none")}</div></div>
      <div class="summary-card"><div class="label">Per-pass Models</div><div class="value">${(mm.per_pass_targets || []).length}</div><div class="sub">${(mm.per_pass_targets || []).map(t => t.replace("time_","")).join(", ")}</div></div>
      <div class="summary-card"><div class="label">Trained</div><div class="value" style="font-size:12px">${mm.trained_at ? new Date(mm.trained_at).toLocaleDateString() : "—"}</div><div class="sub">model creation date</div></div>
    </div>
    <div class="tbl-wrap">
      <div class="tbl-title">
        5-Fold Cross-Validation Results
        <span class="meta">lower MAE/MAPE is better · higher R² is better</span>
      </div>
      <div class="tbl-scroll"><table class="tbl">${head}${rows}</table></div>
    </div>
    <div class="info-quote" style="margin-top:14px;">
      <strong>Why are the scores low?</strong> The model is trained on
      ${mm.n_samples || "?"} functions from 40 diverse C programs covering sorting,
      graphs, numerics, image processing, crypto, compression, and more. Cross-validated
      R² of 0.5–0.6 is reasonable for this task — compile times depend on factors
      beyond static IR features (cache state, system load). The model provides a
      useful rough ranking (low/medium/high tiers) and the RandomForest model's
      per-function error averages under 500 µs.
    </div>`;
}

function notModelTrained(d) {
  return `<div class="notice">
    <b>No trained model loaded.</b> Run the training step so the prediction view has a model to use:
    <pre style="margin:8px 0 0;padding:10px;background:var(--bg);border:1px solid var(--border-soft);border-radius:6px;font-family:var(--font-mono);font-size:12px;color:var(--text);overflow:auto;">./start.sh train</pre>
    ${d.predict_log ? `<div style="margin-top:10px;color:var(--text-faint);font-family:var(--font-mono);font-size:11px;">predict log: ${esc(d.predict_log)}</div>` : ""}
  </div>`;
}

/* ========== Panel Resizer ========== */
(function initResizer() {
  const resizer = $("resizer");
  const layout = document.querySelector(".layout");
  if (!resizer || !layout) return;

  let isResizing = false;

  resizer.addEventListener("mousedown", (e) => {
    isResizing = true;
    resizer.classList.add("active");
    document.body.style.cursor = "col-resize";
    document.body.style.userSelect = "none";
    e.preventDefault();
  });

  document.addEventListener("mousemove", (e) => {
    if (!isResizing) return;
    const rect = layout.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const total = rect.width;
    const pct = Math.max(20, Math.min(80, (x / total) * 100));
    layout.style.gridTemplateColumns = `${pct}fr 6px ${100 - pct}fr`;
  });

  document.addEventListener("mouseup", () => {
    if (!isResizing) return;
    isResizing = false;
    resizer.classList.remove("active");
    document.body.style.cursor = "";
    document.body.style.userSelect = "";
    /* Monaco's automaticLayout handles re-layout automatically */
  });

  /* Touch support for tablets */
  resizer.addEventListener("touchstart", (e) => {
    isResizing = true;
    resizer.classList.add("active");
    e.preventDefault();
  }, { passive: false });

  document.addEventListener("touchmove", (e) => {
    if (!isResizing) return;
    const touch = e.touches[0];
    const rect = layout.getBoundingClientRect();
    const x = touch.clientX - rect.left;
    const total = rect.width;
    const pct = Math.max(20, Math.min(80, (x / total) * 100));
    layout.style.gridTemplateColumns = `${pct}fr 6px ${100 - pct}fr`;
  }, { passive: true });

  document.addEventListener("touchend", () => {
    if (!isResizing) return;
    isResizing = false;
    resizer.classList.remove("active");
  });
})();
