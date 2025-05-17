// double-validator.js  – ES-module rewrite of legacy **WDoubleValidator**
// Usage:
//   import { makeDoubleValidator } from './double-validator.js';
//   const validate = makeDoubleValidator({ min:0, max:100 });
//   const {valid,message} = validate('42,5');

const escapeRE = s => s.replace(/[\\^$.*+?()[\]{}|\-]/g, '\\$&');

/**
 * Create a reusable *double / floating-point* validator.
 *
 * @param {Object}  opt
 * @param {boolean} [opt.mandatory=false]           – empty string disallowed
 * @param {boolean} [opt.ignoreSpaces=false]        – trim before parse
 * @param {number|null} [opt.min=null]              – inclusive lower bound
 * @param {number|null} [opt.max=null]              – inclusive upper bound
 * @param {string}  [opt.decimalPoint='.']          – decimal separator
 * @param {string}  [opt.groupSeparator='']         – thousands separator
 * @param {string}  [opt.blankError='Required']     – msg for empty
 * @param {string}  [opt.nanError='Not a number']   – msg for NaN
 * @param {string}  [opt.tooSmallError='Too small'] – msg for < min
 * @param {string}  [opt.tooLargeError='Too large'] – msg for > max
 *
 * @returns {(txt:string) => {valid:boolean,message?:string}}
 */
export const makeDoubleValidator = ({
  mandatory        = false,
  ignoreSpaces     = false,
  min              = null,
  max              = null,
  decimalPoint     = '.',
  groupSeparator   = '',
  blankError       = 'Cannot be blank',
  nanError         = 'Invalid number',
  tooSmallError    = 'Value too small',
  tooLargeError    = 'Value too large',
} = {}) => {
  const grpRE   = groupSeparator ? new RegExp(escapeRE(groupSeparator), 'g') : null;
  const decSwap = decimalPoint !== '.' && decimalPoint;

  return txt => {
    let s = String(txt);

    if (ignoreSpaces) s = s.trim();
    if (!s.length)
      return mandatory ? { valid:false, message:blankError } : { valid:true };

    if (grpRE)   s = s.replace(grpRE, '');
    if (decSwap) s = s.replace(decSwap, '.');
    if (!ignoreSpaces && s.includes(' '))
      return { valid:false, message:nanError };

    const n = Number(s);
    if (Number.isNaN(n))          return { valid:false, message:nanError };
    if (min !== null && n < min)  return { valid:false, message:tooSmallError };
    if (max !== null && n > max)  return { valid:false, message:tooLargeError };

    return { valid:true };
  };
};
