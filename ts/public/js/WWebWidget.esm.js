/* ------------------------------------------------------------------
 *  animate-display.js  ⚡️ ES-module rewrite of legacy **animateDisplay**
 *
 *  Light, promise-based helper that toggles an element’s *display*
 *  while playing a WAAPI animation.  (Fallback: resolves instantly
 *  if Web-Animations are not supported.)
 * ------------------------------------------------------------------ */

export const Effect = Object.freeze({
  NONE             : 0x0,
  SLIDE_IN_LEFT    : 0x1,
  SLIDE_IN_RIGHT   : 0x2,
  SLIDE_IN_BOTTOM  : 0x3,
  SLIDE_IN_TOP     : 0x4,
  POP              : 0x5,
  FADE             : 0x100           // bit-flag ⟹ may be OR’ed with slide / pop
});

export const Timing = Object.freeze({
  EASE        : 0,
  LINEAR      : 1,
  EASE_IN     : 2,
  EASE_OUT    : 3,
  EASE_IN_OUT : 4
});

const easings = ['ease','linear','ease-in','ease-out','ease-in-out'];

/**
 * Play an *in* / *out* animation while switching `display`.
 *
 * @param {HTMLElement|string} el        element or id
 * @param {Object}  opt
 * @param {number}  opt.effect           ⬆ see *Effect* (default SLIDE_IN_LEFT)
 * @param {number}  opt.timing           ⬆ see *Timing* (default EASE)
 * @param {number}  opt.duration = 300   animation time in ms
 * @param {string}  opt.display  = ''    final css-display for “shown” state
 * @returns {Promise<void>}              resolves when animation finished
 */
export function animateDisplay (
  el,
  { effect = Effect.SLIDE_IN_LEFT,
    timing = Timing.EASE,
    duration = 300,
    display = '' } = {}
){
  el = typeof el === 'string' ? document.getElementById(el) : el;
  if (!el || !(el instanceof HTMLElement)) return Promise.resolve();

  const hide     = display === 'none';
  const curDisp  = getComputedStyle(el).display;
  if ((hide && curDisp === 'none') || (!hide && curDisp !== 'none'))
    return Promise.resolve();                         // nothing to do

  /* ---------------------- keyframe helpers ----------------------- */
  const kf = (prop,val)=>({[prop]:val});
  const slideX = pct => kf('transform',`translateX(${pct}%)`);
  const slideY = pct => kf('transform',`translateY(${pct}%)`);

  /** build *visible* / *hidden* keyframes */
  let visible = {}, hidden = {};
  switch (effect & 0xFF){
    case Effect.SLIDE_IN_LEFT   : visible=slideX(  0); hidden=slideX(-100); break;
    case Effect.SLIDE_IN_RIGHT  : visible=slideX(  0); hidden=slideX( 100); break;
    case Effect.SLIDE_IN_TOP    : visible=slideY(  0); hidden=slideY(-100); break;
    case Effect.SLIDE_IN_BOTTOM : visible=slideY(  0); hidden=slideY( 100); break;
    case Effect.POP             : visible=kf('transform','scale(1)'); hidden=kf('transform','scale(.2)'); break;
    default                     : visible={}; hidden={};
  }
  if (effect & Effect.FADE){
    visible.opacity = 1;
    hidden .opacity = 0;
  }

  const keyframes = hide ? [visible, hidden] : [hidden, visible];

  /* -------------------- no WAAPI? quick exit --------------------- */
  if (!el.animate) {
    el.style.display = display;
    return Promise.resolve();
  }

  /* ------------------------- animate ----------------------------- */
  if (!hide) el.style.display = display;           // make visible first
  const animation = el.animate(keyframes, {
    duration, easing:easings[timing] ?? 'ease', fill:'forwards'
  });

  return animation.finished.then(()=>{
    if (hide) el.style.display = 'none';
  }).catch(()=>{                                   // interrupted or cancelled
    if (hide) el.style.display = 'none';
  });
}
