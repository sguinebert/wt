// WDateValidator.ts – Modern date validator (TypeScript edition)
// © GUINEBERT 2025 – MIT

/** Validation result returned by {@link WDateValidator.validate}. */
export interface ValidationResult {
  valid: boolean;
  message?: string;
}

/** Callback suite used to extract day, month and year parts from a regex match. */
export interface DateFormat {
  /** **Without** leading/trailing anchors – they are added automatically. */
  regexp: string;
  getDay(match: RegExpMatchArray): number;
  getMonth(match: RegExpMatchArray): number;
  getYear(match: RegExpMatchArray): number;
}

/** Error‑message map. */
export interface MessageMap {
  blankError: string;
  formatError: string;
  tooSmallError: string;
  tooLargeError: string;
}

/** Internal immutable configuration shape. */
interface Config {
  mandatory: boolean;
  formats: DateFormat[];
  bottom: Date | null;
  top: Date | null;
  msgs: MessageMap;
}

export default class WDateValidator {
  // Using `private readonly` avoids accidental mutation; additionally, `Object.freeze()`
  // guarantees deep immutability at runtime (in non‑strict mode code paths too).
  private readonly cfg: Readonly<Config>;

  /**
   * @param mandatory     Empty input triggers an error when **true**.
   * @param formats       Array of parsing formats; the first that matches wins.
   * @param bottom        Inclusive minimum date (or **null** for none).
   * @param top           Inclusive maximum date (or **null** for none).
   * @param blankError    Message when value is blank but mandatory.
   * @param formatError   Message when the input doesn't match any format.
   * @param tooSmallError Message when the date is < *bottom*.
   * @param tooLargeError Message when the date is > *top*.
   */
  constructor (
    mandatory: boolean,
    formats: DateFormat[],
    bottom: Date | null,
    top: Date | null,
    blankError: string,
    formatError: string,
    tooSmallError: string,
    tooLargeError: string,
  ) {
    this.cfg = Object.freeze({
      mandatory,
      formats: formats.slice(), // shallow‑copy to prevent external mutations
      bottom,
      top,
      msgs: { blankError, formatError, tooSmallError, tooLargeError },
    });
  }

  /* ------------------------------------------------------------------
     validate() – synchronously validate a textual date representation.  
  ------------------------------------------------------------------ */
  validate(text: string): ValidationResult {
    const { mandatory, formats, bottom, top, msgs } = this.cfg;

    // 1. Blank input ---------------------------------------------------
    if (text.trim().length === 0) {
      return mandatory ? { valid: false, message: msgs.blankError }
                       : { valid: true };
    }

    // 2. Attempt listed formats ---------------------------------------
    let d = -1, m = -1, y = -1;
    for (const f of formats) {
      const match = text.match(new RegExp(`^${f.regexp}$`));
      if (!match) continue; // try next format

      // Extract parts (caller‑provided helpers may throw – propagate as is)
      m = f.getMonth(match);
      d = f.getDay(match);
      y = f.getYear(match);
      break;
    }

    // No format matched
    if (d < 0) return { valid: false, message: msgs.formatError };

    // 3. Range & calendar sanity checks -------------------------------
    if (d < 1 || d > 31 || m < 1 || m > 12) {
      return { valid: false, message: msgs.formatError };
    }

    const dt = new Date(y, m - 1, d); // JS months are 0‑based

    // Invalid day/month (e.g., 31 Feb) or pre‑1400 guard
    if (
      dt.getDate()   !== d ||
      dt.getMonth()  !== m - 1 ||
      dt.getFullYear() !== y ||
      y < 1400
    ) {
      return { valid: false, message: msgs.formatError };
    }

    // Minimum / maximum date boundaries
    if (bottom && dt < bottom) {
      return { valid: false, message: msgs.tooSmallError };
    }
    if (top && dt > top) {
      return { valid: false, message: msgs.tooLargeError };
    }

    // 4. All good ------------------------------------------------------
    return { valid: true };
  }
}
