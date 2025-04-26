/* ------------------------------------------------------------------
   WDateEdit.es2022.js     (modern replacement for the old macro file)
   ------------------------------------------------------------------ */
   export default class WDateEdit {
    /** @param {HTMLInputElement} input  (the <input> driving the widget)
     *  @param {HTMLElement}      popup  (calendar or date-picker element)
     *  @param {number}           hoverZonePx – width of the click-zone at
     *                                         the right edge (default 40 px)
     */
    constructor (input, popup, hoverZonePx = 40) {
      if (!(input instanceof HTMLInputElement))
        throw new TypeError('WDateEdit: first arg must be an <input>');
      if (!(popup instanceof HTMLElement))
        throw new TypeError('WDateEdit: second arg must be an HTMLElement');
  
      // public handles
      this.input = input;
      this.popup = popup;
  
      // private state
      this.#hoverZone = hoverZonePx;
      this.#ac        = new AbortController();
      this.#bindBaseEvents();
      this.#setupPopup();
    }
  
    /* ───────────────────────── public helpers ────────────────────────── */
  
    /** clean-up listeners / timers (call before removing the node) */
    destroy () { this.#ac.abort(); }
  
    /* ───────────────────────── private fields ────────────────────────── */
  
    #hoverZone;           // px
    #downInZone = false;  // pointer started in the icon area?
    #ac;                  // AbortController for all listeners
  
    get #sig () { return { signal: this.#ac.signal }; }
    get #readOnly () {
      return this.input.readOnly || this.input.hasAttribute('readonly');
    }
  
    /* ───────────────────────── private methods ───────────────────────── */
  
    #bindBaseEvents () {
      const zoneHit = evt => this.#zoneRect().contains(evt);
  
      /** hover feedback */
      this.input.addEventListener('pointermove', e => {
        this.input.classList.toggle('hover', !this.#readOnly && zoneHit(e));
      }, this.#sig);
  
      /** leave → remove hover */
      this.input.addEventListener('pointerleave', () => {
        this.input.classList.remove('hover');
      }, this.#sig);
  
      /** pointer-down (start click) */
      this.input.addEventListener('pointerdown', e => {
        if (this.#readOnly || !zoneHit(e)) return;
        this.#downInZone = true;
        this.input.setPointerCapture(e.pointerId);
        this.input.classList.add('active', 'unselectable');
      }, this.#sig);
  
      /** pointer-up → open the calendar */
      this.input.addEventListener('pointerup', e => {
        if (!this.#downInZone) return;
        this.#downInZone = false;
        this.input.releasePointerCapture(e.pointerId);
        this.input.classList.remove('active', 'unselectable');
        this.#showPopup();
      }, this.#sig);
    }
  
    /** rectangle covering the right-hand hover zone */
    #zoneRect () {
      const { left, width, top, height } = this.input.getBoundingClientRect();
      return new DOMRect(left + width - this.#hoverZone, top,
                         this.#hoverZone, height);
    }
  
    /** one-time popup bootstrap */
    #setupPopup () {
      this.popup.style.display = 'none';
      this.popup.style.position = 'absolute';
      if (!this.popup.hasAttribute('role')) this.popup.setAttribute('role', 'dialog');
    }
  
    /** show popup & install outside-click auto-close */
    #showPopup () {
      const { left, bottom } = this.input.getBoundingClientRect();
      this.popup.style.left   = `${left  + window.scrollX}px`;
      this.popup.style.top    = `${bottom + 4 + window.scrollY}px`;
      this.popup.style.zIndex = '10000';
      this.popup.style.display = 'block';
  
      /* close when user clicks outside the picker */
      const outside = ({ target }) => {
        if (target === this.input || this.popup.contains(target)) return;
        this.#hidePopup();
      };
      window.addEventListener('pointerdown', outside, this.#sig);
    }
  
    #hidePopup () {
      this.popup.style.display = 'none';
      this.input.classList.remove('hover', 'active');
      // flush outside-click listener by restarting the controller
      this.#ac.abort();            // drop all listeners (incl. outside click)
      this.#ac = new AbortController();
      this.#bindBaseEvents();      // re-bind base events
    }
  }
  