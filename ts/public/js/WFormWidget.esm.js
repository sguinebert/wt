// WFormWidget.js
// Drop-in replacement for the legacy “empty-text” helper.

export default class WFormWidget {
  /**
   * @param {HTMLInputElement|HTMLTextAreaElement} el  field to decorate
   * @param {string}  emptyText      – text shown when the field is empty & unfocused
   * @param {string}  cssClass="Wt-edit-emptyText" – class added while placeholder is visible
   */
  constructor(el, emptyText, cssClass = 'Wt-edit-emptyText') {
    this.el       = el;
    this.emptyTxt = emptyText;
    this.cssClass = cssClass;

    // Native placeholder does the job in every evergreen browser;
    // we keep the CSS class for styling parity with old code.
    el.placeholder = emptyText;

    el.addEventListener('focus',     () => this.#apply());
    el.addEventListener('blur',      () => this.#apply());
    el.addEventListener('input',     () => this.#toggleClass());

    /** Initial state */
    this.#apply();
  }

  /** Programmatically change the placeholder text. */
  setEmptyText(text) {
    this.emptyTxt          = text;
    this.el.placeholder    = text;
    this.#apply();
  }

  /* ---------- private helpers ---------- */
  #apply() {
    // Nothing fancy anymore – leave `type="password"` intact: modern browsers
    // support placeholder for password fields.
    this.#toggleClass();
  }

  #toggleClass() {
    this.el.classList.toggle(
      this.cssClass,
      !this.el.matches(':focus') && this.el.value === ''
    );
  }
}
