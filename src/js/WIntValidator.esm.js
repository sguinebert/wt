// Copyright © 2024 – MIT (https://mit-license.org)

/**
 * Fast, zero-dependency integer validator.
 *
 * ```js
 * import WIntValidator from './WIntValidator.esm.js';
 *
 * const v = new WIntValidator({ required: true, min: 1, max: 100 });
 * v.check('42');           // {valid:true}
 * v.check('3 000');        // {valid:false, message:'Too large'}  (NB: non-breaking-space group sep)
 * ```
 */
export default class WIntValidator {
  #required;
  #min;
  #max;
  #sep;
  #msg;

  /**
   * @param {Object}  opts
   * @param {boolean} [opts.required=false]      Empty string invalid if true
   * @param {number}  [opts.min]                 Inclusive lower bound
   * @param {number}  [opts.max]                 Inclusive upper bound
   * @param {string}  [opts.groupSeparator=""]   Thousands separator to ignore
   * @param {Object}  [opts.messages]            Custom error strings
   */
  constructor({
    required = false,
    min,
    max,
    groupSeparator = '',
    messages = {},
  } = {}) {
    this.#required = required;
    this.#min      = Number.isFinite(min) ? min : null;
    this.#max      = Number.isFinite(max) ? max : null;
    this.#sep      = groupSeparator;
    this.#msg      = {
      blank      : 'Value required',
      nan        : 'Not an integer',
      tooSmall   : `Must be ≥ ${min}`,
      tooLarge   : `Must be ≤ ${max}`,
      ...messages,
    };
  }

  /** Validate a value (string \| number). */
  check = value => {
    const txt = String(value ?? '');

    // 1. Empty?
    if (!txt.length) {
      return this.#required
        ? { valid: false, message: this.#msg.blank }
        : { valid: true };
    }

    // 2. Strip thousands separator if provided
    const canonical = this.#sep ? txt.split(this.#sep).join('') : txt;

    // 3. Integer test (fast path before Number() allocation)
    if (!/^-?\d+$/.test(canonical)) {
      return { valid: false, message: this.#msg.nan };
    }

    const n = Number(canonical);                // safe: matches /^\d+$/

    // 4. Range checks
    if (this.#min !== null && n < this.#min) {
      return { valid: false, message: this.#msg.tooSmall };
    }
    if (this.#max !== null && n > this.#max) {
      return { valid: false, message: this.#msg.tooLarge };
    }

    return { valid: true };
  };
}
