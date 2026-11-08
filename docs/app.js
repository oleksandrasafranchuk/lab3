let module;

class Exit {
  constructor(code) { this.code = code; }
}

async function cparse(args, source) {
  const input = new TextEncoder().encode(source);
  const argv = ["cparse", ...args, "-"].map(a => new TextEncoder().encode(a + "\0"));
  const out = { 1: [], 2: [] };
  let memory, pos = 0;
  const view = () => new DataView(memory.buffer);
  const bytes = () => new Uint8Array(memory.buffer);

  const wasi = {
    args_sizes_get(pc, ps) {
      view().setUint32(pc, argv.length, true);
      view().setUint32(ps, argv.reduce((s, a) => s + a.length, 0), true);
      return 0;
    },
    args_get(pv, pb) {
      for (const a of argv) {
        view().setUint32(pv, pb, true);
        bytes().set(a, pb);
        pv += 4;
        pb += a.length;
      }
      return 0;
    },
    fd_write(fd, iovs, n, written) {
      let total = 0;
      for (let i = 0; i < n; i++) {
        const p = view().getUint32(iovs + i * 8, true), len = view().getUint32(iovs + i * 8 + 4, true);
        if (out[fd]) out[fd].push(bytes().slice(p, p + len));
        total += len;
      }
      view().setUint32(written, total, true);
      return 0;
    },
    fd_read(fd, iovs, n, read) {
      let total = 0;
      for (let i = 0; i < n && fd === 0 && pos < input.length; i++) {
        const p = view().getUint32(iovs + i * 8, true), len = view().getUint32(iovs + i * 8 + 4, true);
        const chunk = input.subarray(pos, pos + len);
        bytes().set(chunk, p);
        pos += chunk.length;
        total += chunk.length;
      }
      view().setUint32(read, total, true);
      return 0;
    },
    fd_fdstat_get(fd, p) {
      if (fd > 2) return 8;
      view().setUint8(p, 2);
      return 0;
    },
    fd_fdstat_set_flags: () => 0,
    fd_prestat_get: () => 8,
    fd_prestat_dir_name: () => 8,
    fd_seek: () => 70,
    fd_close: () => 0,
    path_open: () => 44,
    proc_exit(code) { throw new Exit(code); },
  };

  const instance = await WebAssembly.instantiate(module, { wasi_snapshot_preview1: wasi });
  memory = instance.exports.memory;
  let code = 0;
  try {
    instance.exports._start();
  } catch (e) {
    code = e instanceof Exit ? e.code : 1;
    if (!(e instanceof Exit)) out[2].push(new TextEncoder().encode(String(e)));
  }
  const text = fd => new TextDecoder().decode(new Uint8Array(out[fd].flatMap(b => [...b])));
  return { code, stdout: text(1), stderr: text(2) };
}

function drawAst(data) {
  const svg = d3.select("svg");
  svg.selectAll("*").remove();
  if (!data) {
    svg.append("text").attr("x", 20).attr("y", 30).attr("fill", "#c00").text("AST не побудовано: є синтаксичні помилки");
    return;
  }

  const g = svg.append("g");
  const zoom = d3.zoom().scaleExtent([0.1, 3]).on("zoom", e => g.attr("transform", e.transform));
  svg.call(zoom);

  const root = d3.hierarchy(data);
  const sub = d => d.data.label === d.data.kind ? (d.data.ctype || "") : d.data.label + (d.data.ctype ? " : " + d.data.ctype : "");
  const width = d => Math.max(sub(d).length * 6.8, d.data.kind.length * 7.4) + 18;
  d3.tree().nodeSize([1, 70]).separation((a, b) => (width(a) + width(b)) / 2 + 12)(root);

  g.selectAll(".link").data(root.links()).join("path")
    .attr("class", "link")
    .attr("d", d3.linkVertical().x(d => d.x).y(d => d.y));

  const node = g.selectAll(".node").data(root.descendants()).join("g")
    .attr("class", "node")
    .attr("transform", d => `translate(${d.x},${d.y})`);
  node.append("rect").attr("x", d => -width(d) / 2).attr("y", -17).attr("width", width).attr("height", 34).attr("rx", 4);
  node.append("text").attr("text-anchor", "middle").attr("y", -3).text(d => d.data.kind);
  node.append("text").attr("text-anchor", "middle").attr("y", 11).attr("class", "type").text(sub);

  const nodes = root.descendants();
  const x0 = d3.min(nodes, d => d.x - width(d) / 2), x1 = d3.max(nodes, d => d.x + width(d) / 2);
  const y0 = -17, y1 = d3.max(nodes, d => d.y) + 17;
  const W = svg.node().clientWidth, H = svg.node().clientHeight;
  const k = Math.min(1.3, (W - 30) / (x1 - x0), (H - 30) / (y1 - y0));
  svg.call(zoom.transform, d3.zoomIdentity.translate(W / 2 - k * (x0 + x1) / 2, (H - k * (y1 - y0)) / 2 - k * y0).scale(k));
}

async function analyze() {
  const code = document.getElementById("code").value;
  const out = document.getElementById("out");
  const py = document.getElementById("py");

  const parsed = await cparse(["-j"], code);
  drawAst(parsed.stdout ? JSON.parse(parsed.stdout) : null);

  if (parsed.code !== 0) {
    out.innerHTML = `<span class="err">${parsed.stderr.replace(/</g, "&lt;")}</span>`;
    py.textContent = "Трансляція не виконується: програма містить помилки";
    return;
  }
  const result = await cparse(["-r"], code);
  out.innerHTML = result.stdout.replace(/</g, "&lt;") +
    (result.stderr ? `<span class="err">${result.stderr.replace(/</g, "&lt;")}</span>` : "") +
    `\n<span class="${result.code ? "err" : "ok"}">[програма завершилась з кодом ${result.code}]</span>`;
  py.textContent = (await cparse(["-p"], code)).stdout;
}

const select = document.getElementById("examples");
EXAMPLES.forEach((e, i) => select.add(new Option(e.title, i)));
select.onchange = () => {
  document.getElementById("code").value = EXAMPLES[select.value].code;
  analyze();
};
document.getElementById("run").onclick = analyze;
document.getElementById("code").value = EXAMPLES[0].code;
document.getElementById("src-parser").textContent = SOURCES.parser;
document.getElementById("src-lexer").textContent = SOURCES.lexer;

(async () => {
  const bin = Uint8Array.from(atob(WASM), c => c.charCodeAt(0));
  module = await WebAssembly.compile(bin);
  analyze();
})();
