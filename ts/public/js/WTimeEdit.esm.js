// WTimeEdit.js  –  ES module
export default class WTimeEdit {
  static BTN_W = 40;                 // width (px) of the ▶ icon zone
  #input; #popup; #popupApi;

  /**
   * @param {HTMLInputElement} input     – the `<input type="time">`
   * @param {HTMLElement}      popupUl   – the <ul> / container that acts as the
   *                                       dropdown. Must have .wtPopup API.
   * @param {Object}           config    - Configuration options
   * @param {string}           [config.min] - Minimum time (HH:MM format)
   * @param {string}           [config.max] - Maximum time (HH:MM format)
   * @param {boolean}          [config.required] - Whether input is required
   * @param {string}           [config.step] - Time step in seconds (e.g. '60' for minutes)
   * @param {string}           [config.value] - Default time value
   * @param {string}           [config.pattern] - Regex pattern for validation
   * @param {string}           [config.autocomplete] - Autocomplete setting
   * @param {string}           [config.label] - Associated label text
   * @param {string}           [config.helpText] - Help text to display
   */
  constructor(input, popupUl, config = {}){
    this.#input = typeof input === 'string' ? document.getElementById(input) : input;
    if(!(this.#input instanceof HTMLInputElement))
      throw new TypeError('WTimeEdit: first arg must be <input>');
    
    this.#popup = popupUl;
    this.#popupApi = popupUl.wtPopup;               // already created elsewhere

    // Ensure input has type="time"
    input.type = 'time';

    // Apply HTML5 time attributes from config
    this.#applyTimeAttributes(config);

    // Apply label if provided
    if (config.label) {
      this.#createLabel(config.label);
    }

    // Apply help text if provided
    if (config.helpText) {
      this.#createHelpText(config.helpText);
    }

    // Populate the dropdown based on min/max/step
    this.#populateTimeDropdown();
    
    // Pointer modelling for all devices
    input.addEventListener('pointermove',  this.#onMove);
    input.addEventListener('pointerleave', this.#onLeave);
    input.addEventListener('pointerdown',  this.#onDown);
    input.addEventListener('pointerup',    this.#onUp);
  }

  /**
   * Apply HTML5 time attributes from config
   * @param {Object} config - Configuration options
   */
  #applyTimeAttributes(config) {
    const input = this.#input;
    
    // Apply each attribute if provided in config
    if (config.min) input.min = config.min;
    if (config.max) input.max = config.max;
    if (config.required !== undefined) input.required = Boolean(config.required);
    if (config.step) input.step = config.step;
    if (config.value) input.value = config.value;
    if (config.pattern) input.pattern = config.pattern;
    if (config.autocomplete) input.autocomplete = config.autocomplete;
    
    // Apply additional ARIA attributes for accessibility
    input.setAttribute('aria-label', config.label || 'Time picker');
  }

  /**
   * Create and attach a label element
   * @param {string} labelText - The label text
   */
  #createLabel(labelText) {
    // Ensure input has an ID for label association
    if (!this.#input.id) {
      this.#input.id = `time_${Math.random().toString(36).substring(2, 9)}`;
    }
    
    const label = document.createElement('label');
    label.setAttribute('for', this.#input.id);
    label.textContent = labelText;
    
    // Insert label before input
    this.#input.parentNode.insertBefore(label, this.#input);
  }

  /**
   * Create and attach help text
   * @param {string} helpText - The help text
   */
  #createHelpText(helpText) {
    const small = document.createElement('small');
    small.textContent = helpText;
    small.style.display = 'block';
    small.style.marginTop = '4px';
    small.style.color = '#666';
    
    // Insert after input
    const nextSibling = this.#input.nextSibling;
    if (nextSibling) {
      this.#input.parentNode.insertBefore(small, nextSibling);
    } else {
      this.#input.parentNode.appendChild(small);
    }
  }

  /**
   * Populate the time dropdown with appropriate options
   * based on min, max, and step settings
   */
  #populateTimeDropdown() {
    // Clear existing options
    this.#popup.innerHTML = '';
    
    // Get min and max time values
    const minTime = this.#parseTime(this.#input.min || '00:00');
    const maxTime = this.#parseTime(this.#input.max || '23:59');
    
    // Determine step in minutes (default: 30 minutes)
    const stepSeconds = parseInt(this.#input.step || '1800', 10);
    const stepMinutes = Math.max(1, Math.floor(stepSeconds / 60));
    
    // Calculate total minutes for min and max
    const minMinutes = minTime.hours * 60 + minTime.minutes;
    const maxMinutes = maxTime.hours * 60 + maxTime.minutes;
    
    // Generate time options
    for (let mins = minMinutes; mins <= maxMinutes; mins += stepMinutes) {
      const hours = Math.floor(mins / 60);
      const minutes = mins % 60;
      
      // Format time as HH:MM
      const timeValue = `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}`;
      
      // Create list item
      const li = document.createElement('li');
      li.textContent = timeValue;
      li.dataset.value = timeValue;
      
      // Add click handler
      li.addEventListener('click', () => {
        this.#input.value = timeValue;
        this.#popupApi.hide();
        
        // Dispatch change event
        const event = new Event('change', { bubbles: true });
        this.#input.dispatchEvent(event);
      });
      
      this.#popup.appendChild(li);
    }
  }

  /**
   * Parse time string into hours and minutes
   * @param {string} timeStr - Time in HH:MM format
   * @returns {Object} Object with hours and minutes
   */
  #parseTime(timeStr) {
    const [hours, minutes] = timeStr.split(':').map(Number);
    return { 
      hours: isNaN(hours) ? 0 : hours, 
      minutes: isNaN(minutes) ? 0 : minutes 
    };
  }

  /* ---------- pointer handlers ------------------------------------- */
  #onMove = e => {
    if(this.#input.readOnly) return;
    const hover = e.offsetX > this.#input.clientWidth - WTimeEdit.BTN_W;
    this.#input.classList.toggle('hover', hover);
  };
  
  #onLeave = () => this.#input.classList.remove('hover');

  #onDown = e => {
    if(this.#input.readOnly) return;
    if(e.offsetX > this.#input.clientWidth - WTimeEdit.BTN_W){
      this.#input.classList.add('active','unselectable');
    }
  };
  
  #onUp = e => {
    this.#input.classList.remove('unselectable');
    if(e.offsetX > this.#input.clientWidth - WTimeEdit.BTN_W){
      this.#showPopup();
    }else{
      this.#input.classList.remove('active');
    }
  };

  /* ---------- helpers ---------------------------------------------- */
  #showPopup() {
    this.#input.classList.add('active');

    // ensure we reset the button when popup closes
    this.#popupApi.onHide = () => this.#input.classList.remove('active');

    // position & show
    this.#popupApi.show(this.#input,'vertical');   // your helper adapts
  }
}
