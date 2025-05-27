// WIntValidator.ts – Fast, zero‑dependency integer validator (TypeScript)
// Copyright © 2024 – MIT

export interface ValidationResult {
  valid: boolean;
  message?: string;
}

export interface WIntValidatorOptions {
  /** Empty string invalid if true */
  required?: boolean;
  /** Inclusive lower bound */
  min?: number;
  /** Inclusive upper bound */
  max?: number;
  /** Thousands separator to ignore (e.g. "\u00A0" for non‑breaking space) */
  groupSeparator?: string;
  /** Override error strings */
  messages?: Partial<{
    blank: string;
    nan: string;
    tooSmall: string;
    tooLarge: string;
  }>;
}

export default class WIntValidator {
  #required: boolean;
  #min: number | null;
  #max: number | null;
  #sep: string;
  #msg: Record<string, string>;

  constructor({
    required = false,
    min,
    max,
    groupSeparator = '',
    messages = {},
  }: WIntValidatorOptions = {}) {
    const minVal = typeof min === 'number' && Number.isFinite(min) ? min : null;
    const maxVal = typeof max === 'number' && Number.isFinite(max) ? max : null;

    this.#required = required;
    this.#min = minVal;
    this.#max = maxVal;
    this.#sep = groupSeparator;

    this.#msg = {
      blank: 'Value required',
      nan: 'Not an integer',
      tooSmall: `Must be ≥ ${minVal}`,
      tooLarge: `Must be ≤ ${maxVal}`,
      ...messages,
    } as Record<string, string>;
  }

  /** Validate a value (string | number). */
  check = (value: string | number): ValidationResult => {
    const txt = String(value ?? '');

    // 1. Empty?
    if (!txt.length) {
      return this.#required
        ? { valid: false, message: this.#msg.blank }
        : { valid: true };
    }

    // 2. Strip thousands separator if provided
    const canonical = this.#sep ? txt.split(this.#sep).join('') : txt;

    // 3. Integer format test (fast path before Number allocation)
    if (!/^-?\d+$/.test(canonical)) {
      return { valid: false, message: this.#msg.nan };
    }

    const n = Number(canonical); // safe because canonical matches /^\d+$/

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
