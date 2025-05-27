/* eslint-disable max-lines */
// StdLayout2.js – Modern ES 2022+ rewrite of the legacy “StdLayout2”
// -----------------------------------------------------------------------------
// • 100 % evergreen‑browser features (ES2022+, ESM)
// • No globals: simply `import StdLayout2 from "./StdLayout2.js"`
// • Private class fields for internal state
// • ResizeObserver + MutationObserver for live re‑flow
// • Pure‑CSS Grid for layout ‑‑ no hand‑crafted absolute maths anymore
// -----------------------------------------------------------------------------

import { size } from "../vendor/floating-ui/floating-ui.core.browser.min.mjs";

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
  #enableSplitters = false; 
  constructor(cfg = {}) {
    /* ---------------------------- runtime config --------------------------- */
    this.#conf = {
      root      : cfg.root ?? (() => { throw new Error("root element required"); })(),
      rows      : cfg.rows      ?? [],
      cols   : cfg.columns   ?? [],
      items     : cfg.items     ?? [],
      rtl       : cfg.rtl       ?? false,
      margins   : {
        h : cfg.hMargins ?? [8, 8, 8], // [gap, left, right]
        v : cfg.vMargins ?? [8, 8, 8]  // [gap, top, bottom]
      }
    };
    this.#enableSplitters = cfg.splitters ?? false; 

    console.log('this.#conf', this.#conf.cols);

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
  enableSplitters(on = true) {
    this.#enableSplitters = !!on;
    return this.refresh();
  }

  /* ───────────────────────────── public API ─────────────────────────────── */
  addRow(track = { stretch: 1 })         { this.#conf.rows.push(track); return this.refresh(); }
  addColumn(track = { stretch: 1 })      { this.#conf.cols.push(track); return this.refresh(); }

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
    while (this.#conf.cols.length < wantCols) this.#conf.cols.push({ stretch: 1 });
  }
  setColSize(col, size) {
        if (col < 0 || col >= this.#conf.cols.length) {
      throw new Error(`StdLayout2 › invalid column index: ${col}`);
    }
    if (typeof size !== "number" || size < 0) {
      throw new TypeError(`StdLayout2 › invalid size: ${size}`);
    }
    this.#conf.cols[col].preferred = size;
    this.refresh();
    return this;
  }
  setRowSize(row, size) {
    if (row < 0 || row >= this.#conf.rows.length) {
      throw new Error(`StdLayout2 › invalid row index: ${row}`);
    }
    if (typeof size !== "number" || size < 0) {
      throw new TypeError(`StdLayout2 › invalid size: ${size}`);
    }
    this.#conf.rows[row].preferred = size;
    this.refresh();
    return this;
  }

  /** Measure min & preferred sizes (scroll boxes) per track. */
  #measure() {
    const R = this.#conf.rows.length;
    const C = this.#conf.cols.length;
    this.#measures.rows = Array.from({ length: R }, () => ({ min: 0, preferred: 0, max : 0 }));
    this.#measures.cols = Array.from({ length: C }, () => ({ min: 0, preferred: 0, max: 0 }));

    // 1 – measure items ---------------------------------------------------
    const hasStretchingRow = this.#conf.rows.length > 1 && this.#conf.rows.some(row => !row.preferred && row.stretch && row.stretch > 0);
    const hasStretchingColumn =  this.#conf.cols.length > 1 && this.#conf.cols.some(col => !col.preferred && col.stretch && col.stretch > 0);

    console.log(`hasStretchingRow: ${hasStretchingRow}`);
    console.log(`hasStretchingColumn: ${hasStretchingColumn}`);
        console.log('this.#conf', this.#conf.cols);


    for (const itm of this.#conf.items) {
      const el = typeof itm.el === "string" ? document.getElementById(itm.el) : itm.el;
      if (!el) continue;

      const s   = getComputedStyle(el);
      const minW = parseFloat(s.minWidth)  || 0;
      const minH = parseFloat(s.minHeight) || 0;
      const width = this.#conf.cols[itm.col].max || 0;
      const height = this.#conf.rows[itm.row].max || 0;

      //console.log(`col : ${itm.col} -> preferred: ${width} - ${el.style.maxWidth} - ${s.maxWidth} `, this.#measures.rows[itm.col]);
      if(hasStretchingRow&&this.#conf.rows[itm.row].preferred)
        this.#measures.rows[itm.row].preferred = this.#conf.rows[itm.row].preferred;
      if(hasStretchingColumn&&this.#conf.cols[itm.col].preferred)
        this.#measures.cols[itm.col].preferred = this.#conf.cols[itm.col].preferred;

      this.#measures.rows[itm.row].max = height;
      this.#measures.cols[itm.col].max = width;

      /* For now we only keep minima – CSS Grid will stretch automatically. */
      this.#measures.rows[itm.row].min = Math.max(this.#measures.rows[itm.row].min, minH);
      this.#measures.cols[itm.col].min = Math.max(this.#measures.cols[itm.col].min, minW);
    }
  }
  mkHandle = (i, isRow) => {
    const h = document.createElement('div');
    h.className = `stdSplitter ${isRow?'row':'col'}`;
    h.style.cursor = isRow ? 'row-resize' : 'col-resize';
    h.style.userSelect = 'none';
    h.style.touchAction = 'none';
    h.style[isRow?'gridRow':'gridColumn'] = `${i+1}`;
    h.addEventListener('pointerdown', e => this.#startDrag(e, i, isRow));
    root.appendChild(h);
  };


  #startDrag(evt, trackIndex, isRow) {
    const id = evt.pointerId;
    const sizeKey = isRow ? 'gridTemplateRows' : 'gridTemplateColumns';
    const init = root.style[sizeKey].split(' ');
    const start  = evt[isRow?'clientY':'clientX'];

    const move = e => {
      if (e.pointerId !== id) return;
      const delta = e[isRow?'clientY':'clientX'] - start;
      const v = parseFloat(init[trackIndex]);           // px, guaranteed
      init[trackIndex] = `${Math.max(24, v + delta)}px`; // clamp ≥24 px
      root.style[sizeKey] = init.join(' ');
    };

    const up = e => {
      if (e.pointerId !== id) return;
      root.releasePointerCapture(id);
      root.removeEventListener('pointermove', move);
      root.removeEventListener('pointerup',   up);
      this.emit(root, 'onResize', { track:trackIndex, rows:isRow });
    };

    root.setPointerCapture(id);
    root.addEventListener('pointermove', move);
    root.addEventListener('pointerup',   up);
  }
  

  /** Apply CSS Grid styles to root + items. */
  #apply() {
    const { root, rows, cols: columns, margins, rtl } = this.#conf;

    const trackToCss = (t, measure) => {
      if (t.size != null) return `${t.size}px`;
      //if (measure.preferred) return `${measure.preferred}px`;

      const stretch = t.stretch ?? 0;
      const min = Math.max(t.min ?? 0, measure?.min ?? 0);
      const preferred = measure.preferred ? `${measure.preferred}px` : stretch ? `${stretch}fr` : `${min}px` ;
      const max = t.max ? `${t.max}px` : preferred;


      //                min -> preferred -> max

        console.log(`stretch: ${stretch}`);
        console.log(`t.min: ${t.min??0}, c.preferred: ${measure.preferred??0}, t.max: ${t.max??0}`);
        console.log(`min: ${min}, preferred: ${preferred}, max: ${max}`);


      return `minmax(${min}px, ${preferred})`;

      if (measure.preferred) {
        console.log(`clamp(${min}px, ${measure.preferred}px, '1fr')`);
         return `minmax(${min}px, ${measure.preferred}px)`;
      }
      if (stretch)
        return `minmax(${min}px, ${preferred})`;
      else //return `${min}px`; //grid-template-rows: minmax(0px, 1fr) minmax(0px, 2fr) 80px;
        return `clamp(${min}px, ${preferred}, ${max})`;
    };

    // 1 – grid templates ---------------------------------------------------
    const buildTemplate = (tracks, measures, isRow) => {
      const parts = [];
      tracks.forEach((t, i) => {
        parts.push(trackToCss(t, measures[i]));                       // normal track
        if (this.#enableSplitters && t.resizable &&
            i < tracks.length - 1 && !tracks[i+1].resizable)
        {
          parts.push('4px');                             // 4 px splitter track
        }
      });
      return parts.join(' ');
    };
    root.style.gridTemplateRows    = buildTemplate(rows, this.#measures.rows, true);
    root.style.gridTemplateColumns = buildTemplate(columns, this.#measures.cols, false);


    // 1 – grid templates ---------------------------------------------------
    // const tpl = (tracks, measures) => tracks.map((t, i) => {
    //   const min = Math.max(t.min ?? 0, measures[i]?.min ?? 0);
    //   return t.stretch ? `minmax(${min}px, ${t.stretch}fr)` : `${min}px`;
    // }).join(" ");

    // root.style.gridTemplateRows    = tpl(rows,    this.#measures.rows);
    // root.style.gridTemplateColumns = tpl(columns, this.#measures.cols);
    root.style.gap     = `${margins.v[0]}px ${margins.h[0]}px`;
    root.style.padding = `${margins.v[1]}px ${margins.h[1]}px ${margins.v[2]}px ${margins.h[2]}px`;
    root.style.direction = rtl ? "rtl" : "ltr";

    // 2 – position items ---------------------------------------------------
    for (const itm of this.#conf.items) {
      const el = typeof itm.el === "string" ? document.getElementById(itm.el) : itm.el;
      if (!el) continue;

      Object.assign(el.style, {
        position        : "relative",      // override legacy absolute
        justifySelf : itm.hAlign ?? 'stretch',    // start | center | end | stretch
        alignSelf   : itm.vAlign ?? 'stretch',
        gridRow         : `${itm.row + 1} / span ${itm.rowSpan ?? 1}`,
        gridColumn      : `${itm.col + 1} / span ${itm.colSpan ?? 1}`,
        width           : "auto",
        height          : "auto",
        boxSizing       : "border-box"
      });
    }
  }
}
