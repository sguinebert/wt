/**
 * Simple value-validator based on a regular expression.
 *
 * ```js
 * import WRegExpValidator from './WRegExpValidator.esm.js';
 *
 * const v = new WRegExpValidator(
 *   true,               // mandatory
 *   '[A-Z]{2}-\\d{4}',  // pattern (string or RegExp)
 *   'i',                // flags (ignored when pattern is RegExp)
 *   'Value required',   // blank-error message
 *   'Format invalid'    // regexp-error message
 * );
 *
 * v.validate('AB-1234');  // → { valid:true }
 * v.validate('');         // → { valid:false, message:'Value required' }
 * ```
 */
export default class WRegExpValidator {
  /**
   * @param {boolean}           mandatory
   * @param {string|RegExp}     pattern      String pattern *or* RegExp instance
   * @param {string}            flags        Ignored when `pattern` is RegExp
   * @param {string}            blankError   Shown when field is empty & mandatory
   * @param {string}            invalidError Shown when regexp does not match
   */
  constructor(
    mandatory     = false,
    pattern       = null,
    flags         = '',
    blankError    = 'Required',
    invalidError  = 'Invalid'
  ) {
    this.mandatory    = Boolean(mandatory);
    this.regex        = pattern ? (pattern instanceof RegExp ? pattern
                         : new RegExp(pattern, flags)) : null;
    this.blankError   = blankError;
    this.invalidError = invalidError;
  }

  /**
   * Validate a value.
   * @param  {string} value
   * @return {{valid:boolean, message?:string}}
   */
  validate(value = '') {
    if (value.length === 0) {
      return this.mandatory
        ? { valid: false, message: this.blankError }
        : { valid: true };
    }

    if (!this.regex) return { valid: true };

    // Match must cover the complete string, just like in the legacy code.
    const m = this.regex.exec(value);
    return m && m[0].length === value.length
      ? { valid: true }
      : { valid: false, message: this.invalidError };
  }
}
