/**
 * @param {Object} cfg
 * @param {boolean} [cfg.mandatory=false]  – empty value not allowed
 * @param {number}  [cfg.min]              – inclusive minimum
 * @param {number}  [cfg.max]              – inclusive maximum
 * @param {string}  [cfg.blankMsg]         – message when blank but required
 * @param {string}  [cfg.tooShortMsg]      – message when shorter than `min`
 * @param {string}  [cfg.tooLongMsg]       – message when longer than `max`
 */
export function lengthValidator({
  mandatory   = false,
  min         = null,
  max         = null,
  blankMsg    = 'This field is required',
  tooShortMsg = length => `Must be ≥ ${length} characters`,
  tooLongMsg  = length => `Must be ≤ ${length} characters`,
} = {}) {

  /**  @param {string} value */
  return function validate(value = '') {
    if (!value.length) {
      return mandatory ? { valid:false, message:blankMsg } : ok();
    }

    if (min != null && value.length < min)
      return { valid:false, message:
               typeof tooShortMsg === 'function' ? tooShortMsg(min) : tooShortMsg };

    if (max != null && value.length > max)
      return { valid:false, message:
               typeof tooLongMsg === 'function' ? tooLongMsg(max) : tooLongMsg };

    return ok();
  };

  function ok() { return { valid:true }; }
}
