// ScrollVisibility.js
// Drop-in helper to emit a `scrollVisibilityChanged` CustomEvent
// whenever an element (plus optional margin) moves in/out of view.

export default class ScrollVisibility {
  /** @type {WeakMap<HTMLElement,Entry>} */
  #entries = new WeakMap();

  /** single IntersectionObserver shared by all elements (desktop & mobile) */
  #io = new IntersectionObserver(
    entries => entries.forEach(e => {
      const rec = this.#entries.get(e.target);
      if (!rec) return;

      // element might be display:none or visibility:hidden
      const cssVisible = this.#cssVisible(e.target);

      const inViewport =
        cssVisible && (e.intersectionRatio > 0 || !e.isIntersecting && rec.margin > 0);

      if (inViewport !== rec.visible) {
        rec.visible = inViewport;
        e.target.dispatchEvent(
          new CustomEvent('scrollVisibilityChanged', { detail: inViewport })
        );
      }
    }),
    { root: null, rootMargin: '0px', threshold: 0 }
  );

  /**
   * Observe an element.
   * @param {HTMLElement} el
   * @param {number} [margin=0] – positive pixels around the viewport that count as “visible”
   */
  add(el, margin = 0) {
    if (!el || !(el instanceof HTMLElement)) throw new TypeError('add() expects an HTMLElement');
    this.remove(el);                               // (re)-add with new options

    const entry = { margin, visible: false };
    this.#entries.set(el, entry);

    // update observer with individual margin
    this.#io.unobserve(el);
    this.#io.observe(el);
    el.style.scrollMargin = `${margin}px`;          // helps browser align when using :target etc.
  }

  /** Stop observing an element */
  remove(el) {
    if (this.#entries.delete(el)) {
      this.#io.unobserve(el);
    }
  }

  /* ---------- helpers ---------- */

  #cssVisible(el) {
    let node = el;
    while (node && node.nodeType === 1) {
      const style = getComputedStyle(node);
      if (style.display === 'none' || style.visibility === 'hidden' || node.classList.contains('out'))
        return false;
      node = node.parentElement;
    }
    return true;
  }
}
