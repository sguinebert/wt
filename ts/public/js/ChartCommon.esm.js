// chart-common.js  (ES2022+, no fall-backs)
export const Cmd = Object.freeze({
  MOVE:0, LINE:1, CUBIC_C1:2, CUBIC_C2:3, CUBIC_END:4,
  QUAD_C:5,  QUAD_END:6, ARC_C:7, ARC_R:8, ARC_SWEEP:9
});

export const X = 0, Y = 1;

/* ---------- tiny helpers ---------- */
export const dist2 = ([x1,y1],[x2,y2]) => (x2-x1)**2+(y2-y1)**2;
export const withinRadius = (a,b,r) => dist2(a,b) <= r**2;
export const zoomLevel = f => Math.round(Math.log2(f))+1;
export const inRect = ({x,y},[l,t,w,h]) => x>=l && x<=l+w && y>=t && y<=t+h;

/* ---------- search utilities ---------- */
const binarySearch = (series, x, axis) => {
  // assumes anchor-points only, series sorted on axis
  let lo=0, hi=series.length-1;
  while(lo<=hi){
    const mid=Math.trunc((lo+hi)/2);
    const val=series[mid][axis];
    if(val===x) return mid;
    val<x ? lo=mid+1 : hi=mid-1;
  }
  return Math.max(0,hi);
};

/* ---------- public API ---------- */
export const closestPoint = (series, x, {horizontal=false}={})=>{
  if(!series.length) return null;
  const axis = horizontal?Y:X;
  const idx  = binarySearch(series,x,axis);
  const next = Math.min(idx+1, series.length-1);
  return Math.abs(series[idx][axis]-x) <
         Math.abs(series[next][axis]-x) ? series[idx] : series[next];
};

export const minMaxY = (series,{horizontal=false}={})=>{
  const axis = horizontal?X:Y;
  let min=Infinity,max=-Infinity;
  for(const [x,y,type] of series)
    if(type!==Cmd.CUBIC_C1 && type!==Cmd.CUBIC_C2 && type!==Cmd.QUAD_C){
      const v = horizontal?x:y;
      if(v<min) min=v;
      if(v>max) max=v;
    }
  return [min,max];
};

export const projection = (θ,[mx,my])=>{
  const c=Math.cos(θ),s=Math.sin(θ),h=-(mx*c+my*s);
  return [c*c,c*s,c*s,s*s,c*h+mx,s*h+my];   // DOM-matrix (a,b,c,d,e,f)
};

/**
 * Multiplies a 2-D point by a 2×3 affine matrix
 * m = [a, b, c, d, e, f]  →  | a c e |
 *                            | b d f |
 *                            | 0 0 1 |
 */
export const mulMatPt = ([a, b, c, d, e, f], [x, y]) => [a * x + c * y + e, b * x + d * y + f];

/**
* Convert a model-space point to canvas pixel coordinates.
*
* @param {[number,number]} pt          – model point
* @param {[number,number,number]} area – [x, y, w, h] of drawing area in px
* @param {[number,number,number,number]} model
*                        – [x, y, w, h] extents of model in model units
* @param {boolean} horizontal          – swap axes for horizontal charts
* @param {Array<number>} localXform    – optional 2×3 matrix applied last
* @returns {[number,number]} pixel point
*/
export function modelToCanvas(
  pt,
  area,
  model,
  horizontal = false,
  localXform = [1, 0, 0, 1, 0, 0]
) {
  const [mx, my, mw, mh] = model;
  const [ax, ay, aw, ah] = area;

  // Normalised unit-square coords in [0,1]²
  const u = horizontal
    ? [(pt[0] - mx) / mw, (pt[1] - my) / mh]      // plain
    : [(pt[0] - mx) / mw, 1 - (pt[1] - my) / mh]; // Y flipped (classic cartesian)

  // Scale to pixels
  const px = ax + u[0] * aw;
  const py = ay + u[1] * ah;

  // Optional local affine (zoom/pan)
  return mulMatPt(localXform, [px, py]);
}

/* ------------------------------------------------------------------ */
/* 2. y-range helper (used for auto-zoom)                              */
/* ------------------------------------------------------------------ */

/**
* Compute y-range for a slice of a curve that lies between lower/upper
* x bounds.  Works for simple poly-lines; Bézier support was dropped
* (too heavy for client-side – let the server pre-sample if needed).
*
* @param {Array<[number,number]>} points    – data in model coords
* @param {number} x0  – lower bound  (model units)
* @param {number} x1  – upper bound  (model units)
* @returns {{min:number,max:number}} y range or null if empty
*/
export function yRange(points, x0, x1) {
  const lo = Math.min(x0, x1), hi = Math.max(x0, x1);

  let min = Infinity;
  let max = -Infinity;

  for (const [x, y] of points) {
    if (x >= lo && x <= hi) {
      if (y < min) min = y;
      if (y > max) max = y;
    }
  }
  return min === Infinity ? null : { min, max };
}

/* ------------------------------------------------------------------ */
/* 3. Generic axis hit-test                                           */
/* ------------------------------------------------------------------ */

/**
* Hit-test either x- or y-axes that sit *around* the plotting rectangle.
*
* @param {{x:number,y:number}}  pt        – mouse coordinate in px
* @param {Array<{side:"min"|"max"|"both", width:number,
*                minOffset:number, maxOffset:number}>} axes
* @param {"horizontal"|"vertical"} orientation
*                    – pass "horizontal" to test X axes on a horizontal chart
* @param {[number,number,number,number]} plotRect – [x,y,w,h] in px
* @returns {number} index of the hit axis or -1
*/
export function hitTestAxis(pt, axes, orientation, plotRect) {
  const [x0, y0, w, h] = plotRect;
  const x1 = x0 + w, y1 = y0 + h;

  const isH = orientation === 'horizontal';

  const insideMain =
    isH ? pt.y >= y0 && pt.y <= y1
      : pt.x >= x0 && pt.x <= x1;

  if (!insideMain) return -1;

  for (let i = 0; i < axes.length; ++i) {
    const { side, width, minOffset, maxOffset } = axes[i];

    if (isH) { // X axis = horizontal strip left/right of plot
      if ((side === 'min' || side === 'both') &&
        pt.x >= x0 - minOffset - width && pt.x <= x0 - minOffset)
        return i;

      if ((side === 'max' || side === 'both') &&
        pt.x >= x1 + maxOffset && pt.x <= x1 + maxOffset + width)
        return i;
    } else {   // Y axis = vertical strip top/bottom of plot
      if ((side === 'min' || side === 'both') &&
        pt.y >= y1 + minOffset && pt.y <= y1 + minOffset + width)
        return i;

      if ((side === 'max' || side === 'both') &&
        pt.y >= y0 - maxOffset - width && pt.y <= y0 - maxOffset)
        return i;
    }
  }
  return -1;
}

/* All functions are side-effect-free and canvas/SVG-agnostic. */
