/*  Modern WPopupMenu  –  ES-2022+  */

export default class WPopupMenu {
  #root;                  // <ul>  root element of the popup
  #autoHideDelay;         // ms
  #hideTimer = null;      // setTimeout handle
  #currentItem = null;    // <li> currently “hot” (hovered) item
  #listeners = new Map();      // array of [target, type, fn, capture] for cleanup

  /* ---------- life-cycle ---------- */

  constructor(rootUl, autoHideDelay = 700) {
    if (!(rootUl instanceof HTMLElement))
      throw new TypeError('WPopupMenu needs a UL/OL root element');

    this.#root          = rootUl;
    this.#autoHideDelay = autoHideDelay;

    /* event delegation = single listener for entire list */
    this.#on(this.#root, 'pointerover',  this.#handlePointerOver);
    this.#on(this.#root, 'pointerleave', this.#scheduleHide);
    this.#on(this.#root, 'pointerdown',  e => e.stopPropagation()); // keep focus
  }

  /** Public: show at given x/y (page-coords). */
  show({x, y}) {
    clearTimeout(this.#hideTimer);
    this.#root.style.cssText = `
      position: absolute; display: block;
      left:${x}px; top:${y}px;
    `;

    /* Close when user clicks elsewhere or presses ESC. */
    this.#on(document, 'pointerdown',  this.#hideOnOutsideClick, true);
    this.#on(document, 'keydown',      this.#escHandler,         true);
  }

  /** Public: hide programmatically. */
  hide() { this.#doHide(); }

  /* ---------- private helpers ---------- */

  /** attach listener & remember for easy cleanup */
  #on(target, type, fn, capture = false) {
    target.addEventListener(type, fn, capture);
    // store a reference so we can remove it later ↓
    (this.#listeners ??= []).push([target, type, fn, capture]);
  }

  #offAll() {
    this.#listeners?.forEach(([t, ty, fn, cap]) => t.removeEventListener(ty, fn, cap));
    this.#listeners = [];
  }

  /** lazy auto-hide */
  #scheduleHide = () => {
    clearTimeout(this.#hideTimer);
    if (this.#autoHideDelay >= 0)
      this.#hideTimer = setTimeout(() => this.#doHide(), this.#autoHideDelay);
  };

  #doHide() {
    clearTimeout(this.#hideTimer);
    this.#root.style.display = 'none';
    this.#setActive(null);
    this.#offAll();                       // detach global listeners
    this.#root.dispatchEvent(new CustomEvent('cancel'));
  }

  #hideOnOutsideClick = e => {
    if (!this.#root.contains(e.target)) this.#doHide();
  };

  #escHandler = e => { if (e.key === 'Escape') this.#doHide(); };

  /** return submenu UL inside an LI (if any) */
  #submenu(li) { return li.querySelector(':scope > ul, :scope > ol'); }

  #setActive(li) {
    if (this.#currentItem && this.#currentItem !== li)
      this.#currentItem.classList.remove('active');

    if (li) li.classList.add('active');
    this.#currentItem = li;
  }

  #handlePointerOver = e => {
    const li = e.target.closest('li', this.#root);
    if (!li) return;

    this.#setActive(li);
    this.#showSubMenu(li);
  };

  #showSubMenu(li) {
    /* Collapse all non-ancestors */
    this.#root.querySelectorAll('ul,ol').forEach(u => {
      if (!u.contains(li)) u.style.display = 'none';
    });

    const menu = this.#submenu(li);
    if (!menu) return;

    menu.style.display = 'block';

    /* simple horizontal fly-out */
    const {right, top} = li.getBoundingClientRect();
    Object.assign(menu.style, {
      position:'absolute',
      left :`${right + window.scrollX}px`,
      top  :`${top   + window.scrollY}px`
    });
  }
}
