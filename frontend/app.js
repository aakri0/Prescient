/* Prescient web frontend — SPA logic.
 * Monaco editor on the left, results panel on the right.
 * POST /api/analyze runs the existing pipeline; we just render its JSON.
 */

const SAMPLES = {
  simple: `// Simplest possible function — the baseline.
int add(int a, int b) {
    return a + b;
}
`,
  branchy: `// Many "forks in the road" — Cyclo and BBs go up.
int classify(int x) {
    if (x < 0)         return -1;
    else if (x == 0)   return 0;
    else if (x < 10)   return 1;
    else if (x < 100)  return 2;
    else if (x < 1000) return 3;
    else               return 4;
}
`,
  nested: `// Three loops stacked — Loops=3, Depth=3.
int triple_loop(int n) {
    int total = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            for (int k = 0; k < n; k++)
                total += i + j + k;
    return total;
}
`,
  memory: `// Many memory loads / stores through pointers.
void blur(int *out, const int *in, int n) {
    for (int i = 1; i < n - 1; i++)
        out[i] = in[i - 1] + in[i] + in[i + 1];
}
`,
  types: `// Data nested inside data — TypeCx goes up.
struct Inner  { int *values; int count; };
struct Middle { struct Inner items[4]; double scale; };
struct Outer  { struct Middle layers[2]; struct Middle *active; };

double type_heavy(const struct Outer *o) {
    return o->active->scale + o->layers[0].scale;
}
`,
  failure: `// Looks expensive (high Insts, depth 3) but compiles fast:
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
`,
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

let editor = null;
let lastResult = null;
let activeTab = "features";

const $  = (id) => document.getElementById(id);
const esc = (s) => String(s ?? "").replace(/[&<>"']/g, c => ({
  "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;"
})[c]);
const fmtNum = (v, kind) => kind === "float2"
  ? Number(v || 0).toFixed(2)
  : Number(Math.trunc(Number(v || 0))).toLocaleString();
const fmtInt = (n) => Number(n || 0).toLocaleString();

/* ---------- Monaco bootstrap ---------- */
require.config({
  paths: { vs: "https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs" },
});
require(["vs/editor/editor.main"], function () {
  editor = monaco.editor.create($("editor"), {
    value: SAMPLES.simple,
    language: "c",
    theme: "vs-dark",
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
  editor.onDidChangeModelContent(updateSizeHint);
  updateSizeHint();
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
  } catch (_) {
    $("health").textContent = "backend unreachable";
    $("health").className = "health bad";
  }
})();

/* ---------- UI wiring ---------- */
$("sample").addEventListener("change", (e) => {
  const key = e.target.value;
  if (key && SAMPLES[key]) {
    editor.setValue(SAMPLES[key]);
    e.target.value = "";
  }
});

$("lang").addEventListener("change", (e) => {
  const lang = e.target.value === "cpp" ? "cpp" : "c";
  monaco.editor.setModelLanguage(editor.getModel(), lang);
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
  if (activeTab === "features")    r.innerHTML = renderFeatures(lastResult);
  else if (activeTab === "predictions") r.innerHTML = renderPredictions(lastResult);
  else if (activeTab === "passes")      r.innerHTML = renderPerPass(lastResult);
}

function emptyState() {
  return `<div class="empty">
    <div class="empty-icon">⌬</div>
    <p class="empty-title">Write C / C++ code on the left and click <b>Analyze</b>.</p>
    <p class="empty-sub">Or pick a sample from the dropdown above.</p>
    <p class="empty-shortcut"><kbd>⌘</kbd>/<kbd>Ctrl</kbd> + <kbd>Enter</kbd> runs the analysis.</p>
  </div>`;
}

function renderError(d) {
  return `<div class="error-box">
    <div class="error-title">${esc(d.stage ? d.stage + " failed" : "error")}</div>${esc(d.error || "unknown error")}</div>`;
}

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
    return `<tr><td class="func-name">${esc(f.function_name)}</td>${cells}</tr>`;
  }).join("");
  return cards + `
    <div class="tbl-wrap">
      <div class="tbl-title">
        IR Complexity Features
        <span class="meta">${feats.length} function${feats.length === 1 ? "" : "s"} · ${d.elapsed_ms ?? "—"} ms · full 23-field detail in JSON</span>
      </div>
      <table class="tbl">${head}${rows}</table>
    </div>`;
}

function summaryCards(feats) {
  const totalInsts = feats.reduce((s, f) => s + (f.instruction_count || 0), 0);
  const maxDepth   = Math.max(0, ...feats.map((f) => f.max_loop_depth || 0));
  const maxCyclo   = Math.max(0, ...feats.map((f) => f.cyclomatic_complexity || 0));
  const memOps     = feats.reduce((s, f) => s + (f.total_memory_ops || 0), 0);
  return `<div class="summary">
    <div class="summary-card"><div class="label">Functions</div><div class="value">${fmtInt(feats.length)}</div></div>
    <div class="summary-card"><div class="label">Total instructions</div><div class="value">${fmtInt(totalInsts)}</div><div class="sub">across all functions</div></div>
    <div class="summary-card"><div class="label">Max loop depth</div><div class="value">${maxDepth}</div><div class="sub">deepest nest</div></div>
    <div class="summary-card"><div class="label">Max cyclomatic</div><div class="value">${maxCyclo}</div><div class="sub">most-branchy function</div></div>
    <div class="summary-card"><div class="label">Memory ops</div><div class="value">${fmtInt(memOps)}</div><div class="sub">load/store/GEP</div></div>
  </div>`;
}

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
    <td class="func-name">${esc(p.function_name)}</td>
    <td><span class="tier tier-${esc(p.complexity_tier)}">${esc(p.complexity_tier)}</span></td>
    <td class="num">${fmtInt(p.predicted_total_us)}</td>
    <td class="num">${Number(p.predicted_total_ms || 0).toFixed(2)}</td>
    <td class="muted">${esc(p.confidence)}</td>
    <td class="muted">${esc(p.tier_rationale || "")}</td>
  </tr>`).join("");
  return `
    <div class="summary">
      <div class="summary-card"><div class="label">Low</div><div class="value" style="color:var(--green)">${tiers.low || 0}</div><div class="sub">lighter O1 pipeline</div></div>
      <div class="summary-card"><div class="label">Medium</div><div class="value" style="color:var(--amber)">${tiers.medium || 0}</div><div class="sub">full O2</div></div>
      <div class="summary-card"><div class="label">High</div><div class="value" style="color:var(--red)">${tiers.high || 0}</div><div class="sub">full O2 (preserved)</div></div>
      <div class="summary-card"><div class="label">Functions</div><div class="value">${fmtInt(ps.length)}</div></div>
    </div>
    <div class="tbl-wrap">
      <div class="tbl-title">
        Compile-time Predictions
        <span class="meta">${ps.length} function${ps.length === 1 ? "" : "s"} · ${d.elapsed_ms ?? "—"} ms</span>
      </div>
      <table class="tbl">${head}${rows}</table>
    </div>`;
}

function renderPerPass(d) {
  if (!d.models_available) return notModelTrained(d);
  const ps = d.predictions || [];
  if (!ps.length) return `<div class="notice">No predictions returned.</div>`;
  const names = Array.from(
    new Set(ps.flatMap((p) => (p.expensive_passes || []).map((x) => x.pass_name)))
  ).sort();
  if (!names.length) return `<div class="notice">No per-pass predictions available.</div>`;
  const head = `<tr><th>Function</th>${names.map((n) => `<th class="num">${esc(n)}</th>`).join("")}</tr>`;
  const rows = ps.map((p) => {
    const by = {};
    (p.expensive_passes || []).forEach((x) => { by[x.pass_name] = x.predicted_us; });
    return `<tr>
      <td class="func-name">${esc(p.function_name)}</td>
      ${names.map((n) => `<td class="num">${fmtInt(by[n] || 0)}</td>`).join("")}
    </tr>`;
  }).join("");
  return `<div class="tbl-wrap">
    <div class="tbl-title">
      Predicted Per-Pass Cost
      <span class="meta">microseconds · ${ps.length} function${ps.length === 1 ? "" : "s"}</span>
    </div>
    <table class="tbl">${head}${rows}</table>
  </div>`;
}

function notModelTrained(d) {
  return `<div class="notice">
    <b>No trained model loaded.</b> Run the training step so the prediction view has a model to use:
    <pre style="margin:8px 0 0;padding:10px;background:var(--bg);border:1px solid var(--border-soft);border-radius:6px;font-family:var(--font-mono);font-size:12px;color:var(--text);overflow:auto;">./run.sh train
# or
docker compose run --rm prescient train</pre>
    ${d.predict_log ? `<div style="margin-top:10px;color:var(--text-faint);font-family:var(--font-mono);font-size:11px;">predict log: ${esc(d.predict_log)}</div>` : ""}
  </div>`;
}
