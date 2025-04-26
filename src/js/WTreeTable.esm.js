// WTreeTable.js ------------------------------------------------------------
export default class WTreeTable {
  /** @type {HTMLElement} */ #table;
  #content; #spacer; #ro;

  /**
   * @param {HTMLElement} table – `<div class="Wt-treeTable">…`
   *                              must contain `.Wt-content` + `.Wt-sbspacer`
   */
  constructor(table){
    if(!table) throw new Error('WTreeTable: table element required');

    this.#table   = table;
    this.#content = table.querySelector('.Wt-content');
    this.#spacer  = table.querySelector('.Wt-sbspacer');

    // expose for legacy code that might still poke at it
    table.wtObj = this;

    // auto-adjust on any resize of the table itself
    this.#ro = new ResizeObserver(_=> this.#update());
    this.#ro.observe(table);

    // update on DOM changes inside the scrollable region
    new MutationObserver(_=> this.#update())
      .observe(this.#content,{childList:true,subtree:true});

    this.#update();                       // initial pass
  }

  /* ------------------------------------------------------------------ */
  /*  Legacy-compat public method (kept the same name & signature)      */
  /* ------------------------------------------------------------------ */
  /**
   * @param {HTMLElement} _el   – unused legacy arg
   * @param {number}      _w    – (width, ignored; flex / CSS handles it)
   * @param {number}      h     – full height in px (-1 = “auto”)
   * @param {boolean}     setSize
   */
  wtResize(_el, _w, h, setSize){
    if(setSize){
      this.#table.style.height = h>=0 ? `${h}px` : '';
    }
    this.#update();
  }

  /* ------------------------------------------------------------------ */
  /*  Internals                                                         */
  /* ------------------------------------------------------------------ */
  #update(){
    /* 1. show / hide spacer depending on scroll-bar presence */
    const needsSpacer = this.#content.scrollHeight > this.#content.clientHeight;
    this.#spacer.hidden = !needsSpacer;

    /* 2. if table has an explicit (pixel) height → give the “body” panel
          the remaining space under the header                          */
    const declared = parseFloat(getComputedStyle(this.#table).height);
    const head     = this.#table.firstElementChild;
    const body     = this.#table.lastElementChild;          // .Wt-content’s wrapper

    if(Number.isFinite(declared) && declared>0){
      const remain = declared - head.getBoundingClientRect().height;
      body.style.height = remain>0 ? `${remain}px` : '0';
    }else{
      body.style.height = '';
    }
  }

  /** stop observing when you dispose the widget */
  disconnect(){ this.#ro.disconnect(); }
}

/* ---------- Optional helper: easy usage -----------------------------
import WTreeTable from './WTreeTable.js';
document.querySelectorAll('.Wt-treeTable').forEach(t=>new WTreeTable(t));
--------------------------------------------------------------------- */
