/**
 * Lightweight wrapper that embeds a TinyMCE instance in `host`
 * and keeps it perfectly sized with ResizeObserver.
 */
export default class TextEdit {
  #app;                // reference to the WT-ish app (for emit)
  #host;               // HTMLElement that becomes the editor
  #editor = null;      // TinyMCE editor instance
  #ro;                 // ResizeObserver
  #cfg;                // user-supplied editor config

  /**
   * @param {object}          app           your APP object (must expose app.emit)
   * @param {HTMLElement}     host          the <textarea/ div> turned into the editor
   */
  constructor(app, host) {
    if (!(host instanceof HTMLElement)) {
      throw new TypeError('TextEdit expects an HTMLElement as host');
    }
    this.#app  = app;
    this.#host = host;
    host.wtObj = this;                 // keep original contract
    this.#ro   = new ResizeObserver(() => this.#syncSize());
    this.#ro.observe(host);
  }

  /** Inject & initialise TinyMCE. Call once after constructing. */
  async render(config = {}, css = '', connectOnChange = true) {
    this.#cfg = {
      target: this.#host,
      ...config,
      setup: (editor) => {
        this.#editor = editor;
  
        // Handle TinyMCE's internal resize events
        editor.on('ResizeEditor', () => {
          this.#syncSize();
        });

        if (config.setup) {
          config.setup(editor); // Call user-provided setup function, if any
        }
      },
    };

    await this.#loadTinyMCE();
    this.#editor = (await tinymce.init(this.#cfg))[0];

    // propagate change events back to the server
    if (connectOnChange) {
      this.#editor.on('Change Input Undo Redo', () =>
        this.#app?.emit?.(this.#host, 'change')
      );
    }

    // expose a “render” signal just after init
    this.#editor.once('init', () =>
      this.#app?.emit?.(this.#host, 'render')
    );

    // inline-image pasting
    this.#editor.on('Paste', this.#handlePaste);

    // set optional extra CSS on the iframe’s <html>
    if (css) this.#editor.on('init', () => {
      const doc = this.#editor.getDoc();
      doc.documentElement.style.cssText += css;
    });

    this.#syncSize();                  // initial sizing
  }

  /** Destroy editor + observers. */
  dispose() {
    this.#ro.disconnect();
    this.#editor && tinymce.remove(this.#editor);
    this.#editor = null;
  }

  /* ------------------------------------------------------------------ */
  /* internal helpers                                                   */

  async #loadTinyMCE() {
    if (window.tinymce) return;
    await import(TINYMCE_CDN);
  }

  #handlePaste = (evt) => {
    const { clipboardData } = evt;
    if (!clipboardData?.items) return;

    for (const item of clipboardData.items) {
      if (item.type.startsWith('image/')) {
        evt.preventDefault();
        const file    = item.getAsFile();
        const reader  = new FileReader();
        reader.onload = () =>
          this.#editor.insertContent(`<img src="${reader.result}"/>`);
        reader.readAsDataURL(file);
      }
    }
  };

  #syncSize() {
    if (!this.#editor) return;
    const { width, height } = this.#host.getBoundingClientRect();
    const iframe = this.#host.parentNode.querySelector('iframe');
    if (iframe) {
      iframe.style.width  = `${width}px`;
      iframe.style.height = `${height}px`;
    }
  }
}