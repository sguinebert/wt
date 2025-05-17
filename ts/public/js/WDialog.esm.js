/* -------------------------------------------------------------------------
 * WDialog – modern rewrite (ES 2022+)
 * -------------------------------------------------------------------------
 *  import WDialog from './WDialog.js';
 *  const dlg = new WDialog({
 *      el: document.querySelector('#myDialog'),
 *      titlebar: '.dialog-title',           // selector *inside* el  (string or HTMLElement)
 *      movable   : true,                    // enable drag‑move
 *      centerX   : true,                    // start centred horizontally
 *      centerY   : true,                    // start centred vertically
 *      onMove    : (x,y)=>{},               // cb while dragging / after programmatic move
 *      onResize  : (w,h, finished)=>{},     // cb during and after resize (see Resizable.js)
 *      onZIndex  : z=>{}                    // cb when bringToFront() bumps z‑index
 *  });
 * -------------------------------------------------------------------------*/
export default class WDialog {
  #el; #onMove; #onResize; #onZ; #centerX; #centerY;
  #dragging = false; #startPointer; #startRect;

  constructor({ el, titlebar, movable = true, centerX = false, centerY = false,
                onMove = null, onResize = null, onZIndex = null }) {
    if (!el) throw new Error('WDialog: el is required');
    this.#el   = el;
    this.#onMove   = onMove;
    this.#onResize = onResize;
    this.#onZ      = onZIndex;
    this.#centerX  = centerX;
    this.#centerY  = centerY;

    this.#fixPositioning();
    if (movable)   this.#initDrag(titlebar);
    this.center(); // initial centering if requested

    // keep centred on viewport resize if wanted
    window.addEventListener('resize', () => this.center());
  }

  /* --------------------------------------------------------------------- */
  /*  public API                                                           */
  /* --------------------------------------------------------------------- */
  center() {
    if (!(this.#centerX || this.#centerY)) return;
    const {width: w, height: h} = this.#el.getBoundingClientRect();
    const vw = window.innerWidth , vh = window.innerHeight;
    if (this.#centerX) this.#el.style.left = `${Math.max(0, (vw - w)/2)}px`;
    if (this.#centerY) this.#el.style.top  = `${Math.max(0, (vh - h)/2)}px`;
    this.#notifyMove();
  }

  bringToFront() {
    const topZ = [...document.querySelectorAll('body *')]
                   .reduce((m,e)=>Math.max(m, +getComputedStyle(e).zIndex||0), 0);
    const newZ = topZ + 1;
    this.#el.style.zIndex = newZ;
    this.#onZ?.(newZ);
  }

  /* --------------------------------------------------------------------- */
  /*  private helpers                                                      */
  /* --------------------------------------------------------------------- */
  #fixPositioning() {
    const st = this.#el.style;
    if (!['absolute','fixed'].includes(getComputedStyle(this.#el).position)) {
      st.position = window.CSS && CSS.supports('position','fixed') ? 'fixed' : 'absolute';
    }
    st.left ??= '0px';
    st.top  ??= '0px';
    st.userSelect = 'none';  // prevent text selection inside title bar while dragging
    st.visibility = 'visible';
  }

  #initDrag(titlebar) {
    const bar = typeof titlebar === 'string' ? this.#el.querySelector(titlebar) : titlebar;
    if (!bar) return;
    bar.style.cursor = 'move';
    bar.style.touchAction = 'none';

    bar.addEventListener('pointerdown', e => {
      if (e.button !== 0) return;                 // left button / primary touch only
      e.preventDefault();
      this.#dragging = true;
      this.#startPointer = { x: e.pageX, y: e.pageY };
      const r = this.#el.getBoundingClientRect();
      this.#startRect   = { x: r.left + window.scrollX, y: r.top + window.scrollY };
      this.#el.setPointerCapture(e.pointerId);
      this.#el.style.cursor = 'move';
    });

    this.#el.addEventListener('pointermove', e => {
      if (!this.#dragging || e.pointerId!==e.pointerId) return; // lint appease
      const dx = e.pageX - this.#startPointer.x;
      const dy = e.pageY - this.#startPointer.y;
      this.#el.style.left = `${this.#startRect.x + dx}px`;
      this.#el.style.top  = `${this.#startRect.y + dy}px`;
      this.#centerX = this.#centerY = false;      // once user moved, stop auto‑centering
      this.#notifyMove();
    });

    const stop = e => {
      if (!this.#dragging) return;
      this.#dragging = false;
      this.#el.releasePointerCapture(e.pointerId);
      this.#el.style.cursor = 'auto';
      this.#notifyMove();
    };
    this.#el.addEventListener('pointerup',   stop);
    this.#el.addEventListener('pointercancel', stop);
  }

  #notifyMove() {
    const x = parseFloat(this.#el.style.left) || 0;
    const y = parseFloat(this.#el.style.top)  || 0;
    this.#onMove?.(x,y);
  }

  /* --------------------------------------------------------------------- */
  /* Resize hook – integrate with the modern Resizable.js if desired       */
  /* --------------------------------------------------------------------- */
  onContentResize(w,h, done=false) {
    // call from Resizable.js or ResizeObserver if you attach it
    this.#onResize?.(w,h,done);
  }
}