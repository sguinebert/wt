/* eslint-disable max-lines */
// StdLayout2.js – Modern ES 2022+ rewrite of the legacy “StdLayout2”
// -----------------------------------------------------------------------------
// • 100 % evergreen‑browser features (ES2022+, ESM)
// • No globals: simply `import StdLayout2 from "./StdLayout2.js"`
// • Private class fields for internal state
// • ResizeObserver + MutationObserver for live re‑flow
// • Pure‑CSS Grid for layout ‑‑ no hand‑crafted absolute maths anymore
// -----------------------------------------------------------------------------

/* eslint-disable max-lines */

export default class StdLayout2 {
  /* ────────────────────────────────────────────────────────────────────────── */
  /**
   * @param {Object}  cfg                         – configuration object
   * @param {HTMLElement} cfg.root                – container (must be in DOM)
   * @param {Array<{stretch?:number,min?:number}>} [cfg.rows=[]]    – row tracks
   * @param {Array<{stretch?:number,min?:number}>} [cfg.columns=[]] – column tracks
   * @param {Array<Object>} [cfg.items=[]]        – grid items ({el,row,col,…})
   * @param {boolean} [cfg.rtl=false]             – RTL flow direction
   * @param {[number,number,number]} [cfg.hMargins=[8,8,8]] – [gap,left,right]
   * @param {[number,number,number]} [cfg.vMargins=[8,8,8]] – [gap,top,bottom]
   */
  #busy = false; // true while measuring + applying
  constructor(cfg = {}) {
    /* ---------------------------- runtime config --------------------------- */
    this.#conf = {
      root      : cfg.root ?? (() => { throw new Error("root element required"); })(),
      rows      : cfg.rows      ?? [],
      columns   : cfg.columns   ?? [],
      items     : cfg.items     ?? [],
      rtl       : cfg.rtl       ?? false,
      margins   : {
        h : cfg.hMargins ?? [8, 8, 8], // [gap, left, right]
        v : cfg.vMargins ?? [8, 8, 8]  // [gap, top, bottom]
      }
    };

    if (!this.#conf.root.isConnected) {
      throw new Error("StdLayout2 › root element must be attached to the DOM");
    }

    /* ----------------------------- observers ------------------------------ */
    this.#resizeObs = new ResizeObserver(() => this.refresh());
    this.#resizeObs.observe(this.#conf.root);

    this.#mutObs = new MutationObserver(() => this.refresh());
    this.#mutObs.observe(this.#conf.root, { childList: true, subtree: true, attributes: true });

    /* Root base‑style */
    Object.assign(this.#conf.root.style, {
      position  : "relative",
      boxSizing : "border-box",
      display   : "grid"
    });

    this.refresh();
  }

  /* ───────────────────────────── public API ─────────────────────────────── */
  addRow(track = { stretch: 1 })         { this.#conf.rows.push(track); return this.refresh(); }
  addColumn(track = { stretch: 1 })      { this.#conf.columns.push(track); return this.refresh(); }

  addItem(item)    { return this.#editItem("add",      item); }
  removeItem(el)   { return this.#editItem("remove",   el);   }
  updateItem(el,u) { return this.#editItem("update",   el, u); }
  /** Flip RTL/LTR on the fly */
  setRTL(val=true) { this.#conf.rtl = !!val; return this.refresh(); }
  /** Force re‑measure + re‑layout (idempotent / cheap when nothing changed) */
  refresh()       { if (this.#busy) return this; this.#busy = true; try { this.#measure(); this.#apply(); } finally { this.#busy = false; } return this; }


  /** Disconnect observers and free references */
  dispose() {
    this.#resizeObs.disconnect();
    this.#mutObs.disconnect();
    this.#conf.items.length = 0;
  }

  /* ─────────────────────────── private fields ───────────────────────────── */
  #conf;              // runtime configuration
  #measures = { rows: [], cols: [] }; // min + pref cache per track
  #resizeObs;         // ResizeObserver instance
  #mutObs;            // MutationObserver instance

  /* ──────────────────────────── private API ─────────────────────────────── */
  /** Edits the item array in a single place (add/update/remove). */
  #editItem(action, elOrCfg, updates = undefined) {
    if (action === "add") {
      const itm = {
        el      : elOrCfg.el,
        row     : elOrCfg.row     ?? 0,
        col     : elOrCfg.col     ?? 0,
        rowSpan : elOrCfg.rowSpan ?? 1,
        colSpan : elOrCfg.colSpan ?? 1
      };
      this.#ensureGridSize(itm);
      this.#conf.items.push(itm);
    } else {
      const id   = typeof elOrCfg === "string" ? elOrCfg : elOrCfg.id;
      const item = this.#conf.items.find(i => i.el === id || i.el === elOrCfg);
      if (!item) return this;
      if (action === "remove") {
        this.#conf.items.splice(this.#conf.items.indexOf(item), 1);
      } else if (action === "update") {
        Object.assign(item, updates);
        this.#ensureGridSize(item);
      }
    }
    return this.refresh();
  }

  /** Make sure row / column arrays are large enough for an item. */
  #ensureGridSize(item) {
    const wantRows = item.row + item.rowSpan;
    const wantCols = item.col + item.colSpan;
    while (this.#conf.rows.length    < wantRows) this.#conf.rows.push({ stretch: 1 });
    while (this.#conf.columns.length < wantCols) this.#conf.columns.push({ stretch: 1 });
  }

  /** Measure min & preferred sizes (scroll boxes) per track. */
  #measure() {
    const R = this.#conf.rows.length;
    const C = this.#conf.columns.length;
    this.#measures.rows = Array.from({ length: R }, () => ({ min: 0 }));
    this.#measures.cols = Array.from({ length: C }, () => ({ min: 0 }));

    for (const itm of this.#conf.items) {
      const el = typeof itm.el === "string" ? document.getElementById(itm.el) : itm.el;
      if (!el) continue;

      const s   = getComputedStyle(el);
      const minW = parseFloat(s.minWidth)  || 0;
      const minH = parseFloat(s.minHeight) || 0;

      /* For now we only keep minima – CSS Grid will stretch automatically. */
      this.#measures.rows[itm.row].min = Math.max(this.#measures.rows[itm.row].min, minH);
      this.#measures.cols[itm.col].min = Math.max(this.#measures.cols[itm.col].min, minW);
    }
  }

  /** Apply CSS Grid styles to root + items. */
  #apply() {
    const { root, rows, columns, margins, rtl } = this.#conf;

    // 1 – grid templates ---------------------------------------------------
    const tpl = (tracks, measures) => tracks.map((t, i) => {
      const min = Math.max(t.min ?? 0, measures[i]?.min ?? 0);
      return t.stretch ? `minmax(${min}px, ${t.stretch}fr)` : `${min}px`;
    }).join(" ");

    root.style.gridTemplateRows    = tpl(rows,    this.#measures.rows);
    root.style.gridTemplateColumns = tpl(columns, this.#measures.cols);
    root.style.gap     = `${margins.v[0]}px ${margins.h[0]}px`;
    root.style.padding = `${margins.v[1]}px ${margins.h[1]}px ${margins.v[2]}px ${margins.h[2]}px`;
    root.style.direction = rtl ? "rtl" : "ltr";

    // 2 – position items ---------------------------------------------------
    for (const itm of this.#conf.items) {
      const el = typeof itm.el === "string" ? document.getElementById(itm.el) : itm.el;
      if (!el) continue;

      Object.assign(el.style, {
        position        : "relative",      // override legacy absolute
        gridRow         : `${itm.row + 1} / span ${itm.rowSpan ?? 1}`,
        gridColumn      : `${itm.col + 1} / span ${itm.colSpan ?? 1}`,
        width           : "auto",
        height          : "auto",
        boxSizing       : "border-box"
      });
    }
  }
}
