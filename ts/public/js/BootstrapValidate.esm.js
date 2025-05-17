// validation.js – modern ES 2022 replacement for the legacy Wt validate helpers
// -----------------------------------------------------------------------------
//  API
//  ---
//  import { validateField } from "./validation.js";
//  validateField(editDomElement);                           // runs validation & updates DOM
//
//  If you need fine‑grained control use the lower‑level helpers that are exported
//  as well:
//      getFieldValue(edit)      • extracts the user‑visible value
//      applyValidationState(...) • sets CSS classes + messages
// -----------------------------------------------------------------------------

export const defaultTheme = {
  classes : {
    valid  : "is-valid",
    invalid: "is-invalid",
  },
  type    : "bootstrap",
  version : 5,
};

/**
 * Return the *display* value for any kind of form control we support.
 */
export function getFieldValue(edit) {
  // <select>
  if (edit.options) {
    return edit.options[edit.selectedIndex]?.text ?? "";
  }
  // Custom LOB – exposes `getValue()` like Wt's rich editors
  if (edit.wtLObj?.getValue instanceof Function) {
    return edit.wtLObj.getValue();
  }
  // <input>, <textarea>, …
  return edit.value ?? "";
}

/**
 * Run the widget‑specific validator (assumed to be attached at `edit.wtValidate`)
 * and reflect the result in the DOM.
 *
 * @param {HTMLElement} edit – form control
 * @param {object}      opts – {theme?, styles?}
 *   styles: bitmask 0x1 = show invalid, 0x2 = show valid  (default 0x1)
 * @returns {boolean} whether the field is valid
 */
export function validateField(edit, opts = {}) {
  const value = getFieldValue(edit);
  const { valid, message } = edit.wtValidate.validate(value);
  applyValidationState(edit, valid, message, opts);
  return valid;
}

/** Bit‑flags that decide which CSS classes are toggled */
export const StyleMask = Object.freeze({
  INVALID: 0x1,
  VALID  : 0x2,
});

/**
 * Toggle CSS classes + inline title and (Bootstrap) group classes.
 */
export function applyValidationState(
  edit,
  isValid,
  message = "",
  { theme = defaultTheme, styles = StyleMask.INVALID } = {},
) {
  // ---------------------------------------------------------------------------
  // 1. Field classes
  // ---------------------------------------------------------------------------
  const showValid   =  isValid && (styles & StyleMask.VALID);
  const showInvalid = !isValid && (styles & StyleMask.INVALID);

  const validCls   = theme.classes?.valid   ?? defaultTheme.classes.valid;
  const invalidCls = theme.classes?.invalid ?? defaultTheme.classes.invalid;

  edit.classList.toggle(validCls  , showValid);
  edit.classList.toggle(invalidCls, showInvalid);

  // ---------------------------------------------------------------------------
  // 2. Bootstrap 2/3 support – highlight surrounding .control‑group / .form‑group
  // ---------------------------------------------------------------------------
  const group = edit.closest(".control-group, .form-group");
  if (group) {
    const isBS2 = group.classList.contains("control-group");
    const successCls = isBS2 ? "success"     : "has-success";
    const errorCls   = isBS2 ? "error"       : "has-error";

    group.classList.toggle(successCls, showValid);
    group.classList.toggle(errorCls  , showInvalid);

    group.querySelectorAll(".Wt-validation-message")
         .forEach(node => node.textContent = isValid ? (edit.defaultTT ?? "") : message);
  }

  // ---------------------------------------------------------------------------
  // 3. Fallback tooltip (title attribute)
  // ---------------------------------------------------------------------------
  if (!("defaultTT" in edit)) {
    edit.defaultTT = edit.getAttribute("title") ?? "";
  }
  edit.setAttribute("title", isValid ? edit.defaultTT : message);
}
