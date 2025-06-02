// QtLoader.js — ES2022+ rewrite (rev‑6)
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------

export default class QtLoader {
  /*──────────────── public enums ────────────────*/
  static Status = /** @type {const} */ ({
    Created:  "Created",
    Loading:  "Loading",
    Running:  "Running",
    Exited:   "Exited"
  });

  /*──────────────── browsers feature probes ─────*/
  static get wasmSupported() {
    return typeof WebAssembly !== "undefined";
  }
  static get webglSupported() {
    try {
      const c = document.createElement("canvas");
      return (
        !!window.WebGLRenderingContext &&
        (c.getContext("webgl") || c.getContext("experimental-webgl"))
      );
    } catch {
      return false;
    }
  }

  /*──────────────── private static utilities ───*/
  static #ensureCanvas(el) {
    if (el.tagName === "CANVAS") return el;
    const c = document.createElement("canvas");
    c.style.cssText =
      "width:100%;height:100%;outline:0 solid transparent;caret-color:transparent;cursor:default;";
    el.appendChild(c);
    return c;
  }

  static #mkdirs(FS, fullPath) {
    const parts = fullPath.split("/").slice(0, -1);
    let cur = "/";
    for (const p of parts) {
      if (!p) continue;
      cur += p + "/";
      try {
        FS.mkdir(cur);
      } catch (e) {
        const EEXIST = 20;
        if (e.errno !== EEXIST) throw e;
      }
    }
  }

  static #split(p) {
    const arr = p.split("/");
    const name = arr.pop();
    return { dir: arr.join("/"), name };
  }

  static #wasiStubs() {
    const notImpl = (n) => (...a) => {
      console.warn(`WASI stub '${n}' called`, a);
      return 0;
    };
    return {
      proc_exit: (code) => {
        throw new Error(`WASM exited via proc_exit(${code})`);
      },
      fd_write: notImpl("fd_write"),
      fd_close: notImpl("fd_close"),
      fd_seek: notImpl("fd_seek"),
      fd_read: notImpl("fd_read"),
      environ_sizes_get: notImpl("environ_sizes_get"),
      environ_get: notImpl("environ_get"),
      clock_time_get: notImpl("clock_time_get")
    };
  }

  /*──────────────── private fields ─────────────*/
  #cfg;
  #status = QtLoader.Status.Created;
  #module = null;
  #canvases = [];
  #restartCount = 0;

  /*──────────────── constructor ────────────────*/
  /** @param {QtLoader.Config} cfg */
  constructor(cfg) {
    if (!cfg?.applicationName) throw new Error("applicationName is required");
    if (!cfg?.path) throw new Error("path is required");

    const def = {
      path: "./",
      applicationName: "app",
      environment: {},
      containerElements: [],
      fontDpi: 96,
      preload: [],
      module: undefined,
      onLoaded: undefined,
      onExit: undefined,
      entryFunction: undefined,
      restartMode: "DoNotRestart",
      restartLimit: 5,
      statusChanged: undefined,
      debug: false
    };
    this.#cfg = { ...def, ...cfg };
    if (!this.#cfg.path.endsWith("/")) this.#cfg.path += "/";

    this.#canvases = (this.#cfg.containerElements ?? []).map(QtLoader.#ensureCanvas);
  }

  /*──────────────── getters ────────────────────*/
  get status() {
    return this.#status;
  }
  get module() {
    return this.#module;
  }

  /*──────────────── public API ─────────────────*/
  async load() {
    if (!QtLoader.wasmSupported) throw new Error("WebAssembly not supported");
    if (!QtLoader.webglSupported) throw new Error("WebGL not supported");

    this.#setStatus(QtLoader.Status.Loading);

    const jsUrl = `${this.#cfg.path}${this.#cfg.applicationName}.js`;
    const wasmUrl = `${this.#cfg.path}${this.#cfg.applicationName}.wasm`;

    try {
      // 1️⃣ try ES‑module / MODULARIZE
      this.#debug("Attempting dynamic import …");
      const factory = await this.#tryImportFactory(jsUrl);
      if (factory) {
        await this.#bootWithFactory(factory);
        return this.#module;
      }

      // 2️⃣ try monolithic stub via <script>
      this.#debug("Falling back to <script> stub …");
      const stub = await this.#tryScriptLoader(jsUrl);
      if (stub) {
        this.#module = stub;
        this.#setStatus(QtLoader.Status.Running);
        this.#cfg.onLoaded?.();
        return stub;
      }

      // 3️⃣ stand‑alone WASM
      this.#debug("Manual instantiateStreaming …");
      const { instance } = await WebAssembly.instantiateStreaming(fetch(wasmUrl), this.#makeImports());
      this.#module = instance.exports;
      this.#setStatus(QtLoader.Status.Running);
      this.#cfg.onLoaded?.();
      return this.#module;
    } catch (err) {
      this.#handleExit({ crashed: true, text: err?.message, code: err?.code });
      throw err;
    }
  }

  /*──────────────── private helpers ───────────*/
  async #bootWithFactory(factory) {
    const customInstantiate = this.#cfg.module
      ? (imports, ok) => {
          this.#cfg.module.then((mod) =>
            WebAssembly.instantiate(mod, imports).then((inst) => ok(inst, mod))
          );
          return {};
        }
      : undefined;

    const Module = {
      ENV: { ...this.#cfg.environment },
      qtContainerElements: this.#canvases,
      qtFontDpi: this.#cfg.fontDpi,
      locateFile: (n) => (n.startsWith("libQt6") ? `${this.#cfg.path}qt/lib/${n}` : n),
      instantiateWasm: customInstantiate,
      noInitialRun: true,
      preRun: [this.#preRunHook.bind(this)],
      onRuntimeInitialized: () => {
        this.#cfg.onLoaded?.();
        if (!this.#cfg.entryFunction) this.#module.callMain?.([]);
      },
      quit: (code) => this.#handleExit({ code, crashed: false }),
      onAbort: (t) => this.#handleExit({ text: t, crashed: true }),
      print: (t) => console.log("[stdout]", t),
      printErr: (t) => console.warn("[stderr]", t)
    };

    this.#module = await factory(Module);
    if (this.#cfg.entryFunction) this.#module.ccall(this.#cfg.entryFunction);
    this.#setStatus(QtLoader.Status.Running);
  }

  #preRunHook(instance) {
    if (!this.#cfg.preload?.length) return;
    if (!instance.FS) throw new Error("FS export needed for preload");
    for (const file of this.#cfg.preload) {
      const src = file.source.replace("$QTDIR", `${this.#cfg.path}qt`);
      QtLoader.#mkdirs(instance.FS, file.destination);
      const { dir, name } = QtLoader.#split(file.destination);
      instance.FS.createPreloadedFile(dir, name, src, true, true);
    }
  }

  async #tryImportFactory(url) {
    try {
      const mod = await import(/* webpackIgnore: true */ url + `?v=${Date.now()}`);
      if (typeof mod.default === "function") return mod.default;
    } catch {}
    return null;
  }

  async #tryScriptLoader(url) {
    await new Promise((res, rej) => {
      const s = document.createElement("script");
      s.src = url + `?v=${Date.now()}`;
      s.async = true;
      s.onload = res;
      s.onerror = () => rej(new Error("Failed to load runtime script"));
      document.head.appendChild(s);
    });

    // classic Module global
    if (globalThis.Module && typeof globalThis.Module === "object") {
      await new Promise((ok) => {
        if (globalThis.Module.calledRun) return ok();
        const prev = globalThis.Module.onRuntimeInitialized;
        globalThis.Module.onRuntimeInitialized = (...a) => {
          prev?.(...a);
          ok();
        };
      });
      return globalThis.Module;
    }

    // UMD factory (MODULARIZE without EXPORT_ES6)
    const factoryName = this.#cfg.applicationName.replace(/[^A-Za-z0-9_$]/g, "_") + "_entry";
    const fn = globalThis[factoryName];
    if (typeof fn === "function") {
      this.#debug(`Found UMD factory '${factoryName}' …`);
      const mod = await fn({
        qtContainerElements: this.#canvases,
        ENV: { ...this.#cfg.environment },
        noInitialRun: true,
        preRun: [this.#preRunHook.bind(this)],
        onRuntimeInitialized: () => this.#cfg.onLoaded?.(),
        locateFile: (n) => (n.startsWith("libQt6") ? `${this.#cfg.path}qt/lib/${n}` : n),
        quit: (code) => this.#handleExit({ code, crashed: false }),
        onAbort: (t) => this.#handleExit({ text: t, crashed: true })
      });
      return mod;
    }

    return null;
  }

  #makeImports() {
    const env = {
      memory: new WebAssembly.Memory({ initial: 32 }),
      ...this.#cfg.environment
    };
    return { env, wasi_snapshot_preview1: QtLoader.#wasiStubs() };
  }

  #handleExit({ text = "", code = undefined, crashed = false }) {
    this.#setStatus(QtLoader.Status.Exited);
    this.#cfg.onExit?.({ text, code, crashed });
    if (this.#shouldRestart(crashed)) {
      this.#restartCount += 1;
      this.load();
    }
  }

  #shouldRestart(crashed) {
    const { restartMode, restartLimit } = this.#cfg;
    if (this.#restartCount >= restartLimit) return false;
    if (restartMode === "RestartOnExit" && !crashed) return true;
    if (restartMode === "RestartOnCrash" && crashed) return true;
    return false;
  }

  #setStatus(s) {
    if (this.#status !== s) {
      this.#status = s;
      this.#cfg.statusChanged?.(s);
    }
  }

  #debug(...m) {
    if (this.#cfg.debug) console.debug("[QtLoader]", ...m);
  }
}


