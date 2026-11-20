const CATEGORY = {
  PROGRAM: "decl", FUNC: "decl", PARAMS: "decl", PARAM: "decl", VAR_DECL: "decl", VAR: "decl",
  BLOCK: "stmt", IF: "stmt", WHILE: "stmt", FOR: "stmt", RETURN: "stmt", BREAK: "stmt",
  CONTINUE: "stmt", EXPR_STMT: "stmt", EMPTY: "stmt",
  ASSIGN: "op", BINOP: "op", UNOP: "op", INCDEC: "op",
  CALL: "call",
  ID: "leaf", INT_LIT: "leaf", FLOAT_LIT: "leaf", STRING_LIT: "leaf",
};

const COLOR = {
  decl: "#c792ea", stmt: "#82aaff", op: "#f78c6c", call: "#ffcb6b", leaf: "#c3e88d",
};

const CHILD_ROLES = {
  IF: ["condition", "then", "else"],
  WHILE: ["condition", "body"],
  FOR: ["init", "condition", "step", "body"],
  FUNC: ["params", "body"],
  BINOP: ["left", "right"],
};

const colorOf = d => COLOR[CATEGORY[d.data.kind] || "stmt"];

function shorten(s, max = 20) {
  s = s.replace(/\n/g, "\\n").replace(/\t/g, "\\t");
  return s.length > max ? s.slice(0, max - 1) + "…" : s;
}

function label(n) {
  const nm = n.uname ? `${n.name} [${n.uname}]` : n.name;
  switch (n.kind) {
    case "FUNC":       return ["FUNC", `${n.decl_type} ${n.name}()`];
    case "PARAM":      return ["PARAM", `${n.decl_type} ${nm}`];
    case "VAR_DECL":   return ["VAR_DECL", n.decl_type];
    case "VAR":        return ["VAR", nm];
    case "ID":         return ["", nm];
    case "INT_LIT":    return ["", String(n.value)];
    case "FLOAT_LIT":  return ["", Number.isInteger(n.value) ? n.value.toFixed(1) : String(n.value)];
    case "STRING_LIT": return ["", `"${shorten(n.value)}"`];
    case "BINOP":
    case "UNOP":       return [n.kind, n.op];
    case "ASSIGN":     return ["ASSIGN", `${nm} ${n.op}`];
    case "INCDEC":     return ["INCDEC", n.prefix ? `${n.op}${nm}` : `${nm}${n.op}`];
    case "CALL":       return ["CALL", `${n.name}()`];
    default:           return [n.kind, ""];
  }
}

const svg = d3.select("#tree").append("svg");
const zoomLayer = svg.append("g");
const linkLayer = zoomLayer.append("g");
const nodeLayer = zoomLayer.append("g");

const zoom = d3.zoom()
  .scaleExtent([0.15, 3])
  .on("zoom", e => zoomLayer.attr("transform", e.transform));
svg.call(zoom).on("dblclick.zoom", null);

const NODE_H = 26;
const NODE_W = 230;
const treeLayout = d3.tree().nodeSize([NODE_H, NODE_W]);
const linkPath = d3.linkHorizontal()
  .source(d => ({ x: d.source.x, y: d.source.y + Math.min((d.source.lw || 0) + 16, NODE_W - 40) }))
  .target(d => d.target)
  .x(d => d.y).y(d => d.x);

let root = null;
let data = null;
let nextId = 0;

function toggle(d) {
  if (!d._children) return;
  d.children = d.children ? null : d._children;
}

function expandAll() {
  root.each(d => { d.children = d._children || null; });
  update(root);
  setTimeout(fit, 420);
}

function collapseTo(depth) {
  root.each(d => { d.children = d._children && d.depth < depth ? d._children : null; });
  update(root);
  setTimeout(fit, 420);
}

function update(source) {
  treeLayout(root);
  const nodes = root.descendants();
  const links = root.links();
  const t = svg.transition().duration(380);

  const node = nodeLayer.selectAll("g.node").data(nodes, d => d.id);

  const enter = node.enter().append("g")
    .attr("class", "node")
    .attr("transform", `translate(${source.y0},${source.x0})`)
    .attr("opacity", 0)
    .on("click", (e, d) => { toggle(d); update(d); })
    .on("mouseenter", (e, d) => { showTooltip(e, d); highlightLine(d.data.line, true); })
    .on("mousemove", moveTooltip)
    .on("mouseleave", () => { hideTooltip(); });

  enter.append("circle").attr("r", 6);

  const text = enter.append("text").attr("dy", "0.32em").attr("x", 11);
  text.each(function (d) {
    const [k, detail] = label(d.data);
    const el = d3.select(this);
    if (k) el.append("tspan").attr("class", "k").attr("fill", colorOf(d)).text(k + (detail ? " " : ""));
    if (detail) el.append("tspan").attr("fill", CATEGORY[d.data.kind] === "leaf" ? colorOf(d) : null).text(detail);
    if (d.data.type && d.data.type !== "string") el.append("tspan").attr("class", "ty").text(" : " + d.data.type);
    el.append("tspan").attr("class", "more");
  });

  const merged = enter.merge(node);
  merged.transition(t)
    .attr("transform", d => `translate(${d.y},${d.x})`)
    .attr("opacity", 1);

  merged.select("circle")
    .attr("stroke", colorOf)
    .attr("fill", d => (d._children && !d.children) ? colorOf(d) : "#0f1117");
  merged.select("tspan.more")
    .text(d => (d._children && !d.children) ? `  ▸ ${countHidden(d)}` : "");
  merged.select("text").each(function (d) { d.lw = 11 + this.getComputedTextLength(); });

  node.exit().transition(t)
    .attr("transform", `translate(${source.y},${source.x})`)
    .attr("opacity", 0)
    .remove();

  const link = linkLayer.selectAll("path.link").data(links, d => d.target.id);

  link.enter().append("path")
    .attr("class", "link")
    .attr("d", () => { const o = { x: source.x0, y: source.y0 }; return linkPath({ source: o, target: o }); })
    .merge(link)
    .transition(t)
    .attr("d", linkPath);

  link.exit().transition(t)
    .attr("d", () => { const o = { x: source.x, y: source.y }; return linkPath({ source: o, target: o }); })
    .remove();

  nodes.forEach(d => { d.x0 = d.x; d.y0 = d.y; });
}

function countHidden(d) {
  let c = 0;
  (function walk(n) { (n._children || []).forEach(ch => { c++; walk(ch); }); })(d);
  return c;
}

const MIN_SCALE = 0.55;
function fit() {
  const nodes = root.descendants();
  const xs = nodes.map(d => d.x), ys = nodes.map(d => d.y);
  const minX = d3.min(xs) - 40, maxX = d3.max(xs) + 40;
  const minY = d3.min(ys) - 30, maxY = d3.max(ys) + 230;
  const { width, height } = svg.node().getBoundingClientRect();
  let scale = Math.min(1.15, 0.95 * Math.min(width / (maxY - minY), height / (maxX - minX)));
  let tx = (width - scale * (maxY - minY)) / 2 - scale * minY;
  let ty = (height - scale * (maxX - minX)) / 2 - scale * minX;
  if (scale < MIN_SCALE) {
    scale = MIN_SCALE;
    tx = 30 - scale * minY;
    ty = height / 2 - scale * root.x;
  }
  svg.transition().duration(500).call(zoom.transform, d3.zoomIdentity.translate(tx, ty).scale(scale));
}

const tooltip = d3.select("#tooltip");

function showTooltip(e, d) {
  const n = d.data;
  const rows = [];
  const add = (k, v) => rows.push(`<div class="tt-row"><span>${k}</span>${escapeHtml(String(v))}</div>`);
  add("рядок", n.line);
  if (n.name && n.kind !== "STRING_LIT") add("ім'я", n.name);
  if (n.uname) add("у Python", n.uname);
  if (n.op) add("оператор", n.op);
  if (n.decl_type) add("тип оголош.", n.decl_type);
  if (n.type) add("тип виразу", n.type);
  if (n.value !== undefined) add("значення", typeof n.value === "string" ? JSON.stringify(n.value) : n.value);
  if (n.global) add("область", "глобальна змінна");
  if (d.parent && CHILD_ROLES[d.parent.data.kind]) {
    const idx = d.parent._children.indexOf(d);
    const role = CHILD_ROLES[d.parent.data.kind][idx];
    if (role) add("роль", `${role} (у ${d.parent.data.kind})`);
  }
  if (d._children) add("дітей", d._children.length);

  tooltip.html(`<div class="tt-kind" style="color:${colorOf(d)}">${n.kind}</div>${rows.join("")}`)
    .style("opacity", 1);
  moveTooltip(e);
}

function moveTooltip(e) {
  const pad = 16, node = tooltip.node();
  let x = e.clientX + pad, y = e.clientY + pad;
  if (x + node.offsetWidth > innerWidth) x = e.clientX - node.offsetWidth - pad;
  if (y + node.offsetHeight > innerHeight) y = e.clientY - node.offsetHeight - pad;
  tooltip.style("left", x + "px").style("top", y + "px");
}

function hideTooltip() { tooltip.style("opacity", 0); }

function escapeHtml(s) {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

const C_KW  = new Set(["if", "else", "while", "for", "return", "break", "continue"]);
const C_TY  = new Set(["int", "double", "void"]);
const PY_KW = new Set(["def", "return", "if", "elif", "else", "while", "for", "in", "break",
                       "continue", "import", "global", "and", "or", "not", "pass", "raise",
                       "True", "False", "None"]);
const PY_TY = new Set(["int", "float", "bool", "abs", "len"]);

function highlight(line, lang) {
  const re = lang === "c"
    ? /(\/\/.*$|\/\*.*?\*\/|\/\*.*$|^\s*#.*$)|("(?:[^"\\]|\\.)*")|(\b\d+\.?\d*(?:[eE][+-]?\d+)?\b)|([A-Za-z_]\w*)(?=\s*\()|([A-Za-z_]\w*)/g
    : /(#.*$)|("""(?:.*?)"""|"(?:[^"\\]|\\.)*")|(\b\d+\.?\d*(?:[eE][+-]?\d+)?\b)|([A-Za-z_]\w*)(?=\s*\()|([A-Za-z_]\w*)/g;
  const kw = lang === "c" ? C_KW : PY_KW;
  const ty = lang === "c" ? C_TY : PY_TY;

  let out = "", last = 0, m;
  while ((m = re.exec(line)) !== null) {
    out += escapeHtml(line.slice(last, m.index));
    const [tok, com, str, num, fn, id] = m;
    let cls = null;
    if (com)      cls = (lang === "c" && tok.trim().startsWith("#")) ? "tok-pp" : "tok-com";
    else if (str) cls = "tok-str";
    else if (num) cls = "tok-num";
    else {
      const w = fn || id;
      if (kw.has(w))      cls = "tok-kw";
      else if (ty.has(w)) cls = "tok-ty";
      else if (fn)        cls = "tok-fn";
    }
    out += cls ? `<span class="${cls}">${escapeHtml(tok)}</span>` : escapeHtml(tok);
    last = m.index + tok.length;
    if (tok.length === 0) re.lastIndex++;
  }
  return out + escapeHtml(line.slice(last));
}

function renderCode(container, text, lang) {
  const lines = text.replace(/\n$/, "").split("\n");
  container.html(lines.map((l, i) =>
    `<div class="ln" data-line="${i + 1}"><span class="no">${i + 1}</span><span>${lang ? highlight(l, lang) : escapeHtml(l) || " "}</span></div>`
  ).join(""));
}

function highlightLine(line, scroll) {
  const box = d3.select("#code-c");
  box.selectAll(".ln").classed("hl", false);
  const el = box.select(`.ln[data-line="${line}"]`);
  el.classed("hl", true);
  if (scroll && el.node() && box.classed("active")) el.node().scrollIntoView({ block: "nearest", behavior: "smooth" });
}

function selectLine(line) {
  highlightLine(line, false);
  const matches = [];
  (function walk(d) {
    if (d.data.line === line) matches.push(d);
    (d._children || []).forEach(walk);
  })(root);
  matches.forEach(d => { for (let p = d.parent; p; p = p.parent) p.children = p._children; });
  update(root);
  nodeLayer.selectAll("g.node").classed("match", d => d.data.line === line);
  setTimeout(fit, 420);
}

function load(name) {
  d3.json(`data/${name}.json`).then(json => {
    data = json;

    root = d3.hierarchy(data.ast, n => n.children);
    root.each(d => {
      d.id = nextId++;
      d._children = d.children || null;
      if (d.depth >= 4) d.children = null;
    });
    root.x0 = 0;
    root.y0 = 0;

    nodeLayer.selectAll("*").remove();
    linkLayer.selectAll("*").remove();
    update(root);
    fit();

    renderCode(d3.select("#code-c"), data.source, "c");
    renderCode(d3.select("#code-py"), data.python, "py");
    renderCode(d3.select("#code-out"), data.output, null);
    d3.selectAll("#code-c .ln").on("click", function () { selectLine(+this.dataset.line); });

    let total = 0, depth = 0, funcs = 0;
    (function walk(n, dep) {
      total++; depth = Math.max(depth, dep);
      if (n.kind === "FUNC") funcs++;
      (n.children || []).forEach(c => walk(c, dep + 1));
    })(data.ast, 0);
    d3.select("#stats").html(
      `<b>${data.file}</b> · вузлів: <b>${total}</b> · глибина: <b>${depth}</b> · функцій: <b>${funcs}</b>`);

    history.replaceState(null, "", `#${name}`);
  });
}

d3.selectAll(".tab").on("click", function () {
  const tab = this.dataset.tab;
  d3.selectAll(".tab").classed("active", function () { return this.dataset.tab === tab; });
  d3.selectAll(".code").classed("active", false);
  d3.select(`#code-${tab}`).classed("active", true);
});

d3.select("#btn-expand").on("click", expandAll);
d3.select("#btn-collapse").on("click", () => collapseTo(2));
d3.select("#btn-fit").on("click", fit);

d3.json("data/index.json").then(names => {
  const select = d3.select("#example");
  select.selectAll("option").data(names).join("option").attr("value", d => d).text(d => `${d}.c`);
  const initial = names.includes(location.hash.slice(1)) ? location.hash.slice(1) : names[0];
  select.property("value", initial).on("change", function () { load(this.value); });
  load(initial);
});

window.addEventListener("resize", () => root && fit());
