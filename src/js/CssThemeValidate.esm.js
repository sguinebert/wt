/* ------------------------------------------------------------------
 * Modern replacement for Wt's `validate` + `setValidationState`
 * Requires: native ES2022+ environment (no legacy fall‑backs)
 * ------------------------------------------------------------------ */

/**
 * Apply or clear validation styling + tooltip on a single form control.
 *
 * @param {HTMLElement} el              – the edited element (input, select …)
 * @param {{valid:boolean,message?:string}} state – validation outcome
 * @param {{showValid?:boolean,showInvalid?:boolean}} [opts] – style options
 */
export function setValidationState(el, state, opts = {}) {
  const { valid, message = "" } = state;
  const { showValid = true, showInvalid = true } = opts;

  el.classList.toggle("Wt-valid",   valid   && showValid);
  el.classList.toggle("Wt-invalid", !valid  && showInvalid);

  // Persist original title once
  if (!el.dataset.defaultTitle) {
    el.dataset.defaultTitle = el.getAttribute("title") ?? "";
  }

  el.setAttribute("title", valid ? el.dataset.defaultTitle : message);
}

/**
 * Unified validation entry‑point for any Wt input element.
 *
 * The element must expose a `wtValidate.validate(value)` function which returns
 * `{ valid: boolean, message?: string }`.
 *
 * @param {HTMLElement & { wtValidate: {validate:(value:string)=>{valid:boolean,message?:string}}, wtLObj?:{getValue:()=>string} }} el
 * @returns {boolean} – `true` when valid
 */
export function validate(el) {
  // 1️⃣ extract a *display* value for the widget
  let value;
  if (el instanceof HTMLSelectElement) {
    value = el.selectedOptions[0]?.text ?? "";
  } else if (typeof el.wtLObj?.getValue === "function") {
    value = el.wtLObj.getValue();
  } else {
    value = el.value ?? "";
  }

  // 2️⃣ run domain/length validation provided by the server side
  const result = el.wtValidate.validate(String(value));

  // 3️⃣ reflect outcome in UI (both valid + invalid styles)
  setValidationState(el, result, { showValid: true, showInvalid: true });

  return result.valid;
}
