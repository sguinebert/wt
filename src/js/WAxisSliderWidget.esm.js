/* -------------------------------------------------------------------------
 * WAxisSliderWidget – modern rewrite (ES 2022+)
 * -------------------------------------------------------------------------
 * Usage
 *   import WAxisSliderWidget from './WAxisSliderWidget.js';
 *   const slider = new WAxisSliderWidget({
 *     widget,                 // outer <div>
 *     canvas: target.canvas,  // <canvas> used for visuals
 *     chart,                  // Wt chart object
 *     series,                 // series index
 *     rect,                   // () => [x,y,w,h] of current slider rectangle
 *     drawArea,               // [x,y,w,h] of full chart draw area
 *     transform,              // vec‑style transform ([sx,0,0,sy,tx,ty])
 *     updateYAxis,            // flag passed to chart.setXRange()
 *   });
 * -------------------------------------------------------------------------*/

 export default class WAxisSliderWidget {
  /** @param {Object} cfg  (see top‑of‑file) */
  constructor (cfg) {
    Object.assign(this, cfg);                 // hoist props onto instance

    /* ---------- constants & helpers ------------------------------------ */
    this.#LEFT   = 1;
    this.#ON     = 2;
    this.#RIGHT  = 3;

    this.#framePending = false;
    this.#pointers = new Map();               // id → {x,y}

    this.canvas.style.touchAction = 'none';   // disable browser gestures
    this.canvas.style.userSelect  = 'none';

    this.#bindEvents();
  }

  /* -------------------------------------------------------------------- */
  /*  PUBLIC API                                                          */
  /* -------------------------------------------------------------------- */

  /** programmatically update config keys */
  updateConfig (partial) {
    Object.assign(this, partial);
    this.#repaint(false);
  }

  /* -------------------------------------------------------------------- */
  /*  PRIVATE FIELDS                                                      */
  /* -------------------------------------------------------------------- */
  #LEFT; #ON; #RIGHT;                    // enum values
  #framePending;                         // rAF throttle flag
  #pointers;                             // active pointer map
  #position   = null;                    // drag mode (LEFT / ON / RIGHT)
  #prevXY     = null;                    // previous pointer coords
  #pinchDelta = null;                    // distance between two fingers

  /* -------------------------------------------------------------------- */
  /*  EVENT BINDING                                                       */
  /* -------------------------------------------------------------------- */
  #bindEvents () {
    const onDown  = e => this.#handleDown(e);
    const onMove  = e => this.#handleMove(e);
    const onEnd   = e => this.#handleEnd(e);
    const onHover = e => this.#updateCursor(this.#toLocal(e));

    this.canvas.addEventListener('pointerdown',  onDown,  {passive:false});
    this.canvas.addEventListener('pointermove',  onMove,  {passive:false});
    this.canvas.addEventListener('pointerup',    onEnd,   {passive:false});
    this.canvas.addEventListener('pointercancel',onEnd,   {passive:false});
    this.canvas.addEventListener('pointermove',  onHover, {passive:true});  // for mouse hover cursors
  }

  /* -------------------------------------------------------------------- */
  /*  POINTER HANDLERS                                                    */
  /* -------------------------------------------------------------------- */
  #handleDown (e) {
    this.#pointers.set(e.pointerId, this.#toLocal(e));
    this.canvas.setPointerCapture(e.pointerId);

    if (this.#pointers.size === 1) {
      // single‑finger drag → determine position (left / on / right)
      this.#prevXY = this.#pointers.get(e.pointerId);
      this.#position = this.#hitZone(this.#prevXY);
    } else if (this.#pointers.size === 2) {
      // pinch/zoom start
      const [a,b] = [...this.#pointers.values()];
      this.#pinchDelta = this.#axisDist(a,b);
      this.#position = null;
    }
    e.preventDefault();
  }

  #handleMove (e) {
    if (!this.#pointers.has(e.pointerId)) return;

    // update stored position
    const pos = this.#toLocal(e);
    this.#pointers.set(e.pointerId, pos);

    if (this.#pointers.size === 1 && this.#position) {
      // single finger drag
      const dx = this.#axisDelta(pos, this.#prevXY);
      switch (this.#position) {
        case this.#LEFT : this.#dragLeft (dx); break;
        case this.#ON   : this.#move     (dx); break;
        case this.#RIGHT: this.#dragRight(dx); break;
      }
      this.#prevXY = pos;
      this.#repaint(true);

    } else if (this.#pointers.size === 2) {
      // pinch/zoom: symmetric expand/contract around centre
      const [a,b] = [...this.#pointers.values()];
      const newΔ  = this.#axisDist(a,b);
      const d     = newΔ - this.#pinchDelta;
      this.#dragLeft (-d/2);
      this.#dragRight( d/2);
      this.#pinchDelta = newΔ;
      this.#repaint(true);
    }
    e.preventDefault();
  }

  #handleEnd (e) {
    this.#pointers.delete(e.pointerId);
    this.canvas.releasePointerCapture(e.pointerId);

    if (this.#pointers.size === 0) {
      this.#position   = null;
      this.#prevXY     = null;
      this.#pinchDelta = null;
    } else if (this.#pointers.size === 1) {
      // continue as single‑finger drag if one finger remains
      this.#position   = null;                 // will be recalculated on next move
      this.#prevXY     = [...this.#pointers.values()][0];
      this.#pinchDelta = null;
    }
  }

  /* -------------------------------------------------------------------- */
  /*  GEOMETRY + HIT‑TESTING                                              */
  /* -------------------------------------------------------------------- */
  #toLocal (e) {
    const rect = this.widget.getBoundingClientRect();
    return {
      x: e.clientX - rect.left,
      y: e.clientY - rect.top,
    };
  }

  #hitZone (p) {
    const r = this.rect(); // [x,y,w,h]
    const [x0,y0,w,h] = r;
    const x1 = x0 + w, y1 = y0 + h;

    const border = 10;                              // px threshold
    const onLeft  = p.y>=y0 && p.y<=y1 && p.x > x0-border/2 && p.x < x0+border/2;
    const onRight = p.y>=y0 && p.y<=y1 && p.x > x1-border/2 && p.x < x1+border/2;
    const inside  = p.y>=y0 && p.y<=y1 && p.x > x0 && p.x < x1;

    return onLeft ? this.#LEFT : onRight ? this.#RIGHT : inside ? this.#ON : null;
  }

  #axisDelta (a,b) {                      // delta along slider axis
    return this.chart.config.isHorizontal ? (b.y - a.y) : (b.x - a.x);
  }
  #axisDist  (a,b) {                      // distance between two pts along axis
    return Math.abs(this.#axisDelta(a,b));
  }

  /* -------------------------------------------------------------------- */
  /*  DRAG / MOVE  (logic copied from legacy code, but member‑ised)       */
  /* -------------------------------------------------------------------- */
  #dragLeft  = dx => this.#dragBorder(true,  dx);
  #dragRight = dx => this.#dragBorder(false, dx);
  #dragBorder (leftSide, dx) {
    const {transform,drawArea} = this;
    const u = transform[4] / drawArea[2];          // left  in [0,1]
    const v = transform[0] + u;                    // right in [0,1]

    const beforePx = (leftSide ? u : v) * drawArea[2];
    const afterPx  = beforePx + dx;
    let   after    = afterPx / drawArea[2];

    if ( leftSide && v <= after) return;           // no inversion
    if (!leftSide && after <= u) return;

    const newZoom = 1 / ( (leftSide? v-after : after-u) );
    if (newZoom > this.#maxXZoom() || newZoom < this.#minXZoom()) return;

    after = Math.min(Math.max(after,0),1);
    leftSide ? this.#changeRange(after, v) : this.#changeRange(u, after);
  }

  #move (dx) {
    const {transform,drawArea} = this;
    const spanPx = transform[0] * drawArea[2];
    let uPx = transform[4] + dx;
    uPx = Math.min(Math.max(uPx,0), drawArea[2]-spanPx);
    const u = uPx / drawArea[2];
    const v = u + transform[0];
    this.#changeRange(u, v);
  }

  /* wrappers around legacy helper logic (kept verbatim) */
  #changeRange = (u,v)=>{ this.changeRange?.(u,v); };
  #minXZoom    = ()=> this.chart.config.minZoom.x[this.#seriesXAxis()];
  #maxXZoom    = ()=> this.chart.config.maxZoom.x[this.#seriesXAxis()];
  #seriesXAxis = ()=> this.chart.config.series[this.series].xAxis;

  /* -------------------------------------------------------------------- */
  /*  REPAINT throttled via rAF                                           */
  /* -------------------------------------------------------------------- */
  #repaint (setRange) {
    if (this.#framePending) return;
    this.#framePending = true;
    requestAnimationFrame(()=>{
      this.target.repaint();
      if (setRange) {
        const [u,v] = this.#transformToUV();
        this.chart.setXRange(this.series, u, v, this.updateYAxis);
      }
      this.#framePending = false;
    });
  }

  #transformToUV () {
    const {transform,drawArea} = this;
    const u = transform[4] / drawArea[2];
    return [u, transform[0] + u];
  }

  /* -------------------------------------------------------------------- */
  /*  VISUAL CURSOR feedback                                              */
  /* -------------------------------------------------------------------- */
  #updateCursor (p) {
    const zone = this.#hitZone(p);
    if (zone===this.#LEFT||zone===this.#RIGHT)
      this.canvas.style.cursor = this.chart.config.isHorizontal? 'row-resize' : 'col-resize';
    else if (zone===this.#ON)
      this.canvas.style.cursor = 'move';
    else
      this.canvas.style.cursor = 'auto';
  }
}