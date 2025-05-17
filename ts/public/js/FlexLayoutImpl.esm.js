/* -------------------------------------------------------------------------
 * FlexLayout – modern rewrite (ES 2022+)
 * -------------------------------------------------------------------------
 * import FlexLayout from './FlexLayout.js';
 * const flexLayout = new FlexLayout('#elementId');
 * flexLayout.adjust();
 * ------------------------------------------------------------------------- */

 export default class FlexLayout {
  constructor(selector) {
    this.el = typeof selector === 'string' ? document.querySelector(selector) : selector;

    if (this.el) {
      requestAnimationFrame(() => this.init());
      this.resizeObserver = new ResizeObserver(() => this.adjust());
      this.resizeObserver.observe(this.el);
    }
  }

  init() {
    [...this.el.children].forEach(c => {
      if (this.shouldSkip(c)) return;

      const overflow = getComputedStyle(c).overflow;
      if (!overflow || overflow === 'visible') {
        c.style.overflow = 'hidden';
      }
    });
  }

  shouldSkip(c) {
    return (
      c.style.display === 'none' ||
      c.classList.contains('out') ||
      c.classList.contains('resize-sensor')
    );
  }

  adjust() {
    requestAnimationFrame(() => {
      const children = [...this.el.children].filter(c => !this.shouldSkip(c));

      const totalStretch = children.reduce((total, c) => {
        const flg = c.getAttribute('flg');
        if (flg === '0') return total;

        const flexGrow = parseFloat(getComputedStyle(c).flexGrow) || 0;
        return total + flexGrow;
      }, 0);

      children.forEach(c => {
        if (c.resizeSensor) c.resizeSensor.trigger?.();

        const flg = c.getAttribute('flg');
        let stretch = 1;

        if (totalStretch !== 0) {
          stretch = flg === '0' ? 0 : parseFloat(getComputedStyle(c).flexGrow) || 0;
        }

        c.style.flexGrow = stretch;
      });
    });
  }

  disconnect() {
    if (this.resizeObserver) this.resizeObserver.disconnect();
  }
}
