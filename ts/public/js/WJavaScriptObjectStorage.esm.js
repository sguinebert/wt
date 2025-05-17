// WJavaScriptObjectStorage.js  (ES-module, no global macros)

/*  Copyright © 2024  */
/*  Released under the same license as the original file.             */

/* --------------------------------------------------------------- *\
   A tiny “KV store” that lets a widget keep arbitrary JS data and
   later serialise only the fields that actually changed.
\* --------------------------------------------------------------- */

export default class JavaScriptObjectStorage {
  #values   = [];        // live     values
  #previous = new Map(); // snapshot values (deep-cloned)

  /* ---------- utilities ---------------------------------------- */

  /** Is it an object literal? (`{}` created with {} or new Object) */
  static #isPlain = v =>
    Object.prototype.toString.call(v) === '[object Object]';

  /** Recursively (de)clones using the structured-clone algorithm. */
  static #clone = globalThis.structuredClone
    ? v => structuredClone(v)                           // native
    : v => JSON.parse(JSON.stringify(v));              // fallback

  /** Deep equality for JSON-serialisable data (object / array / lit) */
  static #deepEqual(a, b) {
    if (a === b) return true;

    if (Array.isArray(a) && Array.isArray(b))
      return a.length === b.length &&
             a.every((v, i) => this.#deepEqual(v, b[i]));

    if (this.#isPlain(a) && this.#isPlain(b)) {
      const aKeys = Object.keys(a);
      const bKeys = Object.keys(b);
      return aKeys.length === bKeys.length &&
             aKeys.every(k => b.hasOwnProperty(k) &&
                              this.#deepEqual(a[k], b[k]));
    }

    return false;
  }

  /** Painter-path heuristic (unchanged from original) */
  static #isPainterPath(v) { return Array.isArray(v) && v.length > 6; }

  /* ---------- public API --------------------------------------- */

  /**
   * @param {number} idx
   * @param {*}      value – anything JSON-serialisable
   */
  set(idx, value) {
    if (!JavaScriptObjectStorage.#isPainterPath(value)) {
      // keep a snapshot so we can later diff efficiently
      this.#previous.set(idx, JavaScriptObjectStorage.#clone(value));
    }
    this.#values[idx] = value;
  }

  /**
   * Serialises ONLY the entries that changed since the last snapshot.
   * Call this from your widget’s `wtEncodeValue`.
   */
  encodeChanged() {
    const dirty = {};
    this.#values.forEach((val, idx) => {
      if (JavaScriptObjectStorage.#isPainterPath(val)) return;

      const old = this.#previous.get(idx);
      if (!JavaScriptObjectStorage.#deepEqual(val, old))
        dirty[idx] = val;
    });
    return JSON.stringify(dirty);
  }
}
