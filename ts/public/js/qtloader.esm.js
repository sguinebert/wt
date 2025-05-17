/* -------------------------------------------------------------------------
 * QtLoader – modern ES2022+ rewrite with progressive‑enhancement, offline
 * caching, and WebAssembly.instantiateStreaming()
 * -------------------------------------------------------------------------
 * Usage:
 *   import QtLoader from './QtLoader.js';
 *   const loader = new QtLoader({
 *     applicationName: 'myApp',
 *     containerElements: [document.getElementById('app')],
 *     path: '/wasm/',
 *     restartMode: 'RestartOnCrash',
 *   });
 *   loader.load();
 * ------------------------------------------------------------------------- */

 export default class QtLoader {
    /* -------------------------------------------------------------------
     * Public API
     * ----------------------------------------------------------------- */
    get module()      { return this.#module; }
    get status()      { return this.#status; }
    get wasmSupported(){ return typeof WebAssembly !== 'undefined'; }
    get webglSupported(){
      try {
        const c = document.createElement('canvas');
        return !!window.WebGLRenderingContext && (c.getContext('webgl') || c.getContext('experimental-webgl'));
      } catch { return false; }
    }
  
    /** Begin loading Qt (JS runtime + wasm) */
    async load() {
      if (!this.wasmSupported) throw new Error('WebAssembly not supported');
      if (!this.webglSupported) throw new Error('WebGL not supported');
  
      this.#setStatus('Loading');
  
      const base = this.#cfg.path.endsWith('/') ? this.#cfg.path : `${this.#cfg.path}/`;
      const app  = this.#cfg.applicationName;
  
      try {
        const [jsSource, wasmModule] = await Promise.all([
          this.#fetchText(`${base}${app}.js`),
          this.#getOrCompileWasm(`${base}${app}.wasm`),
        ]);
        await this.#bootEmscripten(jsSource, wasmModule);
      } catch (e) {
        this.#handleAbort(e);
        throw e;
      }
    }
  
    /* -------------------------------------------------------------------
     * Constructor / private state ( # fields )
     * ----------------------------------------------------------------- */
    /** @param {Object} cfg */
    constructor(cfg = {}) {
      this.#cfg = {
        path: '',
        restartMode: 'DoNotRestart',     // DoNotRestart | RestartOnExit | RestartOnCrash
        restartLimit: 5,
        stdoutEnabled: true,
        stderrEnabled: true,
        environment: {},
        ...cfg,
      };
    }
  
    /* --------------------------------------------------- private fields */
    #cfg;               // normalised user config
    #status = 'Created';
    #restartCount = 0;
    #module   = null;   // Emscripten module instance once running
    #wasmCache = null;  // IndexedDB helper (lazy)
  
    /* -------------------------------------------------------------------
     * Network helpers
     * ----------------------------------------------------------------- */
    async #fetchText(url){
      const res = await fetch(url);
      if (!res.ok) throw new Error(`${url} – ${res.status} ${res.statusText}`);
      return res.text();
    }
  
    /* ---------------- WASM caching pipeline --------------------------- */
    async #getOrCompileWasm(url){
      const idb = await this.#openWasmDB();
  
      // 1) compiled module cache
      const cached = await idb.get(url);
      if (cached instanceof WebAssembly.Module) return cached;
  
      // 2) fetch (Cache‑Storage first)
      const response = await this.#fetchAndCache(url);
  
      // 3) compile / instantiateStreaming
      let module;
      if (WebAssembly.instantiateStreaming){
        ({module} = await WebAssembly.instantiateStreaming(response.clone(), {}));
      } else if (WebAssembly.compileStreaming){
        module = await WebAssembly.compileStreaming(response.clone());
      } else {
        module = await WebAssembly.compile(await response.clone().arrayBuffer());
      }
  
      // 4) store compiled module (best‑effort)
      idb.set(url, module).catch(()=>{/* ignore */});
  
      return module;
    }
  
    async #fetchAndCache(url){
      const cache = await caches.open('qtloader‑wasm');
      let res = await cache.match(url);
      if (!res){
        res = await fetch(url, { integrity: this.#cfg.integrity });
        if (!res.ok) throw new Error(`Failed to fetch ${url}`);
        cache.put(url, res.clone());
      }
      return res;
    }
  
    /* ---------------- IndexedDB helper ------------------------------- */
    async #openWasmDB(){
      if (this.#wasmCache) return this.#wasmCache;
  
      const db = await new Promise((ok,err)=>{
        const req = indexedDB.open('QtLoaderWasmCache',1);
        req.onupgradeneeded=()=>req.result.createObjectStore('wasm');
        req.onsuccess=()=>ok(req.result);
        req.onerror =()=>err(req.error);
      });
  
      const wrap = {
        get:(k)=>new Promise(r=>{
          db.transaction('wasm').objectStore('wasm').get(k).onsuccess=e=>r(e.target.result||null);
        }),
        set:(k,v)=>new Promise(r=>{
          const tx=db.transaction('wasm','readwrite');
          tx.objectStore('wasm').put(v,k).onsuccess=()=>r();
        }),
      };
      return (this.#wasmCache = wrap);
    }
  
    /* ---------------- Boot Emscripten runtime ------------------------- */
    async #bootEmscripten(jsSource, wasmModule){
      const cfg = this.#cfg;
      const modCfg = {
        locateFile:(f)=>`${cfg.path}${f}`,
        instantiateWasm:(imports,cb)=>{
          WebAssembly.instantiate(wasmModule,imports).then(({instance})=>cb(instance,wasmModule));
          return {};
        },
        print:  cfg.stdoutEnabled ? console.log : ()=>{},
        printErr: cfg.stderrEnabled ? console.error : ()=>{},
        onAbort: (m)=>this.#handleAbort(m),
        quit:    (c,e)=>this.#handleQuit(c,e),
        preRun:[m=>Object.assign(m.ENV,cfg.environment)],
        setStatus:(txt)=>{ if(txt.startsWith('Running')) this.#setStatus('Running'); },
      };
  
      const blobURL = URL.createObjectURL(new Blob([jsSource],{type:'text/javascript'}));
      const moduleFactory = (await import(blobURL)).default;
      URL.revokeObjectURL(blobURL);
  
      this.#module = await moduleFactory(modCfg);
      this.#setStatus('Running');
    }
  
    /* ---------------- Error / exit handling -------------------------- */
    #handleAbort(msg){
      console.error('QtLoader abort:',msg);
      this.#setStatus('Error');
      this.#cfg.onAbort?.(msg);
    }
  
    #handleQuit(code,exc){
      if(code===0) return; // clean exit
      console.warn('QtLoader quit:',code,exc);
      this.#setStatus('Exited');
      this.#cfg.onQuit?.(code,exc);
      if(this.#cfg.restartMode==='RestartOnCrash') this.#tryRestart();
    }
  
    #tryRestart(){
      if(++this.#restartCount>this.#cfg.restartLimit) return;
      this.load().catch(e=>console.error('Restart failed:',e));
    }
  
    /* ---------------- Status ----------------------------------------- */
    #status;
    #setStatus(s){
      if(this.#status===s) return;
      this.#status = s;
      this.#cfg.statusChanged?.(s);
    }
  }
  