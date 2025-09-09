
export default class WPushButton {
  /**
   * Create a new push button
   * @param {Object} options - Button configuration options
   * @param {string} [options.id] - Button ID
   * @param {string} [options.text='Button'] - Button text
   * @param {string} [options.className=''] - Additional CSS classes
   * @param {boolean} [options.disabled=false] - Initial disabled state
   * @param {string} [options.type='button'] - Button type (button, submit, reset)
   * @param {Function} [options.onClick] - Click event handler
   */
  constructor(options = {}) {
    this.id = options.id || `btn_${Math.random().toString(36).substring(2, 9)}`;
    this.text = options.text || 'Button';
    this.className = options.className || '';
    this.disabled = options.disabled || false;
    this.type = options.type || 'button';
    this.onClick = options.onClick || null;
    
    // Create button element
    this.element = this.#createButtonElement();
  }
  
  /**
   * Create the button DOM element
   * @returns {HTMLButtonElement} The button element
   * @private
   */
  #createButtonElement() {
    const button = document.createElement('button');
    button.id = this.id;
    button.type = this.type;
    button.textContent = this.text;
    button.disabled = this.disabled;
    
    if (this.className) {
      button.className = this.className;
    }
    
    if (this.onClick) {
      button.addEventListener('click', this.onClick);
    }
    
    return button;
  }
  
  /**
   * Set button text
   * @param {string} text - New button text
   * @returns {WPushButton} This instance for chaining
   */
  setText(text) {
    this.text = text;
    this.element.textContent = text;
    return this;
  }
  
  /**
   * Enable or disable the button
   * @param {boolean} isDisabled - Whether to disable the button
   * @returns {WPushButton} This instance for chaining
   */
  setDisabled(isDisabled) {
    this.disabled = isDisabled;
    this.element.disabled = isDisabled;
    return this;
  }
  
  /**
   * Add a click event listener
   * @param {Function} handler - Click event handler
   * @returns {WPushButton} This instance for chaining
   */
  onClick(handler) {
    this.onClick = handler;
    this.element.addEventListener('click', handler);
    return this;
  }
  
  /**
   * Append button to a container element
   * @param {HTMLElement} container - Container to append to
   * @returns {WPushButton} This instance for chaining
   */
  appendTo(container) {
    container.appendChild(this.element);
    return this;
  }
  
  /**
   * Remove button from DOM
   * @returns {WPushButton} This instance for chaining
   */
  remove() {
    if (this.element.parentNode) {
      this.element.parentNode.removeChild(this.element);
    }
    return this;
  }
  
  /**
   * Convert button state to JSON
   * @returns {Object} JSON representation of button state
   */
  toJSON() {
    return {
      id: this.id,
      text: this.text,
      className: this.className,
      disabled: this.disabled,
      type: this.type
    };
  }
  
  /**
   * Create a button from JSON data
   * @param {Object} json - JSON data
   * @returns {WPushButton} New button instance
   */
  static fromJSON(json) {
    return new WPushButton(json);
  }
}
