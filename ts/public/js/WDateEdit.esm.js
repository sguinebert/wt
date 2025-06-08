/* ------------------------------------------------------------------
   WDateEdit.es2022.js     (modern replacement for the old macro file)
   ------------------------------------------------------------------ */

  import WDateValidator from "./WDateValidator.esm.js";

   export default class WDateEdit {
  /** @param {HTMLInputElement} input  (the <input> driving the widget)
   *  @param {HTMLElement}      popup  (calendar or date-picker element)
   *  @param {Object}           config - Configuration options
   *  @param {number}           [config.hoverZonePx] – width of the click-zone at
   *                                         the right edge (default 40 px)
   *  @param {Object|boolean}   [config.validator] - WDateValidator options or true to use default
   *  @param {Function}         [config.onValidationFailed] - Called when validation fails
   */
    constructor (input, popup, config = {}) {
      if (!(input instanceof HTMLInputElement))
        throw new TypeError('WDateEdit: first arg must be an <input>');
      if (!(popup instanceof HTMLElement))
        throw new TypeError('WDateEdit: second arg must be an HTMLElement');
  
      // public handles
      this.input = input;
      this.popup = popup;

      this.tableClass = config.tableClass || 'wt-calendar';
      this.currentDate = config.currentDate || new Date();
      this.selectedDate = config.selectedDate || null;
      this.startWeekOnMonday = config.startWeekOnMonday || false;

      // Setup validation
      this.setupValidator(config.validator);
      this.onValidationFailed = config.onValidationFailed || null;
      
      // Day names (short and full versions)
      this.dayNames = config.dayNames || 
        ['Su', 'Mo', 'Tu', 'We', 'Th', 'Fr', 'Sa'];
      
      this.fullDayNames = config.fullDayNames || 
        ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
      
      if (this.startWeekOnMonday) {
        // Rotate arrays to start with Monday
        this.dayNames = [...this.dayNames.slice(1), this.dayNames[0]];
        this.fullDayNames = [...this.fullDayNames.slice(1), this.fullDayNames[0]];
      }
      
      this.monthNames = config.monthNames || 
        ['January', 'February', 'March', 'April', 'May', 'June', 
        'July', 'August', 'September', 'October', 'November', 'December'];
      
      this.onDateSelected = config.onDateSelected || null;
  
      // private state
      this.#hoverZone = config.hoverZonePx || 40; // px
      this.#ac        = new AbortController();
      this.#bindBaseEvents();
      this.#setupPopup();
      this.#setupInputValidation();

    }
  
    /* ───────────────────────── public helpers ────────────────────────── */
  
    /** clean-up listeners / timers (call before removing the node) */
    destroy () { this.#ac.abort(); }


  /**
   * Setup the date validator
   * @param {Object|boolean} validatorConfig - Validator config or true for default
   */
  setupValidator(validatorConfig) {
    if (!validatorConfig) {
      this.validator = null;
      return;
    }

    // If validatorConfig is true or an object, create a validator
    if (validatorConfig === true) {
      // Default validation settings
      this.validator = new WDateValidator({
        mandatory: true,
        formats: [
          {
            regexp: '(\\d{4})-(\\d{1,2})-(\\d{1,2})',
            getYear: match => parseInt(match[1], 10),
            getMonth: match => parseInt(match[2], 10) - 1, // 0-based month
            getDay: match => parseInt(match[3], 10)
          }
        ],
        min: null, // no min date
        max: null, // no max date
        messages: {
          blank: 'Date is required',
          format: 'Invalid date format, use YYYY-MM-DD',
          tooSmall: 'Date is before minimum date',
          tooLarge: 'Date is after maximum date'
        }
      });
    } else if (typeof validatorConfig === 'object') {
      // Use provided validation settings
      this.validator = new WDateValidator(validatorConfig);
    }
  }

  /**
   * Validate the current input value
   * @returns {Object} Validation result {valid: boolean, message: string}
   */
  validate() {
    if (!this.validator) return { valid: true, message: '' };
    
    return this.validator.validate(this.input.value);
  }
  /**
   * Build the 42‑cell day grid (6 weeks × 7 days).
   * @param {Date}   date               Month/year currently shown
   * @param {boolean}startWeekOnMonday  true ⇒ week starts Monday
   * @returns {string[]} c – length‑42 array with <a>/<span> HTML for each cell
   */
  buildDayCells(date, startWeekOnMonday = false) {
    const year        = date.getFullYear();
    const month       = date.getMonth();
    const daysInMonth = new Date(year, month + 1, 0).getDate();
    const firstDay    = new Date(year, month, 1);

    let weekday = firstDay.getDay();           // 0 = Sun, 1 = Mon …
    if (startWeekOnMonday) weekday = weekday === 0 ? 6 : weekday - 1;

    const prevMonthDays = new Date(year, month, 0).getDate();
    const cells = new Array(42);
    let day = 1, next = 1;

    for (let i = 0; i < 42; ++i) {
      let html;
      if (i < weekday) {
        // tail of prev. month
        const num = prevMonthDays - weekday + i + 1;
        html = `<span class="other-month">${num}</span>`;
      } else if (day <= daysInMonth) {
        // current month
        html = `<a href="#" class="calendar-day" data-y="${year}" data-m="${month}" data-d="${day}">${day}</a>`;
        ++day;
      } else {
        // head of next month
        html = `<span class="other-month">${next}</span>`;
        ++next;
      }
      cells[i] = html;
    }
    return cells;
  }

  /**
   * Generate the calendar HTML.
   * @param  {CalendarOptions} opts
   * @return {string} – HTML string ready for innerHTML
   */
  createCalendar() {
    const year   = this.currentDate.getFullYear();
    const month  = this.currentDate.getMonth();

    const dayNames  = this.dayNames;
    const dayTitles = this.fullDayNames;

    const c = this.buildDayCells(this.currentDate, this.startWeekOnMonday);
    const t = dayTitles; // full names
    const d = dayNames;  // short names

    const tableClass = this.tableClass || 'wt-calendar';

    /*───────────────────────── template literal ───────────────────────*/
    return /*html*/ `
    <table class="days ${tableClass}" cellspacing="0" cellpadding="0">
      <tr>
        <th class="caption"><button type="button" class="nav-prev" aria-label="Previous month">◀</button></th>
        <th class="caption" colspan="5">
          <select class="month-select" aria-label="Choose month">
            ${this.monthNames.map((m,i)=>`<option value="${i}" ${i===month?'selected':''}>${m}</option>`).join('')}
          </select>
          <input class="year-input" type="number" min="1900" max="2100" step="1" value="${year}" aria-label="Year" />
        </th>
        <th class="caption"><button type="button" class="nav-next" aria-label="Next month">▶</button></th>
      </tr>
      <tr>
        ${t.map((title,i)=>`<th title="${title}" scope="col">${d[i]}</th>`).join('')}
      </tr>
      ${Array.from({length:6},(_,w)=>`<tr>${Array.from({length:7},(_,d)=>`<td>${c[w*7+d]}</td>`).join('')}</tr>`).join('')}
    </table>`;
  }
  bindEvents = (container) => {
    container.querySelector('.nav-prev')?.addEventListener('click', () => {
      this.prevMonth();
    });

    container.querySelector('.nav-next')?.addEventListener('click', () => {
      this.nextMonth();
    });

    container.querySelector('.month-select')?.addEventListener('change', e => {
      this.currentDate.setMonth(parseInt(e.target.value, 10));
      this.render(container);
    });

    container.querySelector('.year-input')?.addEventListener('change', e => {
      const year = parseInt(e.target.value, 10);
        this.currentDate.setFullYear(year);
        this.render(container);
    });

    container.querySelectorAll('.calendar-day').forEach(a => {
      a.addEventListener('click', e => {
        e.preventDefault();
        const y = parseInt(a.dataset.y, 10);
        const m = parseInt(a.dataset.m, 10);
        const d = parseInt(a.dataset.d, 10);
        this.selectDate(new Date(y, m, d));
      });
    });
  };
  
  /**
   * Check if a date is today
   * @param {Date} date - Date to check
   * @returns {boolean} True if date is today
   */
  isToday(date) {
    const today = new Date();
    return date.getDate() === today.getDate() && 
           date.getMonth() === today.getMonth() && 
           date.getFullYear() === today.getFullYear();
  }
  
  /**
   * Check if a date is the selected date
   * @param {Date} date - Date to check
   * @returns {boolean} True if date is selected
   */
  isSelectedDate(date) {
    if (!this.selectedDate) return false;
    return date.getDate() === this.selectedDate.getDate() && 
           date.getMonth() === this.selectedDate.getMonth() && 
           date.getFullYear() === this.selectedDate.getFullYear();
  }
  
  /**
   * Select a date and update the input
   * @param {Date} date - Date to select
   */
  selectDate(date) {
    this.selectedDate = date;
    
    // Format the date as YYYY-MM-DD
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const formattedDate = `${year}-${month}-${day}`;
    
    // Update the input value
    this.input.value = formattedDate;
    
    // Validate the selected date
    if (this.validator) {
      const validationResult = this.validate();
      
      if (!validationResult.valid) {
        if (this.onValidationFailed) {
          this.onValidationFailed(validationResult.message, date);
        }
        // Add validation visual feedback
        this.input.classList.add('invalid');
        // Create or update validation message element
        this.#showValidationMessage(validationResult.message);
      } else {
        this.input.classList.remove('invalid');
        this.#hideValidationMessage();
      }
    }
    
    // Hide the popup after selection
    this.#hidePopup();
    
    // Call the date selected callback
    if (this.onDateSelected) {
      this.onDateSelected(date);
    }
  }
  
  /**
   * Move to previous month
   */
  prevMonth() {
    console.log('WDateEdit: prevMonth called');
    this.currentDate.setMonth(this.currentDate.getMonth() - 1);

    this.render(this.popup);

    if (this.onMonthChange) {
      this.onMonthChange(this.currentDate);
    }
  }
  
  /**
   * Move to next month
   */
  nextMonth() {
    console.log('WDateEdit: nextMonth called');
    this.currentDate.setMonth(this.currentDate.getMonth() + 1);

    this.render(this.popup);


    if (this.onMonthChange) {
      this.onMonthChange(this.currentDate);
    }
  }
  
  /**
   * Render the calendar into a container
   * @param {HTMLElement} container - Container element
   * @returns {HTMLElement} The calendar element
   */
  render(container) {
    container.innerHTML = this.createCalendar();
    this.bindEvents(container);
    return container;
  }
  
    /* ───────────────────────── private fields ────────────────────────── */
  
    #hoverZone;           // px
    #downInZone = false;  // pointer started in the icon area?
    #ac;                  // AbortController for all listeners
    #validationMsg;       // Element to display validation messages

  
    get #sig () { return { signal: this.#ac.signal }; }
    get #readOnly () {
      return this.input.readOnly || this.input.hasAttribute('readonly');
    }
  
    /* ───────────────────────── private methods ───────────────────────── */
   /**
   * Set up input validation
   */
    #setupInputValidation() {
      if (!this.validator) return;
      
      // Create validation message element if needed
      if (!this.#validationMsg) {
        this.#validationMsg = document.createElement('div');
        this.#validationMsg.className = 'validation-message';
        this.#validationMsg.style.color = 'red';
        this.#validationMsg.style.fontSize = '0.8em';
        this.#validationMsg.style.marginTop = '4px';
        this.#validationMsg.style.display = 'none';
        
        // Insert after the input
        this.input.parentNode.insertBefore(this.#validationMsg, this.input.nextSibling);
      }
      
      // Add input event listener for validation
      this.input.addEventListener('input', () => {
        const result = this.validate();
        if (!result.valid) {
          this.input.classList.add('invalid');
          this.#showValidationMessage(result.message);
        } else {
          this.input.classList.remove('invalid');
          this.#hideValidationMessage();
        }
      }, this.#sig);
      
      // Add blur event for validation
      this.input.addEventListener('blur', () => {
        const result = this.validate();
        if (!result.valid) {
          this.input.classList.add('invalid');
          this.#showValidationMessage(result.message);
        } else {
          this.input.classList.remove('invalid');
          this.#hideValidationMessage();
        }
      }, this.#sig);
      
      // Initial validation
      const result = this.validate();
      if (!result.valid) {
        this.input.classList.add('invalid');
        this.#showValidationMessage(result.message);
      }
    }

    /**
     * Show validation message
     * @param {string} message - Validation message to display
     */
    #showValidationMessage(message) {
      if (!this.#validationMsg) return;
      
      this.#validationMsg.textContent = message;
      this.#validationMsg.style.display = 'block';
    }

    /**
     * Hide validation message
     */
    #hideValidationMessage() {
      if (!this.#validationMsg) return;
      
      this.#validationMsg.style.display = 'none';
    }

    #bindBaseEvents() {
      // Replace this line:
      // const zoneHit = evt => this.#zoneRect().contains(evt);
      
      // With this implementation:
      const zoneHit = evt => {
        const rect = this.#zoneRect();
        return evt.clientX >= rect.left && evt.clientX <= rect.right &&
              evt.clientY >= rect.top && evt.clientY <= rect.bottom;
      };

      /** hover feedback */
      this.input.addEventListener('pointermove', e => {
        this.input.classList.toggle('hover', !this.#readOnly && zoneHit(e));
      }, this.#sig);

      // Rest of the method remains the same
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
      return new DOMRect(left + width - this.#hoverZone, top, this.#hoverZone, height);
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
  