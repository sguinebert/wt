// centre-dialog.js  – ES-module, < 0.6 kB min.
/**
 * Centre a dialog in the viewport (and keep it centred on resize).
 *
 * @param {string} id                    element - id
 * @param {{x?:boolean,y?:boolean,watch?:boolean}} [opt]
 *        x      – centre horizontally   (default true)
 *        y      – centre vertically     (default true)
 *        watch  – recalc on window.resize (default true)
 *
 * @returns {() => void}  A function that
 *   1. re-centres the element, or
 *   2. if called with `false`, removes the resize listener.
 */
export const centreDialog = (id, { x = true, y = true, watch = true } = {}) => {
  const el = () => document.getElementById(id);          // late lookup

  const place = () => {
    const e = el();
    if (!e || e.style.display === 'none' || e.style.visibility === 'hidden') return;

    const vw = innerWidth, vh = innerHeight;
    x && (e.style.left = ((vw - e.offsetWidth)  >> 1) + 'px', e.style.marginLeft = 0);
    y && (e.style.top  = ((vh - e.offsetHeight) >> 1) + 'px', e.style.marginTop  = 0);

    e.style.visibility = 'visible';
  };

  // ── first run after DOM ready ──────────────────────────────────────────
  document.readyState === 'loading'
    ? addEventListener('DOMContentLoaded', place, { once: true })
    : place();

  // ── optional resize tracking ───────────────────────────────────────────
  const onResize = () => requestAnimationFrame(place);   // 1-tick throttle
  watch && addEventListener('resize', onResize, { passive: true });

  // expose helper / disposer
  return (active = true) => active ? place() : removeEventListener('resize', onResize);
};
