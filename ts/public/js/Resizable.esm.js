/* -------------------------------------------------------------------------
 * Resizable – modern rewrite (ES 2022+)
 * -------------------------------------------------------------------------
 * Makes any absolutely‑positioned element resizable by dragging its
 * bottom‑right corner (16 × 16 px ‘grab’ zone).
 * -------------------------------------------------------------------------
 *   import Resizable from './Resizable.js';
 *   const res = new Resizable(element, {
 *     minWidth : 120,     // px   – optional (default: current client size)
 *     minHeight: 80,      // px
 *     onResize : (w,h,done)=> console.log(w,h,done)  // callback each move, and when done=true
 *   });
 * -------------------------------------------------------------------------*/
export default class Resizable {
  #el; #opts; #start = null; #raf = 0;

  constructor(el, opts = {}) {
    this.#el   = el;
    this.#opts = { minWidth: 0, minHeight: 0, onResize: () => {}, ...opts };

    // fallback to current size as minimum if none provided
    if (!this.#opts.minWidth)  this.#opts.minWidth  = el.clientWidth;
    if (!this.#opts.minHeight) this.#opts.minHeight = el.clientHeight;

    el.style.touchAction = 'none'; // disable touch panning in the grab zone
    el.addEventListener('pointerdown', this.#down, {passive:false});
  }

  /* ---------- private helpers ------------------------------------------ */
  #isInGrabZone = ({offsetX, offsetY, target}) => {
    // Works even if pointerdown bubbles from a child – convert to element space:
    const rect = this.#el.getBoundingClientRect();
    const x = (offsetX ?? 0) + (target?.getBoundingClientRect().left ?? 0) - rect.left;
    const y = (offsetY ?? 0) + (target?.getBoundingClientRect().top  ?? 0) - rect.top;
    return rect.width  - x < 16 && rect.height - y < 16;
  };

  #down = e => {
    if (!this.#isInGrabZone(e)) return;

    e.preventDefault();
    const {width, height, left, top} = this.#el.getBoundingClientRect();
    this.#start = {
      x0 : e.clientX,
      y0 : e.clientY,
      w0 : width,
      h0 : height,
      left, top,
    };

    // pointer capture guarantees move/up events even outside the element
    this.#el.setPointerCapture(e.pointerId);
    this.#el.addEventListener('pointermove', this.#move, {passive:false});
    this.#el.addEventListener('pointerup',   this.#up,   {passive:false});
    document.body.style.userSelect = 'none';
    document.body.style.cursor     = 'nwse-resize';
  };

  #move = e => {
    if (!this.#start) return;
    // throttle with rAF to avoid layout trashing on every event
    if (this.#raf) return;
    this.#raf = requestAnimationFrame(() => {
      this.#raf = 0;
      const dx = e.clientX - this.#start.x0;
      const dy = e.clientY - this.#start.y0;

      const w  = Math.max(this.#start.w0 + dx, this.#opts.minWidth);
      const h  = Math.max(this.#start.h0 + dy, this.#opts.minHeight);

      this.#el.style.width  = w + 'px';
      this.#el.style.height = h + 'px';
      this.#opts.onResize(w, h, false);
    });
    e.preventDefault();
  };

  #up = e => {
    if (!this.#start) return;
    this.#move(e);                                   // final size update
    this.#el.releasePointerCapture(e.pointerId);
    this.#el.removeEventListener('pointermove', this.#move);
    this.#el.removeEventListener('pointerup',   this.#up);
    document.body.style.userSelect = '';
    document.body.style.cursor     = '';
    const w = parseFloat(this.#el.style.width);
    const h = parseFloat(this.#el.style.height);
    this.#opts.onResize(w, h, true);
    this.#start = null;
  };
}
