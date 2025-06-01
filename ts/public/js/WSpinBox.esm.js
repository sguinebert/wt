/* -------------------------------------------------------------------------
 * WSpinBox – modern rewrite (ES 2022+)
 * -------------------------------------------------------------------------
 *  import WSpinBox from "./WSpinBox.js";
 *  const spin = new WSpinBox({
 *      input: document.querySelector("input[type=number].my-spin"),
 *      precision : 2,
 *      prefix    : "$ ",             // optional text before the number
 *      suffix    : " kg",            // optional text after the number
 *      min       : -10,
 *      max       :  10,
 *      step      :  0.25,
 *      wrap      : false              // true → wrap‑around instead of clamping
 *  });
 * -------------------------------------------------------------------------
 * Features
 *   • Pointer‑based drag in the last 16×16 px (bootstrap‑style) arrow zone.
 *   • Click arrows or wheel / ArrowUp ↓ keys for single‑step changes (repeat held).
 *   • Locale‑aware formatting via Intl.NumberFormat (decimal + grouping).
 *   • Optional prefix / suffix in the display string.
 *   • Emits `onChange(value, finished)` when the user finishes editing.
 * -------------------------------------------------------------------------*/

 export default class WSpinBox {
  /**
   * @param {Object} opts
   * @param {HTMLInputElement} opts.input  – target <input>
   * @param {number}  [opts.precision=0]
   * @param {string}  [opts.prefix=""]
   * @param {string}  [opts.suffix=""]
   * @param {number}  [opts.min=-Infinity]
   * @param {number}  [opts.max=Infinity]
   * @param {number}  [opts.step=1]
   * @param {boolean} [opts.wrap=false]    – wrap‑around instead of clamp
   * @param {string}  [opts.locale]        – defaults to navigator.language
   */
  constructor({
    input,
    precision = 0,
    prefix = "",
    suffix = "",
    min = -Infinity,
    max = Infinity,
    step = 1,
    wrap = false,
    locale = navigator.language
  }) {
    if (!input) throw new Error("input element required");
    /** @private */
    this.#el       = input;

    this.#precision = precision;
    this.#prefix    = prefix;
    this.#suffix    = suffix;
    this.#min       = min;
    this.#max       = max;
    this.#step      = step;
    this.#wrap      = wrap;
    this.#fmt       = new Intl.NumberFormat(locale, {
      minimumFractionDigits: precision,
      maximumFractionDigits: precision
    });

    // Change input type to text if we have prefix/suffix or non-standard formatting
    if (prefix || suffix) {
      input.type = "text";
      console.warn('SpinBox with prefix/suffix should use type="text" instead of type="number"');
    }

    // behaviour flags
    this.#arrowZone = 16;              // px from right edge that acts as arrow area

    // state
    this.#dragStart  = null;           // {y, value} when dragging
    this.#repeatT    = null;           // for key / mousedown repeat

    // Initial display sync
    this.#syncDisplay(this.#clamp(Number(input.value || 0)));

    // ─────────────────── event hooks ───────────────────
    input.addEventListener("wheel",   e => this.#onWheel(e));
    input.addEventListener("keydown", e => this.#onKeyDown(e));
    input.addEventListener("keyup",   () => this.#stopRepeat());

    input.addEventListener("pointerdown", e => this.#onPointerDown(e));
  }

  /*──────────────────────── Public API ────────────────────────*/
  onChange(/* value:number, finished:boolean */) {}

  setValue(v) {
    this.#syncDisplay(this.#clamp(v));
  }
  getValue() {
    return this.#parse(this.#el.value);
  }

  /*─────────────────────── internals ─────────────────────────*/
  #precision;
  #prefix;
  #suffix;
  #min;
  #max;
  #step;
  #wrap;
  #fmt;
  #arrowZone;
  #el;

  #dragStart; // {y,value}
  #repeatT;

  #parse(str) {
    const raw = str.replace(this.#prefix, "").replace(this.#suffix, "");
    
    // Get decimal and group separators for the current locale
    const parts = new Intl.NumberFormat(navigator.language).formatToParts(1234.5);
    const decimalSeparator = parts.find(part => part.type === 'decimal')?.value || '.';
    const groupSeparator = parts.find(part => part.type === 'group')?.value || ',';
    
    // First, remove group separators
    let normalized = raw.replace(new RegExp('\\' + groupSeparator, 'g'), '');
    
    // Then, replace decimal separator with standard period for JS parsing
    if (decimalSeparator !== '.') {
      normalized = normalized.replace(new RegExp('\\' + decimalSeparator, 'g'), '.');
    }
    
    // Now parse the normalized string
    const num = parseFloat(normalized);
    //console.log(`Parsing "${str}" → ${num} (normalized: "${normalized}")`);
    
    return isNaN(num) ? 0 : num;
  }

  #format(num) {
    return `${this.#prefix}${this.#fmt.format(num)}${this.#suffix}`;
  }

  #clamp(val) {
    if (this.#wrap) {
      const span = this.#max - this.#min;
      if (span > 0) {
        return (((val - this.#min) % (span + this.#step) + (span + this.#step)) % (span + this.#step)) + this.#min;
      }
    }
    return Math.min(this.#max, Math.max(this.#min, val));
  }

  #syncDisplay(val) {
    this.#el.value = this.#format(val);
  }

  /*──────────── pointer handling (arrow zone drag) ───────────*/
  #onPointerDown(e) {
    if (this.#el.readOnly) return;

    const rect = this.#el.getBoundingClientRect();
    // Only start drag if inside arrow zone
    if (rect.right - e.clientX <= this.#arrowZone) {
      this.#el.setPointerCapture(e.pointerId);
      this.#dragStart = { y: e.clientY, value: this.getValue() ?? this.#min };
      this.#el.addEventListener("pointermove", this.#pointerMove);
      this.#el.addEventListener("pointerup",   this.#pointerUp, { once:true });
      e.preventDefault();
    }
  }

  #pointerMove = e => {
    if (!this.#dragStart) return;
    const dy = e.clientY - this.#dragStart.y;
    const newVal = this.#dragStart.value - dy * this.#step;
    this.#syncDisplay(this.#clamp(newVal));
    this.onChange(this.getValue(), false);
  };

  #pointerUp = () => {
    this.#el.removeEventListener("pointermove", this.#pointerMove);
    this.#dragStart = null;
    this.onChange(this.getValue(), true);
  };

  /*──────────── key & wheel helpers ───────────*/
  #nudge(dir) {
    const curr = this.getValue() ?? 0;
    this.#syncDisplay(this.#clamp(curr + dir * this.#step));
    this.onChange(this.getValue(), false);
  }
  #onWheel(e) {
    if (this.#el.readOnly) return;
    e.preventDefault();
    const dir = -Math.sign(e.deltaY);
    this.#nudge(dir);
    this.onChange(this.getValue(), true);
  }

  #onKeyDown(e) {
    if (this.#el.readOnly) return;
    if (e.key === "ArrowUp" || e.key === "ArrowDown") {
      e.preventDefault();
      const dir = e.key === "ArrowUp" ? 1 : -1;
      // first step immediately
      this.#nudge(dir);
      // then repeat while held
      if (!this.#repeatT) {
        this.#repeatT = setInterval(() => this.#nudge(dir), 150);
      }
    } else if (e.key === "Enter") {
      // Parse manual edit
      const v = this.#parse(this.#el.value);
      if (v !== null) this.#syncDisplay(this.#clamp(v));
      this.onChange(this.getValue(), true);
    }
  }
  #stopRepeat() {
    clearInterval(this.#repeatT);
    this.#repeatT = null;
    this.onChange(this.getValue(), true);
  }
}
