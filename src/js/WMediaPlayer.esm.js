// WMediaPlayer.js — ES-module, modern browsers (ES 2022+)

export default class WMediaPlayer {
  /**
   * @param {HTMLElement} host The widget root created by the server
   *                           (contains an <audio> or <video> element).
   */
  constructor(host) {
    /** public handle back to Wt */
    host.wtObj = this;

    /** @type {HTMLMediaElement|null} */
    this.media = host.querySelector('audio,video');
    if (!this.media) {
      throw new Error('WMediaPlayer – no <audio> or <video> element found');
    }

    // expose encoder for server-side synchronisation
    host.wtEncodeValue = this.encodeValue.bind(this);
  }

  /* ------------------------------------------------------------------ *
   *  Public API (keeps the original behaviour / method names)          *
   * ------------------------------------------------------------------ */

  /** Play the media */
  play()  { this.media.play(); }

  /** Pause the media */
  pause() { this.media.pause(); }

  /**
   * Set / get playback-rate (original code named this “wtPlaybackRate”).
   * @param  {number}  [rate] — omit to get current rate
   * @return {number|this}
   */
  wtPlaybackRate(rate) {
    if (rate === undefined) return this.media.playbackRate;
    this.media.playbackRate = rate;
    return this;
  }

  /**
   * Collect the current player state in exactly the same serialised
   * format the legacy code produced so that the C++ side keeps working.
   * @return {string}
   *
   *  volume;currentTime;duration;paused;ended;readyState;playbackRate;seek%
   */
  encodeValue() {
    const m       = this.media;
    const ready   = m.readyState;       // 0–4
    const paused  = m.paused ? 1 : 0;
    const ended   = m.ended  ? 1 : 0;
    const rate    = m.playbackRate || 1;
    const seekPct = m.duration
                  ? Math.round((m.currentTime / m.duration) * 100)
                  : 0;

    return [
      m.volume,           // 0 … 1
      m.currentTime,      // seconds
      m.duration || 0,    // seconds
      paused,
      ended,
      ready,
      rate,
      seekPct,
    ].join(';');
  }
}
