/* ------------------------------------------------------------------
   WDateValidator.es2022.js   ⚡️ modern replacement (© Emweb 2011-2025)
   ------------------------------------------------------------------ */

   export default class WDateValidator {
  /**
   * @param {Object|boolean} config - Configuration object or mandatory flag
   * @param {boolean} [config.mandatory] - Whether the field is required
   * @param {Array<Object>} [config.formats] - Array of format objects
   * @param {Date} [config.min] - Minimum allowed date
   * @param {Date} [config.max] - Maximum allowed date
   * @param {Object} [config.messages] - Error messages
   * @param {string} [config.messages.blank] - Empty field error
   * @param {string} [config.messages.format] - Invalid format error
   * @param {string} [config.messages.tooSmall] - Date too early error
   * @param {string} [config.messages.tooLarge] - Date too late error
   */
      constructor(config, formats, bottom, top, blankError, formatError, tooSmallError, tooLargeError) {
 {
        // Check if first argument is a configuration object
        if (typeof config === 'object') {
          // Object-style initialization
          this.cfg = Object.freeze({
            mandatory: config.mandatory || false,
            formats: config.formats || [],
            bottom: config.min || null,
            top: config.max || null,
            msgs: {
              blankError: config.messages?.blank || 'This field is required',
              formatError: config.messages?.format || 'Invalid date format',
              tooSmallError: config.messages?.tooSmall || 'Date is too early',
              tooLargeError: config.messages?.tooLarge || 'Date is too late'
            }
          });
        } else {
          // Individual parameters initialization (legacy style)
          this.cfg = Object.freeze({
            mandatory: config, // First param is mandatory flag
            formats,
            bottom,
            top,
            msgs: { blankError, formatError, tooSmallError, tooLargeError }
          });
        }
      }
    }
  
    /* -------------------------------------------------------------------
       validate() → { valid : boolean, message? : string }
       ------------------------------------------------------------------ */
    validate (text) {
      const { mandatory, formats, bottom, top, msgs } = this.cfg;
  
      // ── 1. blank input ────────────────────────────────────────────────
      if (text.trim().length === 0) {
        return mandatory
          ? { valid: false, message: msgs.blankError }
          : { valid: true };
      }
  
      // ── 2. attempt the listed formats ────────────────────────────────
      let d = -1, m = -1, y = -1;
      for (const f of formats) {
        const match = text.match(new RegExp(`^${f.regexp}$`));
        if (!match) continue;                 // next format …
  
        m = f.getMonth(match);
        d = f.getDay(match);
        y = f.getYear(match);
        break;
      }
  
      // none matched
      if (d < 0) return { valid: false, message: msgs.formatError };
  
      // ── 3. range + calendar sanity checks ────────────────────────────
      if (d < 1 || d > 31 || m < 1 || m > 12) {
        return { valid: false, message: msgs.formatError };
      }
  
      const dt = new Date(y, m - 1, d);               // JS months are 0-based
  
      // invalid day/month (e.g. 31 Feb) or pre-1400 …
      if (
        dt.getDate()   !== d ||
        dt.getMonth()  !== m - 1 ||
        dt.getFullYear() !== y ||
        y < 1400
      ) {
        return { valid: false, message: msgs.formatError };
      }
  
      // too early?
      if (bottom && dt < bottom) {
        return { valid: false, message: msgs.tooSmallError };
      }
      // too late?
      if (top && dt > top) {
        return { valid: false, message: msgs.tooLargeError };
      }
  
      // ── 4. all good ──────────────────────────────────────────────────
      return { valid: true };
    }
  }
  