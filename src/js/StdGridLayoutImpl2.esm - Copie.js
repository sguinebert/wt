// StdGridLayout.js — ES2022+ helper that lets CSS Grid do the heavy lifting
// ---------------------------------------------------------------------------
// ▸ No measurement loops: Grid’s own algo decides track sizes.
// ▸ ResizeObserver keeps the template in sync with container size changes.
// ▸ *Optional* MutationObserver auto‑refreshes when direct children are
//   inserted/removed; you can disable it for extremely large DOM trees.
// ▸ Each child may declare its cell with a simple data‑attribute.
// ---------------------------------------------------------------------------
//   <div id="root">
//     <section data-grid="0,0"> … </section> <!-- row 0, col 0 -->
//     <aside   data-grid="0,1"> … </aside>   <!-- row 0, col 1 -->
//   </div>
//   import StdGridLayout from "./StdGridLayout.js";
//   const layout = new StdGridLayout({
//     root:   document.getElementById("root"),
//     rows: [               // one entry per track
//       { fr:1   },         // fr = flex‑fraction; min defaults to 0
//       { min:120 }         // fixed‑height track
//     ],
//     cols: [ {fr:2}, {fr:1} ],
//     gap : 8,              // shorthand for [hor,ver]
//     watch: true           // attach MutationObserver
//   });
//   // If you dynamically add <div data-grid="1,0">…</div>, layout.refresh()
//   // is auto‑called (or you can call it yourself).

export default class StdGridLayout {
  /* --------------------------------------------------------------------- */
  /**
   * @param {Object} opts
   * @param {HTMLElement} opts.root   – grid container (must be in the DOM)
   * @param {Array}  opts.rows        – [{fr,min}] or number (=px) per row
   * @param {Array}  opts.cols        – same for columns
   * @param {number|[number,number]} opts.gap – px gap (hor[,ver])
   * @param {boolean} [opts.watch]    – observe childList for auto‑refresh
   */
  constructor({ root, rows = [], cols = [], gap = 8, watch = true }) {
    if (!root?.isConnected) throw new Error("root must be attached to DOM");

    this.#root = root;
    this.#rows = rows;
    this.#cols = cols;
    [this.#gapH, this.#gapV] = Array.isArray(gap) ? gap : [gap, gap];

    // Apply template once ------------------------------------------------
    this.#applyTemplate();

    // Keep template up‑to‑date when the container itself is resized ------
    this.#ro = new ResizeObserver(() => this.refresh());
    this.#ro.observe(this.#root);

    // Auto‑refresh on child additions/removals ---------------------------
    if (watch) {
      this.#mo = new MutationObserver(() => this.refresh());
      this.#mo.observe(this.#root, { childList: true });
    }

    this.refresh();
  }

  /* --------------------------------------------------------------------- */
  /** Re‑computes the `grid-row/column` placement for current children. */
  refresh() {
    // In case caller mutated the rows/cols arrays directly
    this.#applyTemplate();

    this.#root.childNodes.forEach((node) => {
      if (node.nodeType !== 1) return; // ignore text nodes, etc.

      const coords = node.dataset.grid?.split(",").map(Number);
      if (!coords || coords.length < 2) return;

      Object.assign(node.style, {
        gridRow: coords[0] + 1,   // Grid is 1‑based
        gridColumn: coords[1] + 1,
      });
    });
  }

  /** Disconnect observers and clean up */
  dispose() {
    this.#ro.disconnect();
    this.#mo?.disconnect();
  }

  /* --------------------------------------------------------------------- */
  // ── private data & helpers ────────────────────────────────────────────
  #root;
  #rows;
  #cols;
  #gapH;
  #gapV;
  #ro;   // ResizeObserver
  #mo;   // MutationObserver (optional)

  #template(trackArr) {
    return trackArr
      .map((t) =>
        typeof t === "number"
          ? `${t}px` // fixed track
          : t.fr
          ? `minmax(${t.min ?? 0}px, ${t.fr}fr)`
          : `${t.min ?? 0}px`
      )
      .join(" ");
  }

  #applyTemplate() {
    Object.assign(this.#root.style, {
      display: "grid",
      gridTemplateRows: this.#template(this.#rows),
      gridTemplateColumns: this.#template(this.#cols),
      gap: `${this.#gapV}px ${this.#gapH}px`,
    });
  }
}

/* -----------------------------------------------------------------------
 * Optional helper: make any element a drag‑resize splitter that updates a
 * CSS variable (so Grid reacts instantly) – ≈20 LOC, keep separate.
 * --------------------------------------------------------------------- */
export function attachSplitter(handle, cssVar, min = 100) {
  handle.style.cursor = "ew-resize";
  handle.addEventListener("pointerdown", (ev) => {
    const start = ev.clientX;
    const root = document.documentElement;
    const init = parseFloat(getComputedStyle(root).getPropertyValue(cssVar));
    handle.setPointerCapture(ev.pointerId);
    handle.onpointermove = (mv) => {
      const val = Math.max(min, init + mv.clientX - start);
      root.style.setProperty(cssVar, val + "px");
    };
    handle.onpointerup = () => (handle.onpointermove = null);
  });
}
