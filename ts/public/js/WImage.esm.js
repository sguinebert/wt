// WImage.js – no macro, no globals, just a tiny class.
/* eslint no-multi-assign:0 */

const CHUNK_SIZE   = 50;   // rows processed per frame
const WAIT_TIMEOUT = 100;  // ms (debounce while user is zooming / panning)

/**
 * @param {HTMLAreaElement} area         – `<area>` that will receive new coords
 * @param {number[]}        raw          – x1,y1,x2,y2,… [,r]  (last number = r for circles)
 * @param {Function}        multiplyFn   – (transform, [x,y]) => [nx,ny]
 * @param {string}          transformStr – current transform *stringified*
 * @returns {void}
 */
function applyCoords(area, raw, multiplyFn, transformStr) {
  const out = [];
  for (let i = 0; i < raw.length - 1; i += 2) {
    const [x, y] = multiplyFn(transformStr, raw.slice(i, i + 2));
    out.push(Math.round(x), Math.round(y));
  }
  if (raw.length & 1) out.push(raw.at(-1));         // radius for circle
  area.coords = out.join(',');
}

/** Light and fast image-map updater. */
export default class WImage {
  /**
   * @param {object}           opts
   * @param {HTMLElement}      opts.element         – container (not used but kept for parity)
   * @param {() => string}     opts.getTransformStr – returns a *stable* string key for current transform
   * @param {(t: string, p:[number,number])=>[number,number]}
   *                                              opts.multiply     – math helper from gfxUtils
   */
  constructor({ element, getTransformStr, multiply }) {
    element.wtObj = this;            // original API compatibility

    this._getTransformStr = getTransformStr;
    this._mult            = multiply;

    this._areas   = [];              // [[ HTMLAreaElement, number[] ], …]
    this._transform = '';
    this._row      = 0;

    this._raf   = 0;                 // requestAnimationFrame id
    this._timer = 0;                 // setTimeout id
    this._lastChange = 0;
  }

  /** Replace the whole map (JSON produced by the server). */
  setAreaCoordsJSON(json) {
    this._areas     = json ?? [];
    this._transform = '';            // force immediate rebuild
    this._row       = 0;
    this._kick();
  }

  /** Invalidate & schedule work. */
  _kick = () => {
    cancelAnimationFrame(this._raf);
    clearTimeout(this._timer);
    this._raf = requestAnimationFrame(this._step);
  };

  /** One frame worth of work. */
  _step = (ts) => {
    const tr = this._getTransformStr();
    const changed = tr !== this._transform;

    /* 1 – debounce if the transform keeps changing quickly */
    if (changed && ts - this._lastChange < WAIT_TIMEOUT) {
      this._lastChange = ts;
      this._timer = setTimeout(this._kick, WAIT_TIMEOUT);
      return;
    }

    if (changed) {
      this._transform   = tr;
      this._row         = 0;
      this._lastChange  = ts;
    }

    /* 2 – process a chunk */
    let processed = 0;
    while (this._row < this._areas.length && processed++ < CHUNK_SIZE) {
      const [areaEl, pts] = this._areas[this._row++];
      applyCoords(areaEl, pts, this._mult, this._transform);
    }

    /* 3 – schedule next frame if needed */
    if (this._row < this._areas.length) {
      this._raf = requestAnimationFrame(this._step);
    }
  };
}
