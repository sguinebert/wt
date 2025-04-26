/* -------------------------------------------------------------------------
 * WTableView – modern rewrite (ES 2022+)
 * -------------------------------------------------------------------------
 *  import WTableView from './WTableView.js';
 *  const table = new WTableView({
 *      el: document.querySelector('.my-table-view'),
 *      contentsContainer: '.Wt-tv-contents-container',
 *      headerContainer: '.Wt-tv-header-container',
 *      headerColumnsContainer: '.Wt-tv-header-columns-container',
 *      selectedClass: 'selected',
 *      initialScrollTop: 0,
 *      rtl: document.body.classList.contains('Wt-rtl'),
 *      onScrolled: (x, y, width, height) => {  custom scroll handler  },
 *      onColumnResized: (columnId, newWidth) => {  custom resize handler  },
 *      onDropEvent: (rowIdx, columnId, sourceId, mimeType) => {  handle drop  },
 *      onRowDropEvent: (rowIdx, columnId, sourceId, mimeType, side) => { handle row drop },
 *  });
 * ------------------------------------------------------------------------- */

export default class WTableView {
  constructor(config) {
    this.el = typeof config.el === 'string' ? document.querySelector(config.el) : config.el;
    this.contentsContainer = typeof config.contentsContainer === 'string' ?
      this.el.querySelector(config.contentsContainer) : config.contentsContainer;
    this.headerContainer = typeof config.headerContainer === 'string' ?
      this.el.querySelector(config.headerContainer) : config.headerContainer;
    this.headerColumnsContainer = typeof config.headerColumnsContainer === 'string' ?
      this.el.querySelector(config.headerColumnsContainer) : config.headerColumnsContainer;
    this.selectedClass = config.selectedClass;
    this.initialScrollTop = config.initialScrollTop;
    this.rtl = config.rtl;

    this.onScrolled = config.onScrolled;
    this.onColumnResized = config.onColumnResized;
    this.onDropEvent = config.onDropEvent;
    this.onRowDropEvent = config.onRowDropEvent;

    this.setupEvents();
    this.initResizeObserver();

    if (this.initialScrollTop !== 0) {
      this.contentsContainer.scrollTop = this.initialScrollTop;
    }
  }

  setupEvents() {
    this.contentsContainer.addEventListener('scroll', () => this.syncScroll());
    this.headerContainer.addEventListener('scroll', () => this.syncHeaderScroll());
  }

  syncScroll() {
    this.headerContainer.scrollLeft = this.contentsContainer.scrollLeft;
    this.headerColumnsContainer.scrollTop = this.contentsContainer.scrollTop;
    if (this.onScrolled) {
      this.onScrolled(
        this.getScrollLeft(),
        this.contentsContainer.scrollTop,
        this.contentsContainer.clientWidth,
        this.contentsContainer.clientHeight
      );
    }
  }

  syncHeaderScroll() {
    this.contentsContainer.scrollLeft = this.headerContainer.scrollLeft;
  }

  getScrollLeft() {
    if (!this.rtl) return this.contentsContainer.scrollLeft;
    return this.contentsContainer.scrollWidth - this.contentsContainer.clientWidth - this.contentsContainer.scrollLeft;
  }

  initResizeObserver() {
    const resizeObserver = new ResizeObserver(() => {
      if (this.onScrolled) {
        this.onScrolled(
          this.getScrollLeft(),
          this.contentsContainer.scrollTop,
          this.contentsContainer.clientWidth,
          this.contentsContainer.clientHeight
        );
      }
    });
    resizeObserver.observe(this.contentsContainer);
  }

  resizeColumn(columnId, delta) {
    const columnSelector = `.col-${columnId}`;
    const header = this.headerContainer.querySelector(columnSelector);
    const content = this.contentsContainer.querySelector(columnSelector);

    const newWidth = parseInt(header.style.width, 10) + delta;
    header.style.width = `${newWidth}px`;
    content.style.width = `${newWidth}px`;

    if (this.onColumnResized) {
      this.onColumnResized(columnId, newWidth);
    }
  }

  handleDragDrop(action, object, event, sourceId, mimeType) {
    const item = event.target.closest('.Wt-tv-c');
    if (!item) return;

    const columnId = parseInt(item.parentElement.dataset.columnId, 10);
    const rowIdx = parseInt(item.dataset.rowIdx, 10);

    if (action === 'drop' && this.onDropEvent) {
      this.onDropEvent(rowIdx, columnId, sourceId, mimeType);
    }
  }

  scrollTo(x, y) {
    this.contentsContainer.scrollLeft = x;
    this.contentsContainer.scrollTop = y;
  }

  resetScroll() {
    this.scrollTo(0, 0);
  }
}