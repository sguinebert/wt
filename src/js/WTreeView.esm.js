/* ------------------------------------------------------------------
 *  tree-view.js  ⚡️ modern ES-module rewrite of legacy **WTreeView**
 *
 *  ▸ no globals, no WT macros – a self-contained class
 *  ▸ pointer / wheel friendly, ResizeObserver aware
 *  ▸ event forwarding through `CustomEvent`s
 *  ▸ optional drag-&-drop helpers (item + between-rows)
 * ------------------------------------------------------------------ */

export default class WTreeView {
  /** @param {HTMLElement|string} root – element or id */
  constructor (
    root,
    {
      rowHeaderCount   = 0,
      selectedClass    = 'selected',
      itemDrops        = false,
      betweenRowDrops  = false
    } = {}
  ){
    this.el           = typeof root === 'string' ? document.getElementById(root) : root;
    this.contentsWrap = this.el.querySelector('[data-tree-contents]');
    this.headersWrap  = this.el.querySelector('[data-tree-headers]');
    this.rowHeaderCnt = rowHeaderCount;
    this.selectedCls  = selectedClass;
    this.itemDrops    = itemDrops;
    this.rowDrops     = betweenRowDrops;

    /* ---------- column-resize ---------- */
    this.el.addEventListener('pointerdown', e=>{
      const h = e.target.closest('.tree-col-resize');
      if (!h) return;
      e.preventDefault();
      const col   = h.parentElement;
      const start = e.clientX;
      const init  = col.getBoundingClientRect().width;

      const move  = ev=>{
        const dx = ev.clientX - start;
        col.style.width = `${Math.max(16,init+dx)}px`;
        this.#syncLayout();
        this.el.dispatchEvent(new CustomEvent('columnresize',{
          detail:{index:[...col.parentElement.children].indexOf(col),
                  width:parseInt(col.style.width)}
        }));
      };
      const up = ()=>{
        window.removeEventListener('pointermove',move);
        window.removeEventListener('pointerup',up);
      };
      window.addEventListener('pointermove',move);
      window.addEventListener('pointerup',up,{once:true});
    });

    /* ---------- click / double-click / press ---------- */
    this.el.addEventListener('click',     e=> this.#forward(e,'click'));
    this.el.addEventListener('dblclick',  e=> this.#forward(e,'dblclick'));
    this.el.addEventListener('pointerdown',e=> this.#forward(e,'mousedown'));
    this.el.addEventListener('pointerup', e=> this.#forward(e,'mouseup'));

    /* ---------- touch select helper ---------- */
    let touchTimer=0;
    this.el.addEventListener('touchstart',e=>{
      clearTimeout(touchTimer);
      touchTimer=setTimeout(()=>this.#forward(e,'touchselect'), e.touches.length>1?1e3:50);
    });
    this.el.addEventListener('touchmove', _=>clearTimeout(touchTimer));
    this.el.addEventListener('touchend' , _=>clearTimeout(touchTimer));

    /* ---------- drag & drop helpers ---------- */
    if (this.itemDrops || this.rowDrops){
      this.el.addEventListener('dragover', e=>{
        const info = this.#getItemInfo(e);
        if ((this.itemDrops && info.dropCell) ||
            (this.rowDrops  && info.row))
          e.preventDefault();
      });
      this.el.addEventListener('drop', e=>{
        const info = this.#getItemInfo(e);
        if (this.itemDrops && info.dropCell){
          this.#dispatch('itemdrop',{node:info.nodeId,col:info.col});
        }else if (this.rowDrops){
          const side = this.#ySide(info.row,e);
          this.#dispatch('rowdrop',{node:info.nodeId,side});
        }
      });
    }

    /* ---------- layout observers ---------- */
    this.ro = new ResizeObserver(()=> this.#syncLayout());
    this.ro.observe(this.el);
    this.#syncLayout();
  }

  /* ================================================================ */
  /* ------------------------ public API ---------------------------- */

  scrollToRow (row, behavior='auto', align='nearest'){
    const y = row * this.rowHeight;
    this.contentsWrap.scrollTo({top:y, behavior});
  }
  enableItemDrops   (v=true){ this.itemDrops = !!v; }
  enableRowDrops    (v=true){ this.rowDrops  = !!v; }

  /* ================================================================ */
  /* ----------------------- internals ------------------------------ */

  #dispatch(type,detail){ this.el.dispatchEvent(new CustomEvent(type,{detail})); }

  #forward (ev,type){
    const info = this.#getItemInfo(ev);
    if (info.col>=0){
      this.#dispatch(type,{node:info.nodeId,col:info.col,original:ev});
    }else if (!info.nodeId){
      this.#dispatch(`root${type}`,{original:ev});
    }
  }

  #getItemInfo (ev){
    let t = ev.target, col=-1,nodeId=null,dropCell=false,row=null;
    while (t && t!==this.el){
      if (t.matches('li'))        { nodeId=t.dataset.id; row=t; }
      if (t.matches('[data-col]')){ col = +t.dataset.col; dropCell=t.dataset.drop==='1'; }
      t = t.parentElement;
    }
    return {col,nodeId,dropCell,row};
  }
  #ySide(row,ev){
    const r = row.getBoundingClientRect();
    return (ev.clientY - r.top) < r.height/2 ? 'top' : 'bottom';
  }

  /* ---------- column / table width bookkeeping ---------- */
  #syncLayout (){
    const table     = this.contentsWrap.querySelector('table');
    const headerRow = this.headersWrap .querySelector('.tree-header-row');
    if (!table || !headerRow) return;

    /*  total width (scrollbar aware)  */
    const scBar = this.contentsWrap.offsetWidth - this.contentsWrap.clientWidth;
    const fullW = this.el.clientWidth - scBar;
    this.headersWrap.style.width = table.parentElement.style.width = `${fullW}px`;

    /*  auto-expand last column (if no explicit width) */
    const cells = headerRow.children;
    const last  = cells[cells.length-1];
    if (!last.style.width){
      let used=0;
      for (let i=0;i<cells.length-1;i++)
        used += cells[i].getBoundingClientRect().width;
      last.style.width = Math.max(16,fullW-used-2) + 'px';
    }
  }
}
