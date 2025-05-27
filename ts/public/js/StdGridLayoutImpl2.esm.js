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

    this.#initSplitterHandles();

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
  setTrackVisible(index, visible, isRow = true) {
    const track = isRow ? this.#conf.rows[index] : this.#conf.cols[index];
    track.hidden = !visible;
    return this.refresh();
  }
  setTrackStretch(index, stretch, isRow = true) {
    const track = isRow ? this.#conf.rows[index] : this.#conf.cols[index];
    if (typeof stretch !== "number" || stretch < 0) {
      throw new TypeError(`StdLayout2 › invalid stretch value: ${stretch}`);
    }
    track.stretch = stretch;
    return this.refresh();
  }
  setTrackMin(index, min, isRow = true) {
    const track = isRow ? this.#conf.rows[index] : this.#conf.cols[index];
    if (typeof min !== "number" || min < 0) {
      throw new TypeError(`StdLayout2 › invalid minimum size: ${min}`);
    }
    track.min = min;
    return this.refresh();
  }
  setTrackMax(index, max, isRow = true) {
    const track = isRow ? this.#conf.rows[index] : this.#conf.cols[index];
    if (typeof max !== "number" || max < 0) {
      throw new TypeError(`StdLayout2 › invalid maximum size: ${max}`);
    }
    track.max = max;
    return this.refresh();
  }
  setTrackAutoFit(index, autoFit, isRow = true) {
    const track = isRow ? this.#conf.rows[index] : this.#conf.cols[index];
    if (typeof autoFit !== "string" || !["min", "max", "fit-content"].includes(autoFit)) {
      throw new TypeError(`StdLayout2 › invalid auto-fit value: ${autoFit}`);
    }
    track.autoFit = autoFit;
    return this.refresh();
  }
  setTrackSize(index, size, isRow = true) {
    const track = isRow ? this.#conf.rows[index] : this.#conf.cols[index];
    if (typeof size !== "number" || size < 0) {
      throw new TypeError(`StdLayout2 › invalid size: ${size}`);
    }
    track.size = size;
    return this.refresh();
  }

  /** Disconnect observers and free references */
  dispose() {
    this.#resizeObs.disconnect();
    this.#mutObs.disconnect();
    this.#conf.items.length = 0;
  }
  saveLayout() {
    return {
      rows: this.#conf.rows.map(r => ({...r})),
      cols: this.#conf.cols.map(c => ({...c}))
    };
  }
  restoreLayout(savedLayout) {
    this.#conf.rows = savedLayout.rows.map(r => ({...r}));
    this.#conf.cols = savedLayout.cols.map(c => ({...c}));
    return this.refresh();
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
    const rstretch = this.#conf.rows.length > 1 && this.#conf.rows.some(row => !row.preferred && row.stretch && row.stretch > 0);
    const cstretch =  this.#conf.cols.length > 1 && this.#conf.cols.some(col => !col.preferred && col.stretch && col.stretch > 0);

    // console.log(`hasStretchingRow: ${hasStretchingRow}`);
    // console.log(`hasStretchingColumn: ${hasStretchingColumn}`);
    //     console.log('this.#conf', this.#conf.cols);


    for (const itm of this.#conf.items) {
      const el = typeof itm.el === "string" ? document.getElementById(itm.el) : itm.el;
      if (!el) continue;

      const s   = getComputedStyle(el);
      const minW = parseFloat(s.minWidth)  || 0;
      const minH = parseFloat(s.minHeight) || 0;
      const width = this.#conf.cols[itm.col].max || 0;
      const height = this.#conf.rows[itm.row].max || 0;

      //console.log(`col : ${itm.col} -> preferred: ${width} - ${el.style.maxWidth} - ${s.maxWidth} `, this.#measures.rows[itm.col]);
      if(rstretch&&this.#conf.rows[itm.row].preferred)
        this.#measures.rows[itm.row].preferred = this.#conf.rows[itm.row].preferred;
      if(cstretch&&this.#conf.cols[itm.col].preferred)
        this.#measures.cols[itm.col].preferred = this.#conf.cols[itm.col].preferred;

      this.#measures.rows[itm.row].max = height;
      this.#measures.cols[itm.col].max = width;

      /* For now we only keep minima – CSS Grid will stretch automatically. */
      this.#measures.rows[itm.row].min = Math.max(this.#measures.rows[itm.row].min, minH);
      this.#measures.cols[itm.col].min = Math.max(this.#measures.cols[itm.col].min, minW);
    }
  }
  /** Initialize splitter handles once */
  #initSplitterHandles() {
    let insertedrows = 1;
    let insertedcols = 1;
    const { rows, cols, items } = this.#conf;
    for (let i = 0; i < rows.length - 1; i++) {
      if (rows[i].resizable) {
        insertedrows++;
        rows[i].resizable = this.mkHandle(i, true); // Create row splitter
        for (const itm of items) {
          if (itm.row > i) // Only adjust items that are after the current row
            itm.arow = itm.row + insertedrows; // Adjust row index for splitters  
        }
      }
    }
    for (let i = 0; i < cols.length - 1; i++) {
      if (cols[i].resizable) {
        insertedcols++;
        cols[i].resizable = this.mkHandle(i, false); // Create column splitter
        for (const itm of items) {
          //console.log(`itm.col: ${itm.col}, i: ${i}, insertedcols: ${insertedcols}`);
          if (itm.col > i) // Only adjust items that are after the current column
            itm.acol = itm.col + insertedcols; // Adjust column index for splitters  
        }
      }
    }  
  }
  /** Create the div splitters. */
  mkHandle = (i, isRow) => {
    //const gridLine = i + 1 + resizableBefore; // +1 because grid lines start at 1
    const h = document.createElement('div');
    h.className = `stdSplitter ${isRow?'row':'col'}`;
    h.style.cursor = isRow ? 'row-resize' : 'col-resize';
    h.style.userSelect = 'none';
    h.style.touchAction = 'none';
    h.style.margin = isRow ? '-2px 0 0 0' : '0 -2px 0 0'; // overlap gap
    h.style.zIndex = '2'; // above grid items
    h.style[isRow ? 'height' : 'width'] = '4px';
    h.style[isRow? 'gridRow' : 'gridColumn'] = `${i+2} / span 1`; // Position in the grid
    h.style[!isRow ? 'gridRow' : 'gridColumn'] = `1 / -1`;
    h.addEventListener('pointerdown', e => this.#startDrag(e, i, isRow));
    this.#conf.root.appendChild(h);
    return h; 
  }
  /**
 * Get the actual computed size of a track (row/column)
 * @param {number} trackIndex - Index of the track
 * @param {boolean} isRow - Whether this is a row or column
 * @returns {number} The computed size in pixels
 */
  #getComputedTrackSize(trackIndex, isRow) {
   
    // Strategy 1: Find an element that occupies exactly this track
    const itemsInTrack = this.#conf.items.filter(item => {
      if (isRow) {
        return item.row === trackIndex && (item.rowSpan === undefined || item.rowSpan === 1);
      } else {
        return item.col === trackIndex && (item.colSpan === undefined || item.colSpan === 1);
      }
    });
    
    for (const item of itemsInTrack) {
      const el = typeof item.el === "string" ? document.getElementById(item.el) : item.el;
      if (el) {
        const rect = el.getBoundingClientRect();
        //console.log(`Computed size for track ${trackIndex} (${isRow ? 'row' : 'column'}):`, rect);
        // Return the height for rows or width for columns
        return isRow ? rect.height : rect.width;
      }
    }
    console.warn(`No item found for track ${trackIndex} (${isRow ? 'row' : 'column'}). Using default size.`);
    // Strategy 2: Create a temporary measuring element
    const { root } = this.#conf;
    const temp = document.createElement('div');
    temp.style.position = 'relative';
    temp.style.visibility = 'hidden';
    temp.style.height = isRow ? '100%' : '0';
    temp.style.width = isRow ? '0' : '100%';
    
    // Place it in the correct grid cell
    if (isRow) {
      temp.style.gridRow = `${trackIndex + 1} / span 1`;
      temp.style.gridColumn = '1 / -1'; // Span all columns
    } else {
      temp.style.gridColumn = `${trackIndex + 1} / span 1`;
      temp.style.gridRow = '1 / -1'; // Span all rows
    }
    
    root.appendChild(temp);
    
    // Measure
    const rect = temp.getBoundingClientRect();
    const size = isRow ? rect.height : rect.width;
    
    // Clean up
    root.removeChild(temp);   
    return size;
    return 0; // Fallback if no item found
  }
  /** Start dragging a splitter handle. */
  #startDrag(evt, trackIndex, isRow) {
    const { root } = this.#conf;
    const id = evt.pointerId;
    const sizeKey = isRow ? 'gridTemplateRows' : 'gridTemplateColumns';
    const init = root.style[sizeKey].split(' ');
    const start  = evt[isRow?'clientY':'clientX'];
    let target = this.#conf[isRow?'rows':'cols'][trackIndex]; // Get the target track configuration

    // Get the actual computed size by measuring an element in that column
    let initialSize = target.size || this.#getComputedTrackSize(trackIndex, isRow);

    const move = e => {
      if (e.pointerId !== id) return;
      const delta = e[isRow?'clientY':'clientX'] - start;
      const newSize = Math.max(24, initialSize + delta);
      this.#conf[isRow?'rows':'cols'][trackIndex].size = newSize; // Update the size in the style

      // const v = parseFloat(init[trackIndex]);           // px, guaranteed
      // console.log(`Resizing ${isRow ? 'row' : 'column'} ${trackIndex} by ${delta}px (from ${v}px)`);
      // init[trackIndex] = `${Math.max(24, v + delta)}px`; // clamp ≥24 px
      // root.style[sizeKey] = init.join(' ');
      //console.log(`Resizing ${isRow ? 'row' : 'column'} ${trackIndex} to ${init[trackIndex]}`);
      this.refresh(); // Re-apply the layout after resizing
    };

    const up = e => {
      if (e.pointerId !== id) return;
      root.releasePointerCapture(id);
      root.removeEventListener('pointermove', move);
      root.removeEventListener('pointerup',   up);
      //this.emit(root, 'onResize', { track:trackIndex, rows:isRow });
    };

    root.setPointerCapture(id);
    root.addEventListener('pointermove', move);
    root.addEventListener('pointerup',   up);
  }
  

  /** Apply CSS Grid styles to root + items. */
  #apply() {
    const { root, rows, cols: columns, margins, rtl } = this.#conf;

    const trackToCss = (t, measure) => {
      if (t.hidden) return '0px'; // Hidden tracks take no space
      if (t.size != null) return `${t.size}px`;

      const stretch = t.stretch ?? 0;
      const min = Math.max(t.min ?? 0, measure?.min ?? 0);
      const preferred = measure.preferred ? `${measure.preferred}px` : stretch ? `${stretch}fr` : `${min}px` ;

      // Use max if specified, otherwise use preferred as maximum
      if (t.max != null) {
        return `minmax(${min}px, clamp(${min}px, ${preferred}, ${t.max}px))`;
      }
      if (t.autoFit) {
        const autoValue = t.autoFit === 'min' ? 'min-content' : 
                          t.autoFit === 'max' ? 'max-content' :
                          t.autoFit; // Allow direct values like 'fit-content(300px)'
        
        if (t.min != null) {
          return `minmax(${min}px, ${autoValue})`;
        }
        return autoValue;
      } 

      return `minmax(${min}px, ${preferred})`;
    };

    // 1 – grid templates ---------------------------------------------------
    const buildTemplate = (tracks, measures, isRow) => {
      const parts = [];
      tracks.forEach((t, i) => {
        parts.push(trackToCss(t, measures[i]));                       // normal track
        if (t.resizable && i < tracks.length - 1)
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
        margin      : itm.margin ?? '',
        padding     : itm.padding ?? '',
        zIndex      : itm.zIndex ?? 'auto',
        overflow    : itm.overflow ?? '',
        gridRow         : `${itm.arow??itm.row + 1} / span ${itm.rowSpan ?? 1}`,
        gridColumn      : `${itm.acol??itm.col + 1} / span ${itm.colSpan ?? 1}`,
        width           : "auto",
        height          : "auto",
        boxSizing       : "border-box"
      });
    }
  }
}
