// WAbstractMedia.js
// A drop-in replacement for the legacy “WAbstractMedia” helper.

export default class WAbstractMedia {
  /**
   * @param {HTMLElement} host    – wrapper node that carries `mediaId` / `alternativeId` data-attrs
   * @param {(id:string)=>HTMLElement} [lookup] – custom id-lookup (defaults to getElementById)
   */
  constructor(host, lookup = id => document.getElementById(id)) {
    this.host   = host;
    this.lookup = lookup;

    /* give old code a way to reach us */
    host.wtObj         = this;
    host.wtEncodeValue = this.encodeValue;
  }

  /* -------- convenience getters -------- */

  get media()       { return this.lookup(this.host.mediaId     ?? '') ?? null; }
  get alternative() { return this.lookup(this.host.alternativeId ?? '') ?? null; }

  /* -------- public API expected by Wt ---------- */

  /** play() → try real <audio>/<video>, else custom fallback */
  play  = () => this.media?.play()  ?? this.alternative?.WtPlay?.();
  /** pause() → idem */
  pause = () => this.media?.pause() ?? this.alternative?.WtPause?.();

  /**
   * Serialise the current playback state for the server.
   * Format: `volume;current;duration;paused;ended;readyState`
   * @returns {string|null}
   */
  encodeValue = () => {
    const m = this.media;
    if (!m) return null;                          // nothing to report

    const { volume, currentTime, duration, paused, ended, readyState } = m;
    return [
      volume,
      currentTime,
      readyState >= 1 ? duration : 0,              // metadata loaded?
      paused ? 1 : 0,
      ended  ? 1 : 0,
      readyState
    ].join(';');
  };
}
