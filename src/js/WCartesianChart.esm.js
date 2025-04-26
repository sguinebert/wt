/* ------------------------------------------------------------------
   cartesianChart.es2022.js  ⚡️ v1.0.0 — 2025‑04‑24
   ------------------------------------------------------------------
   A modern, **framework‑agnostic** replacement for the legacy
   WCartesianChart interaction layer.

   ✦ ES 2022 modules (exported class + default)
   ✦ Pointer‑Events + passive wheel listeners
   ✦ DOMMatrix/DOMPoint maths (native – no helper libs)
   ✦ rAF–driven inertial pan with springy edge‑bounce
   ✦ ResizeObserver (good‑bye window.onresize polling)
   ✦ AbortController ⇒ automatic teardown
   ✦ Hi‑DPI aware via devicePixelRatio
   ------------------------------------------------------------------ */

/** Utility – distance between 2 DOMPoints / [x,y] pairs */
const dist = (a, b) => {
  if (Array.isArray(a)) a = new DOMPoint(a[0], a[1]);
  if (Array.isArray(b)) b = new DOMPoint(b[0], b[1]);
  return Math.hypot(b.x - a.x, b.y - a.y);
};

/** Utility – midpoint of two points */
const mid = (a, b) => {
  if (Array.isArray(a)) a = new DOMPoint(a[0], a[1]);
  if (Array.isArray(b)) b = new DOMPoint(b[0], b[1]);
  return new DOMPoint((a.x + b.x) / 2, (a.y + b.y) / 2);
};

export class CartesianChartInteractor {
  /**
   * @param {HTMLCanvasElement} canvas   – target canvas (2D or WebGL)
   * @param {Object}   opts             – interaction & config options
   */
  constructor (canvas, opts = {}) {
    if (!(canvas instanceof HTMLCanvasElement)) {
      throw new TypeError('canvas must be an <HTMLCanvasElement>');
    }

    // ⇢ public handles ------------------------------------------------------
    this.canvas = canvas;
    this.ctx    = canvas.getContext('2d');

    // ⇢ configuration -------------------------------------------------------
    const defaults = {
      pan       : true,
      zoom      : true,
      minZoom   : 0.5,
      maxZoom   : 50,
      crosshair : true,
      friction  : 0.0025,   // inertia slowdown per frame
      spring    : 0.00035,  // edge‑bounce stiffness
      bounds    : null,     // {xMin,xMax,yMin,yMax} | null (disabled)
      tooltip   : null,     // fn(dataPt) → string|Promise<string>
    };
    this.cfg = Object.freeze({ ...defaults, ...opts });

    // ⇢ internal state ------------------------------------------------------
    this.transform = new DOMMatrix();           // world→view
    this.velocity  = new DOMPoint();            // px / ms (for inertia)
    this._raf      = 0;                        // requestAnimationFrame id
    this._pointers = new Map();                // active pointer map
    this._cross    = new DOMPoint();           // crosshair px
    this._ac       = new AbortController();    // for clean‑up

    // tooltip state
    this._tipEl    = null;
    this._tipTimer = 0;

    // dpi‑aware sizing + live resize ---------------------------------------
    this.#resize(devicePixelRatio);
    new ResizeObserver(() => this.#resize(devicePixelRatio))
      .observe(canvas, { box: 'content-box', signal: this._ac.signal });

    // ⇢ events --------------------------------------------------------------
    const { signal } = this._ac;
    canvas.addEventListener('pointerdown',   this.#onPointerDown, { signal });
    canvas.addEventListener('pointermove',   this.#onPointerMove, { signal });
    canvas.addEventListener('pointerup',     this.#onPointerUp,   { signal });
    canvas.addEventListener('pointercancel', this.#onPointerUp,   { signal });
    canvas.addEventListener('wheel', this.#onWheel, { passive:false, signal });
  }

  /* --------------------------------------------------------------------- */
  /** destroy/teardown – removes listeners & cancels rAF */
  destroy () {
    cancelAnimationFrame(this._raf);
    this._ac.abort();
    this.#removeTooltip();
  }

  /* --------------------------------------------------------------------- */
  // ——— PUBLIC API ———

  zoomBy (factor, centrePx = new DOMPoint(this.canvas.width/2,this.canvas.height/2)) {
    if (!this.cfg.zoom) return;
    const { minZoom, maxZoom } = this.cfg;
    const current = this.transform.a;
    const target  = current * factor;
    const clamped = Math.min(maxZoom, Math.max(minZoom, target));
    const scale   = clamped / current;
    if (scale === 1) return;

    this.transform = DOMMatrix.fromMatrix(this.transform)
      .translateSelf(centrePx.x, centrePx.y)
      .scaleSelf(scale)
      .translateSelf(-centrePx.x, -centrePx.y);

    this.#applyBounds();
    this.#notify();
    this.requestRender();
  }

  panBy (dx, dy) {
    if (!this.cfg.pan || (!dx && !dy)) return;
    this.transform.e += dx;
    this.transform.f += dy;
    this.#applyBounds();
    this.#notify();
    this.requestRender();
  }

  reset () {
    this.transform = new DOMMatrix();
    this.#notify();
    this.requestRender();
  }

  /** Fit given data‑bounds into viewport */
  fitBounds (xMin, xMax, yMin, yMax, pad = 0.05) {
    const w = xMax - xMin;
    const h = yMax - yMin;
    if (w <= 0 || h <= 0) return;

    const { width: vw, height: vh } = this.canvas.getBoundingClientRect();
    const s  = Math.min((vw*(1-2*pad))/w, (vh*(1-2*pad))/h);
    const tx = -xMin * s + pad * vw;
    const ty = -yMin * s + pad * vh;
    this.transform = new DOMMatrix([ s, 0, 0, s, tx, ty ]);
    this.#applyBounds();
    this.#notify();
    this.requestRender();
  }

  /** Convert world‑space → screen px */
  toScreen (pt){ return this.transform.transformPoint(pt); }
  /** Convert screen px → world‑space */
  toData   (pt){ return this.transform.inverse().transformPoint(pt); }

  /** subscribe to interaction changes */
  onInteraction (cb){ this._onInteract = cb; }
  /** host‑side draw callback */
  set onRender(fn){ this._onRender = fn; this.requestRender(); }
  get onRender(){   return this._onRender; }

  /* --------------------------------------------------------------------- */
  /** schedule a rAF draw if none pending */
  requestRender(){ if (!this._raf) this._raf = requestAnimationFrame(()=>{this._raf=0; this.#render();}); }

  /* --------------------------------------------------------------------- */
  // ——— PRIVATE ———

  #render(){
    const { ctx, canvas } = this;
    const dpr = devicePixelRatio;

    ctx.save();
    ctx.setTransform(dpr,0,0,dpr,0,0);
    ctx.clearRect(0,0,canvas.width/dpr, canvas.height/dpr);

    // delegate to external renderer if provided
    this._onRender?.(ctx, this.transform);

    // crosshair overlay
    if (this.cfg.crosshair){
      ctx.setTransform(1,0,0,1,0,0);
      ctx.strokeStyle = '#888';
      ctx.setLineDash([4,4]);
      ctx.beginPath();
      ctx.moveTo(this._cross.x+0.5, 0);            ctx.lineTo(this._cross.x+0.5, canvas.height);
      ctx.moveTo(0, this._cross.y+0.5);            ctx.lineTo(canvas.width, this._cross.y+0.5);
      ctx.stroke();
    }

    ctx.restore();
  }

  /* --------------------------------------------------------------------- */
  #onPointerDown = e => {
    this.canvas.setPointerCapture(e.pointerId);
    this._pointers.set(e.pointerId, this.#pt(e));
    this.velocity = new DOMPoint();
    this.#removeTooltip();
    this._cross = this.#pt(e);
    this.requestRender();
  }

  #onPointerMove = e => {
    if (!this._pointers.has(e.pointerId)) return;
    const prev = this._pointers.get(e.pointerId);
    const cur  = this.#pt(e);
    this._pointers.set(e.pointerId, cur);

    if (this._pointers.size === 1){ // pan
      const dx = cur.x - prev.x;
      const dy = cur.y - prev.y;
      this.panBy(dx, dy);
      this._cross = cur;
      this.velocity = new DOMPoint(dx, dy);
    } else if (this._pointers.size === 2 && this.cfg.zoom){ // pinch
      const [p1,p2] = [...this._pointers.values()];
      const before  = dist(p1,p2);
      const after   = dist(...this._pointers.values());
      if (before > 0){
        this.zoomBy(after/before, mid(p1,p2));
      }
    }
    e.preventDefault();
  }

  #onPointerUp = e => {
    this._pointers.delete(e.pointerId);
    this.canvas.releasePointerCapture(e.pointerId);

    if (this._pointers.size === 0){
      if (Math.hypot(this.velocity.x,this.velocity.y) > 0.4) this.#kickInertia();
      // simple tap tooltip
      if (e.timeStamp - (this._lastTap||0) < 250){
        this.#queueTooltip(this.#pt(e));
      }
      this._lastTap = e.timeStamp;
    }
  }

  #onWheel = e => {
    if (!this.cfg.zoom) return;
    const factor = e.deltaY < 0 ? 1.1 : 1/1.1;
    this.zoomBy(factor, this.#pt(e));
    e.preventDefault();
  }

  /* --------------------------------------------------------------------- */
  #kickInertia(){
    cancelAnimationFrame(this._raf);
    const step = () => {
      const v = this.velocity;
      if (Math.hypot(v.x,v.y) < 0.05){ this.velocity = new DOMPoint(); return; }

      // friction
      this.velocity = new DOMPoint(v.x*(1-this.cfg.friction), v.y*(1-this.cfg.friction));
      this.panBy(this.velocity.x, this.velocity.y);
      this._raf = requestAnimationFrame(step);
    };
    this._raf = requestAnimationFrame(step);
  }

  /* --------------------------------------------------------------------- */
  #applyBounds(){
    if (!this.cfg.bounds) return;
    const { xMin,xMax,yMin,yMax } = this.cfg.bounds;
    const tl = new DOMPoint(xMin,yMin).matrixTransform(this.transform);
    const br = new DOMPoint(xMax,yMax).matrixTransform(this.transform);

    let dx=0, dy=0;
    if (tl.x > 0) dx -= tl.x;
    if (br.x < this.canvas.width)  dx += this.canvas.width  - br.x;
    if (tl.y > 0) dy -= tl.y;
    if (br.y < this.canvas.height) dy += this.canvas.height - br.y;

    if (dx || dy){
      // springy edge
      this.transform.e += dx;
      this.transform.f += dy;
    }
  }

  /* --------------------------------------------------------------------- */
  #queueTooltip(pt){
    if (!this.cfg.tooltip) return;
    clearTimeout(this._tipTimer);
    this._tipTimer = setTimeout(async ()=>{
      let html = typeof this.cfg.tooltip==='function' ? await this.cfg.tooltip(this.toData(pt)) : this.cfg.tooltip;
      if (!html) return;
      this.#showTooltip(html, pt);
    }, 500);
  }

  #showTooltip(html, pt){
    this.#removeTooltip();
    const div = document.createElement('div');
    div.className = 'chart-tooltip';
    div.innerHTML = html;
    Object.assign(div.style,{
      position:'fixed',zIndex:10000,pointerEvents:'none',
      background:'#222',color:'#fff',padding:'4px 8px',borderRadius:'4px',fontSize:'12px',
      maxWidth:'240px',whiteSpace:'nowrap',textOverflow:'ellipsis',overflow:'hidden'
    });
    document.body.append(div);
    div.style.left = `${pt.x+12}px`;
    div.style.top  = `${pt.y+12}px`;
    this._tipEl = div;
  }

  #removeTooltip(){
    clearTimeout(this._tipTimer);
    this._tipTimer=0;
    this._tipEl?.remove();
    this._tipEl=null;
  }

  /* --------------------------------------------------------------------- */
  #resize(dpr){
    const rect = this.canvas.getBoundingClientRect();
    this.canvas.width  = rect.width  * dpr;
    this.canvas.height = rect.height * dpr;
    this.requestRender();
  }

  #pt(e){ return new DOMPoint(e.clientX, e.clientY); }
  #notify(){ this._onInteract?.(this.transform); }
}