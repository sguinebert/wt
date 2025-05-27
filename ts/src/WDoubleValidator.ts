// double-validator.ts – TypeScript ES‑module rewrite of legacy WDoubleValidator
// Copyright © 2024 – MIT

/**
 * Validation result structure.
 */
export interface ValidationResult {
  valid: boolean;
  message?: string;
}

/**
 * Options for {@link makeDoubleValidator}.
 */
export interface DoubleValidatorOptions {
  /** Empty string disallowed when true (default: false). */
  mandatory?: boolean;
  /** Trim before parse when true (default: false). */
  ignoreSpaces?: boolean;
  /** Inclusive lower bound (default: null = no bound). */
  min?: number | null;
  /** Inclusive upper bound (default: null = no bound). */
  max?: number | null;
  /** Decimal separator (default: '.') */
  decimalPoint?: string;
  /** Thousands separator to ignore (default: '') */
  groupSeparator?: string;
  /** Custom error for blank value. */
  blankError?: string;
  /** Custom error for NaN. */
  nanError?: string;
  /** Custom error when below min. */
  tooSmallError?: string;
  /** Custom error when above max. */
  tooLargeError?: string;
}

/** Escape a string so it can be used as a literal in a RegExp. */
const escapeRE = (s: string): string => s.replace(/[\\^$.*+?()[\]{}|\-]/g, '\\$&');

/**
 * Create a reusable *double / floating‑point* validator.
 *
 * @example
 * ```ts
 * import { makeDoubleValidator } from './double-validator.js';
 *
 * const validate = makeDoubleValidator({ min: 0, max: 100, decimalPoint: ',', groupSeparator: ' ' });
 * const { valid, message } = validate('42,5');
 * ```
 */
export const makeDoubleValidator = ({
  mandatory = false,
  ignoreSpaces = false,
  min = null,
  max = null,
  decimalPoint = '.',
  groupSeparator = '',
  blankError = 'Cannot be blank',
  nanError = 'Invalid number',
  tooSmallError = 'Value too small',
  tooLargeError = 'Value too large',
}: DoubleValidatorOptions = {}): ((txt: string | number) => ValidationResult) => {
  // Pre‑compute immutable helpers for the returned closure.
  const grpRE = groupSeparator ? new RegExp(escapeRE(groupSeparator), 'g') : null;
  const decSwap = decimalPoint !== '.' ? decimalPoint : null;

  return (input: string | number): ValidationResult => {
    let s = String(input);

    // 1. Optionally trim whitespace
    if (ignoreSpaces) s = s.trim();

    // 2. Empty check
    if (!s.length) {
      return mandatory ? { valid: false, message: blankError } : { valid: true };
    }

    // 3. Remove thousands separators and normalise decimal point
    if (grpRE) s = s.replace(grpRE, '');
    if (decSwap) s = s.replace(decSwap, '.');

    // 4. Reject if still contains spaces when not allowed
    if (!ignoreSpaces && s.includes(' ')) {
      return { valid: false, message: nanError };
    }

    // 5. Numeric conversion and range checks
    const n = Number(s);
    if (Number.isNaN(n)) {
      return { valid: false, message: nanError };
    }
    if (min !== null && n < min) {
      return { valid: false, message: tooSmallError };
    }
    if (max !== null && n > max) {
      return { valid: false, message: tooLargeError };
    }

    return { valid: true };
  };
};
