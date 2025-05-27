/* ------------------------------------------------------------------
 *  painted-widget.js  – modern, framework-agnostic rewrite of legacy
 *  WPaintedWidget + gfxUtils helpers
 *
 *  ▸ ES-modules, no WT macros, no globals
 *  ▸ preload-cancel-repaint lifecycle baked into a class
 *  ▸ gfxUtils exported as a plain object with the original numerics
 *    but implemented in clean, readable functions
 * ------------------------------------------------------------------ */

export class PaintedWidget {
  /**
   * @param {string|HTMLElement} root  –  `<div>` that will contain a `<canvas>`
   * @param {number}             w     –  canvas width  (device pixels)
   * @param {number}             h     –  canvas height (device pixels)
   */
  constructor (root, w = 300, h = 150){
    this.host = typeof root === 'string' ? document.getElementById(root) : root;
    this.canvas = document.createElement('canvas');
    this.canvas.width  = w;
    this.canvas.height = h;

    this.host.appendChild(this.canvas);
    this.ctx = this.canvas.getContext('2d');

    /* -------- image pre-loading --------------- */
    /** @type {HTMLImageElement[]} */
    this.images  = [];
    /** @type {AbortController[]} */
    this.preloads = [];

    /* user-supplied paint callback (ctx,w,h,images[]) */
    this.repaint = ()=>{};
  }

  /* ============================================================= */
  /* ---------------- public helpers ----------------------------- */

  /** asynchronous image pre-loader with AbortController */
  async preload (...urls){
    this.cancelPreloads();

    const promises = urls.map(u=>{
      const ctrl = new AbortController();
      this.preloads.push(ctrl);

      return new Promise((resolve,reject)=>{
        const img = new Image();
        img.onload  = ()=> resolve(img);
        img.onerror = ()=> reject(new Error(`failed: ${u}`));
        img.src     = u;
        /* react to external abort */
        ctrl.signal.addEventListener('abort', ()=>{
          img.src = '';            // stop loading
          reject(new DOMException('aborted','AbortError'));
        });
      });
    });

    try {
      this.images = await Promise.all(promises);
      this.repaint(this.ctx,this.canvas.width,this.canvas.height,this.images);
    } finally {
      /* clear finished controllers */
      this.preloads.length = 0;
    }
  }

  cancelPreloads (){
    this.preloads.forEach(c=>c.abort());
    this.preloads.length = 0;
  }

  /** convenience – clears, then calls repaint */
  redraw (){
    const {ctx,canvas:{width:w,height:h}} = this;
    ctx.save();
    ctx.setTransform(1,0,0,1,0,0);
    ctx.clearRect(0,0,w,h);
    ctx.restore();
    this.repaint(ctx,w,h,this.images);
  }
}

/* =====================================================================
 *                     gfxUtils  (subset, modernised)
 * ===================================================================== */

const TA = {// transform array indices
  m11:0,m12:1,m21:2,m22:3,dx:4,dy:5
};
function det (m){ return m[TA.m11]*m[TA.m22]-m[TA.m21]*m[TA.m12]; }

export const gfxUtils = {
  /* tiny 3×3-less affine matrix helpers (array[6]) ------------------ */
  mult(a,b){
    if (b.length === 6){       /* matrix × matrix */
      return [
        a[0]*b[0]+a[2]*b[1] ,  a[1]*b[0]+a[3]*b[1] ,
        a[0]*b[2]+a[2]*b[3] ,  a[1]*b[2]+a[3]*b[3] ,
        a[0]*b[4]+a[2]*b[5]+a[4] ,
        a[1]*b[4]+a[3]*b[5]+a[5]
      ];
    }
    if (b.length === 2){       /* matrix × point */
      return [
        a[0]*b[0]+a[2]*b[1]+a[4],
        a[1]*b[0]+a[3]*b[1]+a[5]
      ];
    }
    throw new TypeError('unsupported multiply');
  },
  invert(m){
    const d = det(m);
    if (!d) throw Error('non-invertible');
    return [
       m[3]/d , -m[1]/d ,
      -m[2]/d ,  m[0]/d ,
      (m[2]*m[5]-m[3]*m[4])/d ,
      (m[1]*m[4]-m[0]*m[5])/d
    ];
  },

  /* quick canvas helpers ------------------------------------------- */
  cssColor: ([r,g,b,a])=>`rgba(${r},${g},${b},${a})`,

  /** draw path (array[[x,y,type]])   MOVE:0 LINE:1 ...  */
  drawPath(ctx,path,{fill=true,stroke=false}={}){
    ctx.beginPath();
    for (const [x,y,t] of path){
      if     (t===0) ctx.moveTo(x,y);
      else if(t===1) ctx.lineTo(x,y);
      /* cubic / quad segments omitted for brevity */
    }
    if (fill)   ctx.fill();
    if (stroke) ctx.stroke();
  },

  /* simple rect helpers ------------------------------------------- */
  rectNorm: ([x,y,w,h]) => w>=0 ? [x,y,w,h] : [x+w,y,-w,h],
  rectCenter: r=>{const [x,y,w,h]=r;return{x:x+w/2,y:y+h/2};}

  
};

// ———————————————————————————————————————————————————————————————
// A compact, ES-module collection of the above features.
//———————————————————————————————————————————————————————————————

/** 1. Crisp‐align a Path2D before stroking */
export const crispPath = (path) => {
  // shift by 0.5px so 1px strokes land on device‐pixel boundaries
  return new Path2D(path).transform(1,0,0,1,0.5,0.5);
};

/** 2. Affine‐matrix helpers ([a,b,c,d,e,f] arrays) */
export const mul = ([a,b,c,d,e,f], [A,B,C,D,E,F]) => [
  a*A + c*B, b*A + d*B,
  a*C + c*D, b*C + d*D,
  a*E + c*F + e, b*E + d*F + f
];
export const det = ([a,_,c,_,_,_ , b,_,d,_,_,_]) => a*d - b*c;
export const inverse = (m) => {
  const D = det(m);
  if (!D) throw new Error("Singular matrix");
  const [a, b, c, d, e, f] = m;
  return [ d/D, -b/D, -c/D, a/D,
          (c*f - d*e)/D, (b*e - a*f)/D ];
};
// apply to point [x,y] or segment [x,y,type]
export const apply = (m, seg) => {
  const [a,_,c,_,e,_, b,_,d,_,_,f] = m.flatMap((v,i)=>
    i<6? [v] : []); // flatten first 6 only
  const [x,y] = seg;
  return [ a*x + c*y + e, b*x + d*y + f, seg[2] ];
};

/** 3. Point‐in‐polygon (ray‐cast) */
export const pointInPoly = (px, py, path) => {
  let inside = false;
  for (let i=0, j=path.length-1; i<path.length; j=i++) {
    const [xi, yi] = path[i], [xj, yj] = path[j];
    const intersect = ((yi>py) !== (yj>py))
      && (px < (xj-xi)*(py-yi)/(yj-yi) + xi);
    if (intersect) inside = !inside;
  }
  return inside;
};

/** 4. Arc point on ellipse */
export const arcPos = (cx, cy, rx, ry, deg) => {
  const rad = -deg * Math.PI/180;
  return [ cx + rx*Math.cos(rad), cy + ry*Math.sin(rad) ];
};

/** 5. Rect helpers */
export const normalizeRect = ([x,y,w,h]) =>
  w>=0
    ? [x,y,w,h]
    : [x+w,y,-w,h];
export const intersectRect = (r1, r2) => {
  const [x1,y1,w1,h1] = normalizeRect(r1),
        [x2,y2,w2,h2] = normalizeRect(r2),
        x = Math.max(x1,x2),
        y = Math.max(y1,y2),
        W = Math.min(x1+w1,x2+w2) - x,
        H = Math.min(y1+h1,y2+h2) - y;
  return W>0 && H>0 ? [x,y,W,H] : [0,0,0,0];
};

/** 6. Draw a Path2D with optional fill/clip */
export function drawPath(ctx, path, { fill=false, stroke=true, clip=false }={}) {
  if (clip) {
    ctx.save();
    ctx.clip(path);
  }
  if (fill)   ctx.fill(path);
  if (stroke) ctx.stroke(path);
  if (clip)   ctx.restore();
}

/** 7. Stamp a small Path2D “stencil” at every MOVE_TO/LINE_TO */
export function drawStencil(ctx, stencil, path, { fill=false, stroke=true }={}) {
  for (const cmd of path.commands) {
    if (cmd.type==="moveTo" || cmd.type==="lineTo") {
      ctx.save();
      ctx.translate(cmd.x, cmd.y);
      drawPath(ctx, stencil, { fill, stroke });
      ctx.restore();
    }
  }
}

/** 8. Draw text with alignment & baseline */
export function drawText(ctx, txt, x, y, {
  hAlign="left",      // left|center|right
  vAlign="alphabetic" // top|middle|bottom|alphabetic etc.
}={}) {
  ctx.textAlign    = hAlign;
  ctx.textBaseline = vAlign;
  ctx.fillText(txt, x, y);
}

/** 9. Soft clip check before drawing */
export function softClip(ctx, path, drawFn) {
  const pt = ctx.getTransform()
    .invertSelf()
    .transformPoint(ctx.currentPath[0]); // first path point
  if (pointInPoly(pt.x, pt.y, path)) drawFn();
}

/** 10. Safe image draw */
export function drawImageSafe(ctx, img, sx, sy, sw, sh, dx, dy, dw, dh) {
  try { ctx.drawImage(img, sx,sy,sw,sh, dx,dy,dw,dh) }
  catch(e) { console.error(`drawImage failed (${e.message})`); }
}
