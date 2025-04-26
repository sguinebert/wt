// ES2022 modern tooltip utility – no global WT dependency
// Usage example:
//   import { attachTooltip } from "./tooltip.es2022.js";
//   attachTooltip(myNode, {
//       content: "Hi there!",              // string | HTMLElement | () => string | HTMLElement
//       deferred: true,                     // if true the loader will request content lazily via loadText()
//       loadText: () => fetch('/hint').then(r=>r.text()), // async supplier (only when deferred)
//       innerClass: 'tooltip-inner',        // default CSS classes
//       outerClass: 'tooltip-outer'
//   });

const _instances = new WeakMap();

/** @typedef {Object} TooltipOptions
 *  @property {string|HTMLElement|(()=>string|HTMLElement|Promise<any>)} content – initial or lazy content
 *  @property {boolean} [deferred=false] – if true wait until pointer‑hover before resolving "content"
 *  @property {() => Promise<string|HTMLElement>} [loadText] – async supplier when deferred
 *  @property {number}  [delay=500] – ms before showing
 *  @property {number}  [hideDelay=200] – ms before hiding after leave
 *  @property {number}  [offset=10] – px diagonal offset from the pointer
 *  @property {string}  [innerClass='WtToolTipInner']
 *  @property {string}  [outerClass='WtToolTipOuter']
 */

export function attachTooltip(target, opts /** @type {TooltipOptions} */) {
  // upgrade existing instance or create a new one
  if (_instances.has(target)) {
    _instances.get(target).update(opts);
    return;
  }
  _instances.set(target, new Tooltip(target, opts));
}

class Tooltip {
  #target; #opts; #showTimer; #hideTimer; #outer; #overTooltip=false;

  constructor(target, opts){
    this.#target = target;
    this.update(opts);
    this.#initListeners();
  }

  update(opts){
    // merge with defaults
    const d = {
      delay: 500,
      hideDelay: 200,
      offset: 10,
      innerClass: 'WtToolTipInner',
      outerClass: 'WtToolTipOuter',
      deferred: false,
      content: '',
    };
    this.#opts = Object.freeze({...d, ...opts});
  }

  /* ---------------- private ---------------- */
  #initListeners(){
    // Use Pointer Events for unified mouse/touch
    this.#target.addEventListener('pointerenter',  e=>this.#scheduleShow(e));
    this.#target.addEventListener('pointermove',   e=>this.#scheduleShow(e));
    this.#target.addEventListener('pointerleave',  ()=>this.#scheduleHide());
  }

  #scheduleShow(evt){
    clearTimeout(this.#showTimer);
    clearTimeout(this.#hideTimer);
    const {delay} = this.#opts;
    const coords = {x: evt.pageX, y: evt.pageY};
    this.#showTimer = setTimeout(()=> this.#show(coords), delay);
  }

  #scheduleHide(){
    clearTimeout(this.#showTimer);
    if(!this.#outer) return;
    const {hideDelay} = this.#opts;
    this.#hideTimer = setTimeout(()=>{
      if(this.#overTooltip) return; // cancelled by entering tooltip
      this.#destroy();
    }, hideDelay);
  }

  async #show({x,y}){
    if(this.#outer) return; // already visible

    let content = this.#opts.content;
    if(this.#opts.deferred && this.#opts.loadText){
      content = await this.#opts.loadText();
    }
    if (typeof content === 'function') content = await content();

    // build DOM lazily
    const inner = document.createElement('div');
    inner.className = this.#opts.innerClass;
    if (content instanceof HTMLElement) inner.appendChild(content);
    else inner.innerHTML = String(content);

    const outer = document.createElement('div');
    outer.className = this.#opts.outerClass;
    outer.style.position = 'absolute';
    outer.style.pointerEvents = 'auto';
    outer.appendChild(inner);

    document.body.appendChild(outer);
    this.#outer = outer;

    // basic viewport fit: position then clamp into view
    const off = this.#opts.offset;
    outer.style.left = `${x + off}px`;
    outer.style.top  = `${y + off}px`;

    const rect = outer.getBoundingClientRect();
    const vw = innerWidth, vh = innerHeight;
    if(rect.right > vw)  outer.style.left = `${vw - rect.width - 4}px`;
    if(rect.bottom > vh) outer.style.top  = `${vh - rect.height - 4}px`;

    // z‑index: stack above any modal/dialog (assumes they are in 10xx range)
    const maxZ = [...document.querySelectorAll('.Wt-dialog,.modal,.modal-dialog')]
                    .reduce((m,e)=>Math.max(m, +getComputedStyle(e).zIndex||0), 0);
    if(maxZ) outer.style.zIndex = String(maxZ + 1000);

    // tooltip hover hold‑on logic
    outer.addEventListener('pointerenter', ()=>{ this.#overTooltip=true; clearTimeout(this.#hideTimer); });
    outer.addEventListener('pointerleave', ()=>{ this.#overTooltip=false; this.#scheduleHide(); });
  }

  #destroy(){
    this.#outer?.remove();
    this.#outer = null;
  }
}