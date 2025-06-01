/* eslint-disable max-lines */
// StdLayout2.ts – Modern ES 2022+ TypeScript rewrite of the legacy “StdLayout2”
// -----------------------------------------------------------------------------
// • 100 % evergreen‑browser features (ES2022+, ESM)
// • No globals: simply `import StdLayout2 from "./StdLayout2.js"`
// • Private class fields for internal state (native `#` syntax)
// • ResizeObserver + MutationObserver for live re‑flow
// • Pure‑CSS Grid for layout ‑‑ no hand‑crafted absolute maths anymore
// -----------------------------------------------------------------------------

/* ---------------------------------------------------------------------------
 * Helper types
 * ------------------------------------------------------------------------ */

export type AutoFitMode = "min" | "max" | "fit-content";

export interface Track {
  stretch?: number;
  min?: number;
  max?: number;
  hidden?: boolean;
  resizable?: boolean | HTMLElement;      // true → create splitter, HTMLElement once created
  autoFit?: AutoFitMode;                 // "min" | "max" | "fit-content"
  size?: number;                         // explicit pixel size (set via splitter or API)
  preferred?: number;                    // explicit preferred size (px)
}

export interface Item {
  el: HTMLElement | string;              // element or its id
  row?: number;
  col?: number;
  rowSpan?: number;
  colSpan?: number;
  hAlign?: "start" | "center" | "end" | "stretch";
  vAlign?: "start" | "center" | "end" | "stretch";
  margin?: string;
  padding?: string;
  zIndex?: number;
  overflow?: string;
  // adjusted row / col positions once splitters are inserted
  arow?: number;
  acol?: number;
}

export interface StdLayout2Config {
  root: HTMLElement;
  rows?: Track[];
  columns?: Track[];
  items?: Item[];
  rtl?: boolean;
  hMargins?: [number, number, number];    // [gap,left,right]
  vMargins?: [number, number, number];    // [gap,top,bottom]
  splitters?: boolean;                    // enable interactive splitters?
}

interface TrackMeasures { min: number; preferred: number; max: number; }

/* ---------------------------------------------------------------------------
 * StdLayout2
 * ------------------------------------------------------------------------ */
export default class StdLayout2 {
  /* ───────────────────────────── private fields ────────────────────────── */
  #busy = false;                         // true while measuring + applying
  #enableSplitters = false;

  #conf!: {
    root: HTMLElement;
    rows: Track[];
    cols: Track[];
    items: Item[];
    rtl: boolean;
    margins: { h: [number, number, number]; v: [number, number, number] };
  };

  #measures: { rows: TrackMeasures[]; cols: TrackMeasures[] } = { rows: [], cols: [] };
  #resizeObs!: ResizeObserver;
  #mutObs!: MutationObserver;

  /* ────────────────────────────── constructor ──────────────────────────── */
  constructor(cfg: Partial<StdLayout2Config> = {}) {
    /* ---------------------------- runtime config ------------------------- */
    this.#conf = {
      root    : cfg.root ?? (() => { throw new Error("root element required"); })(),
      rows    : cfg.rows      ?? [],
      cols    : cfg.columns   ?? [],
      items   : cfg.items     ?? [],
      rtl     : cfg.rtl       ?? false,
      margins : {
        h: cfg.hMargins ?? [8, 8, 8],
        v: cfg.vMargins ?? [8, 8, 8]
      }
    };

    this.#enableSplitters = cfg.splitters ?? false;

    if (!this.#conf.root.isConnected) {
      throw new Error("StdLayout2 › root element must be attached to the DOM");
    }

    this.#initSplitterHandles();

    /* ----------------------------- observers ---------------------------- */
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

  /* ───────────────────────────── public API ────────────────────────────── */
  enableSplitters(on = true): this { this.#enableSplitters = !!on; return this.refresh(); }

  addRow(track: Track = { stretch: 1 }): this  { this.#conf.rows.push(track); return this.refresh(); }
  addColumn(track: Track = { stretch: 1 }): this { this.#conf.cols.push(track); return this.refresh(); }

  addItem(item: Item): this                 { return this.#editItem("add",      item) as this; }
  removeItem(el: HTMLElement | string): this { return this.#editItem("remove",   el)   as this; }
  updateItem(el: HTMLElement | string, u: Partial<Item>): this { return this.#editItem("update",   el, u) as this; }

  /** Flip RTL/LTR on the fly */
  setRTL(val = true): this { this.#conf.rtl = !!val; return this.refresh(); }

  /** Force re‑measure + re‑layout (idempotent / cheap when nothing changed) */
  refresh(): this { if (this.#busy) return this; this.#busy = true; try { this.#measure(); this.#apply(); } finally { this.#busy = false; } return this; }

  setTrackVisible(index: number, visible: boolean, isRow = true): this {
    const track = isRow ? this.#conf.rows[index] : this.#conf.cols[index];
    track.hidden = !visible; return this.refresh();
  }

  setTrackStretch(index: number, stretch: number, isRow = true): this {
    const track = isRow ? this.#conf.rows[index] : this.#conf.cols[index];
    if (typeof stretch !== "number" || stretch < 0) throw new TypeError(`StdLayout2 › invalid stretch value: ${stretch}`);
    track.stretch = stretch; return this.refresh();
  }

  setTrackMin(index: number, min: number, isRow = true): this {
    const track = isRow ? this.#conf.rows[index] : this.#conf.cols[index];
    if (typeof min !== "number" || min < 0) throw new TypeError(`StdLayout2 › invalid minimum size: ${min}`);
    track.min = min; return this.refresh();
  }

  setTrackMax(index: number, max: number, isRow = true): this {
    const track = isRow ? this.#conf.rows[index] : this.#conf.cols[index];
    if (typeof max !== "number" || max < 0) throw new TypeError(`StdLayout2 › invalid maximum size: ${max}`);
    track.max = max; return this.refresh();
  }

  setTrackAutoFit(index: number, autoFit: AutoFitMode, isRow = true): this {
    const track = isRow ? this.#conf.rows[index] : this.#conf.cols[index];
    if (!["min", "max", "fit-content"].includes(autoFit)) throw new TypeError(`StdLayout2 › invalid auto-fit value: ${autoFit}`);
    track.autoFit = autoFit; return this.refresh();
  }

  setTrackSize(index: number, size: number, isRow = true): this {
    const track = isRow ? this.#conf.rows[index] : this.#conf.cols[index];
    if (typeof size !== "number" || size < 0) throw new TypeError(`StdLayout2 › invalid size: ${size}`);
    track.size = size; return this.refresh();
  }

  /** Disconnect observers and free references */
  dispose(): void {
    this.#resizeObs.disconnect();
    this.#mutObs.disconnect();
    this.#conf.items.length = 0;
  }

  saveLayout(): { rows: Track[]; cols: Track[] } {
    return {
      rows: this.#conf.rows.map(r => ({ ...r })),
      cols: this.#conf.cols.map(c => ({ ...c }))
    };
  }

  restoreLayout(savedLayout: { rows: Track[]; cols: Track[] }): this {
    this.#conf.rows = savedLayout.rows.map(r => ({ ...r }));
    this.#conf.cols = savedLayout.cols.map(c => ({ ...c }));
    return this.refresh();
  }

  /* ──────────────────────────── private API ───────────────────────────── */

  /** Edits the item array in a single place (add/update/remove). */
  #editItem(action: "add" | "remove" | "update", elOrCfg: Item | HTMLElement | string, updates: Partial<Item> = {}): this {
    if (action === "add") {
      const itm: Item = {
        el      : (elOrCfg as Item).el,
        row     : (elOrCfg as Item).row     ?? 0,
        col     : (elOrCfg as Item).col     ?? 0,
        rowSpan : (elOrCfg as Item).rowSpan ?? 1,
        colSpan : (elOrCfg as Item).colSpan ?? 1
      };
      this.#ensureGridSize(itm);
      this.#conf.items.push(itm);
    } else {
      const id   = typeof elOrCfg === "string" ? elOrCfg : (elOrCfg as HTMLElement).id;
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
  #ensureGridSize(item: Item): void {
    const wantRows = (item.row ?? 0) + (item.rowSpan ?? 1);
    const wantCols = (item.col ?? 0) + (item.colSpan ?? 1);
    while (this.#conf.rows.length < wantRows) this.#conf.rows.push({ stretch: 1 });
    while (this.#conf.cols.length < wantCols) this.#conf.cols.push({ stretch: 1 });
  }

  setColSize(col: number, size: number): this {
    if (col < 0 || col >= this.#conf.cols.length) throw new Error(`StdLayout2 › invalid column index: ${col}`);
    if (typeof size !== "number" || size < 0) throw new TypeError(`StdLayout2 › invalid size: ${size}`);
    this.#conf.cols[col].preferred = size; return this.refresh();
  }

  setRowSize(row: number, size: number): this {
    if (row < 0 || row >= this.#conf.rows.length) throw new Error(`StdLayout2 › invalid row index: ${row}`);
    if (typeof size !== "number" || size < 0) throw new TypeError(`StdLayout2 › invalid size: ${size}`);
    this.#conf.rows[row].preferred = size; return this.refresh();
  }

  /** Measure min & preferred sizes (scroll boxes) per track. */
  #measure(): void {
    const R = this.#conf.rows.length;
    const C = this.#conf.cols.length;
    this.#measures.rows = Array.from({ length: R }, () => ({ min: 0, preferred: 0, max : 0 }));
    this.#measures.cols = Array.from({ length: C }, () => ({ min: 0, preferred: 0, max : 0 }));

    const rstretch = R > 1 && this.#conf.rows.some(row => !row.preferred && row.stretch && row.stretch > 0);
    const cstretch = C > 1 && this.#conf.cols.some(col => !col.preferred && col.stretch && col.stretch > 0);

    for (const itm of this.#conf.items) {
      const el = typeof itm.el === "string" ? document.getElementById(itm.el) : itm.el;
      if (!el) continue;

      const s       = getComputedStyle(el);
      const minW    = parseFloat(s.minWidth)  || 0;
      const minH    = parseFloat(s.minHeight) || 0;
      const width   = this.#conf.cols[itm.col ?? 0].max || 0;
      const height  = this.#conf.rows[itm.row ?? 0].max || 0;

      if (rstretch && this.#conf.rows[itm.row ?? 0].preferred)
        this.#measures.rows[itm.row ?? 0].preferred = this.#conf.rows[itm.row ?? 0].preferred!;
      if (cstretch && this.#conf.cols[itm.col ?? 0].preferred)
        this.#measures.cols[itm.col ?? 0].preferred = this.#conf.cols[itm.col ?? 0].preferred!;

      this.#measures.rows[itm.row ?? 0].max = height;
      this.#measures.cols[itm.col ?? 0].max = width;

      this.#measures.rows[itm.row ?? 0].min = Math.max(this.#measures.rows[itm.row ?? 0].min, minH);
      this.#measures.cols[itm.col ?? 0].min = Math.max(this.#measures.cols[itm.col ?? 0].min, minW);
    }
  }

  /** Initialize splitter handles once */
  #initSplitterHandles(): void {
    let insertedRows = 1;
    let insertedCols = 1;
    const { rows, cols, items } = this.#conf;

    for (let i = 0; i < rows.length - 1; i++) {
      if (rows[i].resizable) {
        insertedRows++;
        rows[i].resizable = this.mkHandle(i, true); // Create row splitter
        for (const itm of items) {
          if ((itm.row ?? 0) > i) itm.arow = (itm.row ?? 0) + insertedRows; // Adjust row index
        }
      }
    }
    for (let i = 0; i < cols.length - 1; i++) {
      if (cols[i].resizable) {
        insertedCols++;
        cols[i].resizable = this.mkHandle(i, false); // Create column splitter
        for (const itm of items) {
          if ((itm.col ?? 0) > i) itm.acol = (itm.col ?? 0) + insertedCols; // Adjust column index
        }
      }
    }
  }

  /** Create the splitter handle element. */
  mkHandle = (i: number, isRow: boolean): HTMLElement => {
    const h = document.createElement("div");
    h.className = `stdSplitter ${isRow ? "row" : "col"}`;
    h.style.cursor = isRow ? "row-resize" : "col-resize";
    h.style.userSelect = "none";
    h.style.touchAction = "none";
    h.style.margin = isRow ? "-2px 0 0 0" : "0 -2px 0 0";
    h.style.zIndex = "2";
    h.style[isRow ? "height" : "width"] = "4px";
    (h.style as any)[isRow ? "gridRow" : "gridColumn"] = `${i + 2} / span 1`;
    (h.style as any)[!isRow ? "gridRow" : "gridColumn"] = "1 / -1";
    h.addEventListener("pointerdown", e => this.#startDrag(e as PointerEvent, i, isRow));
    this.#conf.root.appendChild(h);
    return h;
  };

  /** Get the actual computed size of a track (row/column). */
  #getComputedTrackSize(trackIndex: number, isRow: boolean): number {
    const itemsInTrack = this.#conf.items.filter(item => {
      if (isRow) {
        return (item.row ?? 0) === trackIndex && (item.rowSpan === undefined || item.rowSpan === 1);
      } else {
        return (item.col ?? 0) === trackIndex && (item.colSpan === undefined || item.colSpan === 1);
      }
    });

    for (const item of itemsInTrack) {
      const el = typeof item.el === "string" ? document.getElementById(item.el) : item.el;
      if (el) {
        const rect = el.getBoundingClientRect();
        return isRow ? rect.height : rect.width;
      }
    }

    const { root } = this.#conf;
    const temp = document.createElement("div");
    temp.style.position = "relative";
    temp.style.visibility = "hidden";
    temp.style.height = isRow ? "100%" : "0";
    temp.style.width = isRow ? "0" : "100%";

    if (isRow) {
      (temp.style as any).gridRow = `${trackIndex + 1} / span 1`;
      (temp.style as any).gridColumn = "1 / -1";
    } else {
      (temp.style as any).gridColumn = `${trackIndex + 1} / span 1`;
      (temp.style as any).gridRow = "1 / -1";
    }

    root.appendChild(temp);
    const rect = temp.getBoundingClientRect();
    const size = isRow ? rect.height : rect.width;
    root.removeChild(temp);
    return size;
  }

  /** Start dragging a splitter handle. */
  #startDrag(evt: PointerEvent, trackIndex: number, isRow: boolean): void {
    const { root } = this.#conf;
    const id = evt.pointerId;
    const start = isRow ? evt.clientY : evt.clientX;
    let target = this.#conf[isRow ? "rows" : "cols"][trackIndex];
    let initialSize = target.size ?? this.#getComputedTrackSize(trackIndex, isRow);

    const move = (e: PointerEvent): void => {
      if (e.pointerId !== id) return;
      const delta = (isRow ? e.clientY : e.clientX) - start;
      const newSize = Math.max(24, initialSize + delta);
      this.#conf[isRow ? "rows" : "cols"][trackIndex].size = newSize;
      this.refresh();
    };

    const up = (e: PointerEvent): void => {
      if (e.pointerId !== id) return;
      root.releasePointerCapture(id);
      root.removeEventListener("pointermove", move);
      root.removeEventListener("pointerup", up);
    };

    root.setPointerCapture(id);
    root.addEventListener("pointermove", move);
    root.addEventListener("pointerup", up);
  }

  /** Apply CSS Grid styles to root + items. */
  #apply(): void {
    const { root, rows, cols: columns, margins, rtl } = this.#conf;

    const trackToCss = (t: Track, measure: TrackMeasures | undefined): string => {
      if (t.hidden) return "0px";
      if (t.size != null) return `${t.size}px`;

      const stretch = t.stretch ?? 0;
      const min = Math.max(t.min ?? 0, measure?.min ?? 0);
      const preferred = measure?.preferred ? `${measure.preferred}px` : stretch ? `${stretch}fr` : `${min}px`;

      if (t.max != null) {
        return `minmax(${min}px, clamp(${min}px, ${preferred}, ${t.max}px))`;
      }
      if (t.autoFit) {
        const autoValue = t.autoFit === "min" ? "min-content" : t.autoFit === "max" ? "max-content" : t.autoFit;
        if (t.min != null) return `minmax(${min}px, ${autoValue})`;
        return autoValue;
      }
      return `minmax(${min}px, ${preferred})`;
    };

    const buildTemplate = (tracks: Track[], measures: TrackMeasures[], isRow: boolean): string => {
      const parts: string[] = [];
      tracks.forEach((t, i) => {
        parts.push(trackToCss(t, measures[i]));
        if (t.resizable && i < tracks.length - 1) parts.push("4px"); // splitter track
      });
      return parts.join(" ");
    };

    root.style.gridTemplateRows    = buildTemplate(rows, this.#measures.rows, true);
    root.style.gridTemplateColumns = buildTemplate(columns, this.#measures.cols, false);

    root.style.gap     = `${margins.v[0]}px ${margins.h[0]}px`;
    root.style.padding = `${margins.v[1]}px ${margins.h[1]}px ${margins.v[2]}px ${margins.h[2]}px`;
    root.style.direction = rtl ? "rtl" : "ltr";

    // Position items ------------------------------------------------------
    for (const itm of this.#conf.items) {
      const el = typeof itm.el === "string" ? document.getElementById(itm.el) : itm.el;
      if (!el) continue;

      Object.assign(el.style, {
        position   : "relative",
        justifySelf: itm.hAlign ?? "stretch",
        alignSelf  : itm.vAlign ?? "stretch",
        margin     : itm.margin ?? "",
        padding    : itm.padding ?? "",
        zIndex     : itm.zIndex ?? "auto",
        overflow   : itm.overflow ?? "",
        gridRow    : `${(itm.arow ?? itm.row ?? 0) + 1} / span ${itm.rowSpan ?? 1}`,
        gridColumn : `${(itm.acol ?? itm.col ?? 0) + 1} / span ${itm.colSpan ?? 1}`,
        width      : "auto",
        height     : "auto",
        boxSizing  : "border-box"
      } as CSSStyleDeclaration);
    }
  }
}
