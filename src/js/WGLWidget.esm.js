/* -------------------------------------------------------------------------
 * WGLWidget – modern rewrite (ES 2022+)
 * -------------------------------------------------------------------------
 * Lightweight helper around an HTMLCanvasElement that owns a WebGL context,
 * plus a pluggable Pointer/Mouse interaction handler. Removes all legacy
 * MSPointer/touch branches and simplifies preload logic.
 * ------------------------------------------------------------------------- */

 import { mat4, vec3 } from 'gl-matrix';

 export default class WGLWidget {
   /**
    * @param {HTMLCanvasElement} canvas – target canvas (will get the context)
    * @param {Object}   [opts]
    * @param {boolean}  [opts.antialias=true] – WebGL antialias flag
    * @param {Function} [opts.noGlFallback]   – called when WebGL unavailable
    */
   constructor(canvas, {
     antialias = true,
     noGlFallback = () => canvas.replaceWith(canvas.firstElementChild)
   } = {}) {
     if (!(canvas instanceof HTMLCanvasElement)) throw new TypeError('canvas must be <canvas>');
 
     // ------------------------------------------------ context discovery
     this.gl = canvas.getContext('webgl2', { antialias }) 
             || canvas.getContext('webgl',  { antialias }) 
             || canvas.getContext('experimental-webgl', { antialias });
 
     if (!this.gl) {
       noGlFallback();
       return;                    // widget becomes inert
     }
 
     // ------------------------------------------------ public hooks
     this.initializeGL = () => {};
     this.paintGL      = () => {};
     this.resizeGL     = () => {};
 
     // ------------------------------------------------ private state
     this.#canvas      = canvas;
     this.#updates     = [];
     this.#initialized = false;
     this.#preload     = { tex:0, buf:0 };
 
     // ------------------------------------------------ pointer handling
     this.#installPointerEvents();
 
     // context loss / restore
     canvas.addEventListener('webglcontextlost',  e => { e.preventDefault(); this.#initialized = false; });
     canvas.addEventListener('webglcontextrestored', () => this.#handlePreload());
   }
 
   // ----------------------------------------------------------------------
   /*                               Public API                              */
   setInteractionHandler(handler) {
     this.#interaction = handler;
     if (handler?.setTarget) handler.setTarget(this);
   }
 
   /** Schedule a function to run after the widget is initialized & preloads */
   enqueueUpdate(fn) { this.#updates.push(fn); }
 
   /** Call when any async buffer/texture load completes */
   handlePreloadDone(kind /* 'tex' | 'buf' */) {
     if (kind) this.#preload[kind] = Math.max(0, this.#preload[kind]-1);
     this.#handlePreload();
   }
 
   // ----------------------------------------------------------------------
   /*                          Built‑in handlers                            */
   static LookAtHandler = class LookAtHandler {
     #drag    = null;      // prev pointer position
     #pinchW  = null;     // pinch distance
     #pinchId1 = null;  // first touch ID
     #pinchId2 = null;  // second touch ID
     #lastTouches = new Map(); // touch ID -> {x,y} coords
     constructor(matrix, center, up, { pitchRate = 0.005, yawRate = 0.005 } = {}) {
       this.m        = matrix;
       this.center   = center;
       this.up       = up;
       this.pitchR   = pitchRate; this.yawR = yawRate;
     }
 
     /* called by WGLWidget */
     setTarget(widget){ this.widget = widget; }
 
     pointerdown(e){ if (e.isPrimary){ this.#drag = this.#pt(e); }}
     pointerup  (_){ this.#drag = null; this.#pinchW = null; }
     pointermove(e){
       if (!this.widget) return;
       if (e.pointerType === 'touch' && e.pointerId===this.#pinchId2 && this.#pinchId1){
         // pinch zoom – compute distance change
         const p1 = this.#lastTouches.get(this.#pinchId1);
         const p2 = {x:e.pageX,y:e.pageY};
         const d  = Math.hypot(p1.x-p2.x, p1.y-p2.y);
         if (this.#pinchW){ this.#zoom(Math.sign(d-this.#pinchW)); }
         this.#pinchW = d; this.#lastTouches.set(e.pointerId,p2);
         return;
       }
       if (!this.#drag) return;
       const now = this.#pt(e);
       const dx = now.x-this.#drag.x, dy = now.y-this.#drag.y;
       this.#rotate(dx,dy);
       this.#drag = now;
     }
     wheel(e){ this.#zoom(Math.sign(e.deltaY)*-1); }
 
     /* internals */
     #pt(e){ return {x:e.pageX,y:e.pageY}; }
     #zoom(dir){
       const s = Math.pow(1.2, dir);
       mat4.translate(this.m, this.center);
       mat4.scale(this.m,[s,s,s]);
       vec3.negate(this.center);
       mat4.translate(this.m, this.center);
       vec3.negate(this.center);
       this.widget.paintGL();
     }
     #rotate(dx,dy){
       const right = vec3.fromValues(this.m[0],this.m[4],this.m[8]);
       const r     = mat4.create();
       mat4.identity(r);
       mat4.translate(r,this.center);
       mat4.rotate(r, dy*this.pitchR, right);
       mat4.rotate(r, dx*this.yawR  , this.up);
       vec3.negate(this.center); mat4.translate(r,this.center); vec3.negate(this.center);
       mat4.multiply(this.m,r,this.m);
       this.widget.paintGL();
     }
   };
 
   // ----------------------------------------------------------------------
   /*                         Implementation details                        */
   #canvas;
   #interaction=null;
   #updates;
   #initialized;
   #preload;
 
   #installPointerEvents(){
     const c = this.#canvas;
     c.style.touchAction = 'none';      // disable browser panning / zoom
 
     c.addEventListener('pointerdown',  e=>{ this.#interaction?.pointerdown?.(e); c.setPointerCapture(e.pointerId); });
     c.addEventListener('pointermove',  e=> this.#interaction?.pointermove?.(e));
     c.addEventListener('pointerup',    e=>{ this.#interaction?.pointerup  ?. (e); c.releasePointerCapture(e.pointerId);} );
     c.addEventListener('pointercancel',e=> this.#interaction?.pointerup  ?. (e));
     c.addEventListener('wheel',        e=>{ this.#interaction?.wheel?.(e); e.preventDefault(); }, {passive:false});
   }
 
   #handlePreload(){
     if (this.#preload.tex||this.#preload.buf) return;  // still pending
     const gl = this.gl;
     if (!this.#initialized){ this.initializeGL(gl); this.#initialized = true; }
     for (const u of this.#updates) u();
     this.#updates.length = 0;
     this.resizeGL(gl);
     this.paintGL(gl);
   }
 }
 