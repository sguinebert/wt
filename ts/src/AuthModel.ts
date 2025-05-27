// AuthThrottle.ts – ES2022+ TypeScript

export default class AuthThrottle {
  #intervalId: number | null = null;     // active interval (null = idle)
  #remaining = 0;                        // seconds left in the countdown
  #origHTML = '';                        // cached button label

  /**
   * @param button        The button to throttle
   * @param textTemplate  Countdown text, use `{1}` for seconds
   *                      e.g. "Retry in {1}s"
   */
  constructor(
    private readonly button: HTMLButtonElement,
    private readonly textTemplate: string = 'Retry in {1}s',
  ) {
    if (!(button instanceof HTMLButtonElement)) {
      throw new TypeError('AuthThrottle expects an <button> element');
    }
  }

  /** Starts / restarts the throttle. `seconds` ≤ 0 cancels it. */
  reset(seconds: number): void {
    this.#stop();                      // clear any previous countdown
    if (seconds <= 0) return;

    this.#origHTML       = this.button.innerHTML;
    this.button.disabled = true;
    this.#remaining      = seconds;

    // first paint immediately …
    this.#render();

    // … then update once per second
    this.#intervalId = window.setInterval(() => {
      this.#remaining -= 1;
      this.#render();
      if (this.#remaining <= 0) this.#stop();
    }, 1000);
  }

  /* --------------------------------------------------------------------- */
  /* internal helpers                                                      */

  /** Replace `{1}` placeholder & paint. */
  #render(): void {
    this.button.innerHTML =
      this.textTemplate.replace('{1}', String(this.#remaining));
  }

  /** Restore original button state & clear timer. */
  #stop(): void {
    if (this.#intervalId !== null) {
      clearInterval(this.#intervalId);
      this.#intervalId = null;
    }
    if (this.#origHTML) {
      this.button.innerHTML = this.#origHTML;
      this.button.disabled  = false;
      this.#origHTML        = '';
    }
  }
}
