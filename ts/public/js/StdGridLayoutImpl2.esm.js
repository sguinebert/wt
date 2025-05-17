// StdLayout2.js – Modern ES2022 rewrite of the legacy “StdLayout2” implementation
// -----------------------------------------------------------------------------
// ▸ 100 % evergreen‑browser features (ES2022+, ESM)
// ▸ No globals: the class can be imported & instantiated anywhere
// ▸ Private fields for internal state (prefixed with #)
// ▸ Pointer events & ResizeObserver for resize tracking
// ▸ Conditional (?. / ??) operators for concise fall‑backs
// -----------------------------------------------------------------------------

/* eslint-disable max-lines */

// Public API ------------------------------------------------------------------
//   import StdLayout2 from "./StdLayout2.js";
//   const layout = new StdLayout2({
//     root: document.querySelector("#layoutRoot"),
//     rows:    [ {stretch:1}, {stretch:0} ],
//     columns: [ {stretch:2}, {stretch:1} ],
//     items:   [ /* … row×col widgets … */ ],
//     rtl:     document.dir === "rtl"
//   });
//   layout.measure();
//   layout.apply();

export default class StdLayout2 {
  /* ------------------------------------------------------------------------ *
   *  Construction / configuration
   * --------------------------------------------------------------------- */
  /**
   * @param {Object} cfg – configuration object. Keys mirror the legacy ctor
   */
  constructor(cfg) {
    this.#conf = {
      root:          cfg.root      ?? (() => { throw new Error("root element required") })(),
      rows:          cfg.rows      ?? [],
      columns:       cfg.columns   ?? [],
      items:         cfg.items     ?? [],
      fitWidth:      cfg.fitWidth  ?? true,
      fitHeight:     cfg.fitHeight ?? true,
      progressive:   cfg.progressive ?? false,
      maxWidth:      cfg.maxWidth  ?? 0,
      maxHeight:     cfg.maxHeight ?? 0,
      margins: {
        horizontal: cfg.hMargins ?? [8, 8, 8],  // spacing, left, right
        vertical:   cfg.vMargins ?? [8, 8, 8]
      },
      rtl: cfg.rtl ?? false
    };

    // Private state -------------------------------------------------------
    this.#dirtyLayout   = true;   // parent/viewport changed
    this.#dirtyContent  = true;   // some item flagged dirty
    this.#measures      = { rows: [], cols: [] };  // cached preferred/min sizes
    this.#sizes         = { rows: [], cols: [] };  // computed runtime sizes

    // Quickly bail out if root is detached --------------------------------
    if (!this.#conf.root.isConnected) {
      throw new Error("Root element must be attached to DOM before instantiating StdLayout2");
    }

    // Observe size changes on root ---------------------------------------
    this.#resizeObs = new ResizeObserver(() => {
      this.#dirtyLayout = true;
      this.apply();
    });
    this.#resizeObs.observe(this.#conf.root);

    // Optional: observe child list mutations to auto‑dirty items ----------
    this.#mutObs = new MutationObserver(() => {
      this.#dirtyContent = true;
      this.measure();
      this.apply();
    });
    this.#mutObs.observe(this.#conf.root, { childList:true, subtree:true, attributes:true });
  }

  /* ------------------------------------------------------------------------ *
   *  Public API
   * --------------------------------------------------------------------- */
  /** Re‑measures the grid’s preferred / minimum sizes (cheap when nothing dirty) */
  measure() {
    if (!this.#dirtyLayout && !this.#dirtyContent) return;

    const { rows, columns } = this.#conf;
    const itemCountR = rows.length;
    const itemCountC = columns.length;

    // reset measurement caches
    this.#measures.rows = Array(itemCountR).fill({ pref:0, min:0 });
    this.#measures.cols = Array(itemCountC).fill({ pref:0, min:0 });

    // iterate items -------------------------------------------------------
    for (const itm of this.#conf.items) {
      const el = typeof itm.el === "string" ? document.getElementById(itm.el) : itm.el;
      if (!el) continue;

      // measure preferred & min sizes along both axes -------------------
      const m = this.#measureElement(el);
      const r = itm.row ?? 0, c = itm.col ?? 0;

      // accumulate per‑row / per‑column maxima --------------------------
      this.#measures.rows[r] = {
        pref: Math.max(this.#measures.rows[r].pref, m.pref.h),
        min:  Math.max(this.#measures.rows[r].min,  m.min.h)
      };
      this.#measures.cols[c] = {
        pref: Math.max(this.#measures.cols[c].pref, m.pref.w),
        min:  Math.max(this.#measures.cols[c].min,  m.min.w)
      };
    }

    this.#dirtyContent = false;
  }

  /** Computes & applies sizes/positions to DOM */
  apply() {
    this.measure();

    const rootRect = this.#conf.root.getBoundingClientRect();
    const availW   = rootRect.width;
    const availH   = rootRect.height;

    // compute sizes per axis --------------------------------------------
    this.#sizes.cols = this.#distribute(availW, this.#measures.cols, this.#conf.columns);
    this.#sizes.rows = this.#distribute(availH, this.#measures.rows, this.#conf.rows);

    // position items -----------------------------------------------------
    for (const itm of this.#conf.items) {
      const el = typeof itm.el === "string" ? document.getElementById(itm.el) : itm.el;
      if (!el) continue;

      const r = itm.row ?? 0, c = itm.col ?? 0;
      const x = this.#sumUntil(this.#sizes.cols, c) + this.#conf.margins.horizontal[1];
      const y = this.#sumUntil(this.#sizes.rows, r) + this.#conf.margins.vertical[1];

      const w = this.#sizes.cols[c];
      const h = this.#sizes.rows[r];

      Object.assign(el.style, {
        position: "absolute",
        inlineSize : `${w}px`,
        blockSize  : `${h}px`,
        insetInlineStart: `${x}px`,
        insetBlockStart:  `${y}px`
      });
    }

    this.#dirtyLayout = false;
  }

  #applyGrid() { // Apply CSS Grid layout to the root element
    const root = this.#conf.root;
  
    // Define grid template rows and columns
    const rows = this.#conf.rows.map(r => r.stretch ? `minmax(0, ${r.stretch}fr)` : `${r.min}px`).join(" ");
    const cols = this.#conf.columns.map(c => c.stretch ? `minmax(0, ${c.stretch}fr)` : `${c.min}px`).join(" ");
  
    // Apply CSS Grid styles to the root element
    Object.assign(root.style, {
      display: "grid",
      gridTemplateRows: rows,
      gridTemplateColumns: cols,
      gap: `${this.#conf.margins.horizontal[0]}px ${this.#conf.margins.vertical[0]}px`,
    });
  
    // Position items using grid-row and grid-column
    for (const itm of this.#conf.items) {
      const el = typeof itm.el === "string" ? document.getElementById(itm.el) : itm.el;
      if (!el) continue;
  
      el.style.gridRow = itm.row + 1; // CSS Grid is 1-based
      el.style.gridColumn = itm.col + 1;
    }
  }

  /** Manually mark an element dirty & request a re‑layout */
  markDirty(el) {
    const id = typeof el === "string" ? el : el.id;
    const item = this.#conf.items.find(i => i.el === id || i.el === el);
    if (item) {
      this.#dirtyContent = true;
      queueMicrotask(() => { this.apply(); });
    }
  }

  dispose() {
    this.#resizeObs.disconnect();
    this.#mutObs.disconnect();
  }

  /* ------------------------------------------------------------------------ *
   *  Private helpers
   * --------------------------------------------------------------------- */
  #conf;              // runtime configuration (see constructor)
  #dirtyLayout;       // parent size changed
  #dirtyContent;      // some child changed
  #measures;          // preferred/min sizes cache
  #sizes;             // computed widths/heights per track
  #resizeObs;         // ResizeObserver instance for root
  #mutObs;            // MutationObserver instance

  /** Measures preferred & min content sizes of element (border‑box) */
  #measureElement(el) {
    const style = getComputedStyle(el);
    const minW = parseFloat(style.minWidth ) || 0;
    const minH = parseFloat(style.minHeight) || 0;

    // Preferred size: if width/height set → use it, else scroll / bbox size
    const prefW = (parseFloat(style.width ) || el.scrollWidth ) + minW;
    const prefH = (parseFloat(style.height) || el.scrollHeight) + minH;

    return {
      min:  { w: minW, h: minH },
      pref: { w: prefW, h: prefH }
    };
  }

  /** Simple linear track size distribution with stretch factors */
  #distribute(avail, tracks, cfgTracks) {
    const margins = this.#conf.margins.horizontal; // same array structure for v & h
    const spacing = margins[0];

    const count   = tracks.length;
    const result  = Array(count).fill(0);

    // Baseline: min sizes -------------------------------------------------
    let used = (count - 1) * spacing + margins[1] + margins[2];
    tracks.forEach((t, i) => {
      result[i] = t.min;
      used     += t.min;
    });

    let remaining = Math.max(0, avail - used);

    // Pass 2: give non‑stretchables up to their preferred --------------
    tracks.forEach((t, i) => {
      const stretch = cfgTracks[i]?.stretch ?? 0;
      if (!stretch) {
        const delta = Math.min(remaining, t.pref - result[i]);
        result[i]  += delta;
        remaining  -= delta;
      }
    });

    // Pass 3: distribute leftovers proportionally among stretchables ----
    const totalStretch = cfgTracks.reduce((s,c)=>s+(c.stretch||0),0) || 1;
    tracks.forEach((t,i) => {
      const stretch = cfgTracks[i]?.stretch ?? 0;
      if (stretch) {
        const share = Math.round(remaining * (stretch / totalStretch));
        result[i] += share;
      }
    });

    return result;
  }

  #sumUntil(arr, idx) {
    let s = 0; for (let i=0;i<idx;i++) s += arr[i];
    return s + (idx>0 ? this.#conf.margins.horizontal[0]*idx : 0);
  }
}
