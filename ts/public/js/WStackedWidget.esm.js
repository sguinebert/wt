/* ------------------------------------------------------------------
 * ⚡️ ES-module rewrite of legacy **WStackedWidget**
 *
 *  ▸ single-child-visible container  (display-swap + optional animation)
 *  ▸ remembers individual scrollLeft / scrollTop per child
 *  ▸ auto-grows / shrinks and re-dispatches a manual “resize” to children
 *  ▸ Web-Animations-API with CSS-fallback (slide / fade / pop)   – permits
 *      auto-reverse when navigating back in the child-list.
 * ------------------------------------------------------------------ */

const css   = name => getComputedStyle(name);
const px    = (el, prop) => parseFloat(css(el)[prop]) || 0;
const isBox = el => css(el).boxSizing === 'border-box';
const proper = el => el.nodeType===1 && !el.classList.contains('wt-reparented') &&
                     !el.classList.contains('resize-sensor');

export class WStackedWidget {
  /**
   * @param {HTMLElement} container   – the stacked-widget root
   * @param {Object} [opt]
   * @param {boolean} [opt.autoReverse=false] – reverse slide if index decreases
   */
  constructor (container,{autoReverse=false}={}) {
    if (!(container instanceof HTMLElement))
      throw new TypeError('First arg must be an element');
    this.root         = container;
    this.autoReverse  = autoReverse;
    this.#scrollX     = new Map();
    this.#scrollY     = new Map();
    new ResizeObserver(e=>this.#onResize(e[0].contentRect))
      .observe(container);
  }

  /* ---------- public API ---------- */

  /** show given child element (or index) */
  set current (child){
    child = typeof child==='number' ? this.#children()[child] : child;
    if (!child || !proper(child)) return;
    this.#swap(child);
  }
  get current(){
    return this.#children().find(c=>c.style.display!=='none');
  }

  /** explicit resize from host (width / height in px or –1 for auto) */
  resize (w,h){
    this.#resize(w,h,false);
  }

  /**
   * animate a child in (effects: 'slide-left|right|top|bottom' 'pop' 'fade')
   * @param {HTMLElement} child
   * @param {Object}      opt
   * @param {string}      [opt.effect='slide-left']
   * @param {number}      [opt.duration=300]   – ms
   * @param {string}      [opt.easing='ease']
   */
  animateChild (child,{effect='slide-left',duration=300,easing='ease'}={}) {
    child = typeof child==='number' ? this.#children()[child] : child;
    if (!child || child===this.current) return;
    const from  = this.current, to = child;
    const cont  = this.root;

    /* record scroll of ‘from’ & restore for ‘to’ ------------------- */
    this.#scrollX.set(from, cont.scrollLeft);
    this.#scrollY.set(from, cont.scrollTop);
    cont.scrollLeft = this.#scrollX.get(to)??0;
    cont.scrollTop  = this.#scrollY.get(to)??0;

    /* prep sizes so animation can be absolute-positioned ---------- */
    const {width:w,height:h} = cont.getBoundingClientRect();
    Object.assign(to.style,{
      position:'absolute',left:px(cont,'paddingLeft')+'px',
      top:px(cont,'paddingTop')+'px',width:w-px(cont,'paddingLeft')-
      px(cont,'paddingRight')+'px',height:h-px(cont,'paddingTop')-
      px(cont,'paddingBottom')+'px',display:'block'
    });

    /* direction auto-flip ----------------------------------------- */
    if (this.autoReverse){
      const idx    = i=>this.#children().indexOf(i);
      if (idx(to)<idx(from)) effect = effect.replace('left','right')
                                            .replace('right','left')
                                            .replace('top','bottom')
                                            .replace('bottom','top');
    }

    const keyframes = {
      'slide-left'  : [{transform:`translateX(${w}px)`}, {transform:'none'}],
      'slide-right' : [{transform:`translateX(-${w}px)`}, {transform:'none'}],
      'slide-top'   : [{transform:`translateY(${h}px)`}, {transform:'none'}],
      'slide-bottom': [{transform:`translateY(-${h}px)`}, {transform:'none'}],
      'pop'         : [{transform:'scale(.8)',opacity:0},
                       {transform:'none',     opacity:1}],
      'fade'        : [{opacity:0},{opacity:1}]
    }[effect] ?? keyframes['slide-left'];

    /* hide ‘from’ at finish */
    const fin = ()=>{ from.style.display='none'; from.style.position=''; 
                      to.style.position=''; };

    /* WAAPI preferred, fallback to CSS class ----------------------- */
    if (to.animate){
      const a1 = from.animate([...keyframes].reverse(),
                              {duration,easing,fill:'forwards'});
      const a2 = to  .animate(keyframes,{duration,easing,fill:'forwards'});
      a2.finished.then(fin,fin);
    } else {
      // CSS fallback – add classes w/ predefined @keyframes (.slide-left etc.)
      from.classList.add('out',effect);
      to  .classList.add('in' ,effect);
      const onEnd = e=>{
        from.classList.remove('out',effect);
        to  .classList.remove('in' ,effect);
        fin();
      };
      to.addEventListener('animationend',onEnd,{once:true});
    }
  }

  /* ---------- internal ---------- */

  #children(){ return [...this.root.childNodes].filter(proper); }

  #swap(child){
    const cur = this.current;
    if (cur===child) return;
    cur && (cur.style.display='none');
    child.style.display='';
    this.#restoreScroll(child);
  }

  #restoreScroll(c){
    const r=this.root;
    r.scrollLeft=this.#scrollX.get(c)??0;
    r.scrollTop =this.#scrollY.get(c)??0;
  }

  #onResize({width,height}){
    this.#resize(width,height,true);
  }

  #resize(w,h,fromObserver){
    const root = this.root;
    const hasH = h>=0;
    if (hasH) root.style.height=h+'px';
    for (const c of this.#children()){
      if (c.style.display==='none') continue;
      if (hasH){
        const m = px(c,'marginTop')+px(c,'marginBottom')+
                  (isBox(c)?0:px(c,'borderTopWidth')+px(c,'borderBottomWidth')+
                              px(c,'paddingTop')+px(c,'paddingBottom'));
        c.style.height=Math.max(0,h-m)+'px';
      } else c.style.height='';
      c.dispatchEvent(new Event('resize'));
    }
  }

  /* ---------- private fields ---------- */
  #scrollX; #scrollY;
}
