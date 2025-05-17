/* ------------------------------------------------------------------
   WDateValidator.es2022.js   ⚡️ modern replacement (© Emweb 2011-2025)
   ------------------------------------------------------------------ */

   export default class WDateValidator {
    /**
     * @param {boolean}         mandatory     – empty input is an error?
     * @param {Array<Object>}   formats       – [{regexp, getDay,getMonth,getYear}]
     * @param {?Date}           bottom        – min allowed date   (or null)
     * @param {?Date}           top           – max allowed date   (or null)
     * @param {string}          blankError    – “field empty”      message
     * @param {string}          formatError   – “bad format”       message
     * @param {string}          tooSmallError – “< bottom”         message
     * @param {string}          tooLargeError – “> top”            message
     */
    constructor (
      mandatory,
      formats,
      bottom,
      top,
      blankError,
      formatError,
      tooSmallError,
      tooLargeError
    ) {
      // -----------------------------------------------------------------
      // ☛ store config (frozen so callers can’t mutate behind our back)
      // -----------------------------------------------------------------
      this.cfg = Object.freeze({
        mandatory,
        formats,
        bottom,
        top,
        msgs: { blankError, formatError, tooSmallError, tooLargeError }
      });
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
  