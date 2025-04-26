/* -------------------------------------------------------------------------
 * SizeHandle – modern rewrite (ES 2022+)
 * -------------------------------------------------------------------------
 * A draggable resize handle that reports the delta once the user releases.
 * Works with mouse, pen and touch by using Pointer Events + pointer capture.
 * -------------------------------------------------------------------------
 *  new SizeHandle({
 *    orientation : 'h' | 'v',     // horizontal (left–right) or vertical (top–bottom)
 *    width       : 4,             // handle thickness
 *    height      : 24,            // perpendicular size (ignored for cursor hit‑box)
 *    minDelta    : -100,          // clamp movement
 *    maxDelta    :  300,
 *    cssClass    : 'wt-resize',   // added to the floating <div>
 *    targetEl    : element,       // element being resized (for coordinates)
 *    parentEl    : parentElement, // stacking context
 *    startEvent  : PointerEvent,  // the original pointerdown/mousedown event
 *    offsetX, offsetY,            // margins baked into the original code
 *    onDone      : delta => {}    // callback when released
 *  });
 * -------------------------------------------------------------------------*/

 export default class SizeHandle {
  #handle; #opts; #startPos; #origin; #cursorBefore;

  constructor(opts) {
    this.#opts = opts;
    this.#createHandle();
    this.#initPosition();
    this.#grabPointer();
  }

  /* ---------- private helpers ----------------------------------------- */
  #createHandle() {
    const {orientation : o, width : w, height : h, cssClass, parentEl} = this.#opts;
    const div = document.createElement('div');
    div.className      = cssClass ?? 'wt-resize';
    div.style.position = 'absolute';
    div.style.zIndex   = '100';
    div.style[o === 'h' ? 'width'  : 'height'] = `${h}px`;
    div.style[o === 'h' ? 'height' : 'width']  = `${w}px`;
    parentEl.appendChild(div);
    this.#handle = div;
  }

  #initPosition() {
    const {targetEl: el, parentEl: p, offsetX=0, offsetY=0, orientation:o} = this.#opts;
    const rectEl = el.getBoundingClientRect();
    const rectP  = p .getBoundingClientRect();

    const x = rectEl.left - rectP.left + offsetX;
    const y = rectEl.top  - rectP.top  + offsetY;

    this.#origin   = {x, y};            // top‑left of element inside parent
    this.#startPos = {x, y};            // will be updated on move

    Object.assign(this.#handle.style, { left:`${x}px`, top:`${y}px` });
  }

  #grabPointer() {
    const {startEvent:e, orientation:o, minDelta:dMin, maxDelta:dMax, parentEl, onDone} = this.#opts;
    const id = e.pointerId ?? 0;

    /* visual feedback --------------------------------------------------- */
    const domRoot = parentEl.closest('.Wt-domRoot') || parentEl;
    domRoot.style.pointerEvents = 'none';
    this.#cursorBefore = document.body.style.cursor;
    document.body.style.cursor = o === 'h' ? 'ew-resize' : 'ns-resize';

    /* capture & listeners ---------------------------------------------- */
    this.#handle.setPointerCapture(id);

    const move = ev => {
      if (ev.pointerId !== id) return;
      const dx = ev.clientX - e.clientX;
      const dy = ev.clientY - e.clientY;
      const delta = o === 'h' ? dx : dy;
      const clamped = Math.min(Math.max(delta, dMin), dMax);
      if (o === 'h') this.#handle.style.left = `${this.#origin.x + clamped}px`;
      else           this.#handle.style.top  = `${this.#origin.y + clamped}px`;
    };

    const up = ev => {
      if (ev.pointerId !== id) return;
      const dx = ev.clientX - e.clientX;
      const dy = ev.clientY - e.clientY;
      const raw = o === 'h' ? dx : dy;
      const delta = Math.min(Math.max(raw, dMin), dMax);

      /* cleanup visuals */
      domRoot.style.pointerEvents = '';
      document.body.style.cursor  = this.#cursorBefore ?? 'auto';

      /* remove elements & listeners */
      this.#handle.remove();
      this.#handle.releasePointerCapture(id);
      this.#handle.removeEventListener('pointermove', move);
      this.#handle.removeEventListener('pointerup',   up);
      onDone?.(delta);
    };

    this.#handle.addEventListener('pointermove', move, {passive:false});
    this.#handle.addEventListener('pointerup',   up,   {passive:false});
  }
}