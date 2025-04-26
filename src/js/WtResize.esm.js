// layout-helpers.js ---------------------------------------------------------
const flexCss = `
.wt-column          { display:flex; flex-direction:column; min-height:0; }
.wt-column > *      { flex: 0 0 auto; }
.wt-column > *.stretch,
.wt-column > :last-child:not(.nostretch) { flex: 1 1 auto; min-height:0; }
.wt-children        { display:flex; flex-direction:column; min-height:0; }
.wt-children > *    { flex: 1 1 auto; min-height:0; }
`;
if(!document.querySelector('style[data-wt-layout]')){
  const s = document.createElement('style');
  s.dataset.wtLayout=''; s.textContent = flexCss;
  document.head.appendChild(s);
}

export class LayoutObserver {
  /** @type {ResizeObserver} */           #ro;

  constructor(){
    // one global ResizeObserver — keep it cheap
    this.#ro = new ResizeObserver(entries=>{
      for(const e of entries){
        const el = e.target;
        // if a container got an explicit pixel height dynamically: no extra JS
        // is needed – flex layout already copes. The observer simply forces a
        // reflow when the element’s box changed, which the browser does anyway
        // on its own; nothing to do here.
      }
    });
  }

  /**
   * Observe a container that should lay out its children vertically.
   * Add either `wt-column` (stretch only last) or `wt-children` (stretch all).
   *
   * @param {HTMLElement} container
   * @param {object}      [opts]
   * @param {boolean}     [opts.stretchLast=true]
   * @returns {()=>void}  a disconnect function
   */
  observe(container,{stretchLast=true}={}){
    if(!container) throw new Error('LayoutObserver: container required');

    container.classList.add(stretchLast? 'wt-column':'wt-children');
    this.#ro.observe(container);
    return ()=> this.#ro.unobserve(container);
  }
}

/* --------------------------------------------------------------------- */
/*  OPTIONAL: tiny helpers for legacy code that still calls wtResize()   */
/* --------------------------------------------------------------------- */
export function setExplicitHeight(el,hPx){
  el.style.height = Number.isFinite(hPx)&&hPx>=0 ? `${hPx}px` : '';
  // flexbox takes over – nothing else needed
}
