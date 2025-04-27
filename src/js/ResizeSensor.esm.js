// ResizeSensor.js — ES 2022
// Lightweight size‑change observer for modern browsers (ResizeObserver‑only).
// Usage:
//   import { ResizeSensor } from './ResizeSensor.js';
//   const sensor = new ResizeSensor(el, (w, h) => console.log(w, h));
//   // … later
//   sensor.disconnect();

export class ResizeSensor {
  #observer;
  #w = 0;
  #h = 0;

  /**
   * @param {HTMLElement} el   Element to observe
   * @param {(width:number,height:number)=>void} cb  Callback on size change
   */
  constructor(el, cb) {
    if (!(el instanceof HTMLElement)) {
      throw new TypeError('ResizeSensor expects an HTMLElement');
    }
    if (typeof cb !== 'function') {
      throw new TypeError('Callback must be a function');
    }

    this.#observer = new ResizeObserver(([entry]) => {
      const { width, height } = entry.contentRect;
      if (width === this.#w && height === this.#h) return; // unchanged
      this.#w = width;
      this.#h = height;
      cb(width, height);
    });

    this.#observer.observe(el);
  }

  /** Stop observing */
  disconnect() {
    this.#observer.disconnect();
  }
}
