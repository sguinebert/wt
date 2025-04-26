/**
 * Lightweight, dependency-free popup widget.
 *
 * Usage example
 *   import WPopupWidget from './WPopupWidget.js';
 *
 *   const popup   = document.querySelector('#myPopup');
 *   const trigger = document.querySelector('#btn');
 *   const pw      = new WPopupWidget(popup, { transient : true,
 *                                             autoHideDelay : 200,
 *                                             shown : false });
 *
 *   trigger.addEventListener('click', () => pw.show(trigger));
 */
export default class WPopupWidget {
  /** @type {HTMLElement} */                  #el;
  /** @type {boolean}     */                  #transient;
  /** @type {number}      */                  #autoDelay;
  /** @type {number|null} */                  #hideTimer  = null;
  /** @type {PointerEvent|null} */            #pointer     = null;
  /** @type {() => void}? */                  #onShowCb;
  /** @type {() => void}? */                  #onHideCb;

  /**
   * @param {HTMLElement} element  DOM element that acts as the popup
   * @param {object}      [opts]
   * @param {boolean}     [opts.transient=false]  Hide when clicking elsewhere
   * @param {number}      [opts.autoHideDelay=0]  ms after which it hides on
   *                                              pointer-leave (0 = never)
   * @param {boolean}     [opts.shown=false]      Start visible?
   */
  constructor (element,
               { transient = false,
                 autoHideDelay = 0,
                 shown = false } = {}) {

    if (!(element instanceof HTMLElement)) {
      throw new TypeError('WPopupWidget: element must be an HTMLElement');
    }

    this.#el        = element;
    this.#transient = transient;
    this.#autoDelay = autoHideDelay;

    // Modern way: semantic boolean, not CSS string juggling
    element.hidden  = !shown;

    // Expose for any legacy code that still expects el.wtPopup
    element.wtPopup = this;

    // ---- Own listeners -------------------------------------------------
    element.addEventListener('pointerenter', this.#onEnter);
    element.addEventListener('pointerleave', this.#onLeave);

    // When the first pointer goes down inside the popup we capture it so a
    // quick drag outside doesn’t drop the “leave” event.
    element.addEventListener('pointerdown',   e => {
      try { element.setPointerCapture(e.pointerId); } catch(_) {}
    });

    if (shown) queueMicrotask(() => this.#handleShown());
  }

  /* ---------------- PUBLIC API -------------------------------------- */

  /** Optional hook called after the popup became visible */
  set onShow (fn) { this.#onShowCb = fn; }
  /** Optional hook called after the popup became hidden  */
  set onHide (fn) { this.#onHideCb = fn; }

  /**
   * Shows the popup.
   *
   * @param {HTMLElement|null} anchor  Widget it should align to
   * @param {'vertical'|'horizontal'} side  How to position (default: vertical)
   */
  show (anchor = null, side = 'vertical') {
    if (!this.#el.hidden) return;

    this.#el.hidden = false;

    // Your own geometry helper keeps its job
    if (anchor) WT.positionAtWidget(this.#el, anchor, side);

    this.#handleShown();
  }

  /** Hides the popup if visible */
  hide () {
    if (this.#el.hidden) return;
    this.#el.hidden = true;
    this.#handleHidden();
  }

  /**
   * Turns *transient* mode on/off and (optionally) updates the delay.
   * While transient, a pointer-down outside the popup closes it.
   */
  setTransient (flag, delay = this.#autoDelay) {
    this.#transient = !!flag;
    this.#autoDelay = delay;
    // Re-bind depending on current visibility
    if (this.#transient && !this.#el.hidden) {
      document.addEventListener('pointerdown', this.#onDocPointer, true);
    } else {
      document.removeEventListener('pointerdown', this.#onDocPointer, true);
    }
  }

  /** Whether the popup is currently visible */
  get visible () { return !this.#el.hidden; }

  /** Underlying element if direct DOM access is needed */
  get element () { return this.#el; }

  /* ---------------- PRIVATE helpers --------------------------------- */

  #onEnter = () => clearTimeout(this.#hideTimer);

  #onLeave = () => {
    clearTimeout(this.#hideTimer);
    if (this.#transient && this.#autoDelay > 0) {
      this.#hideTimer = setTimeout(() => this.hide(), this.#autoDelay);
    }
  };

  // Capture all pointer-downs during transient mode
  #onDocPointer = ev => {
    // Ignore if click happened inside popup
    if (!this.#el.contains(ev.target)) this.hide();
  };

  #handleShown () {
    if (this.#transient) {
      document.addEventListener('pointerdown', this.#onDocPointer, true);
    }
    this.#onShowCb?.();
  }

  #handleHidden () {
    if (this.#transient) {
      document.removeEventListener('pointerdown', this.#onDocPointer, true);
    }
    clearTimeout(this.#hideTimer);
    this.#onHideCb?.();
  }
}
