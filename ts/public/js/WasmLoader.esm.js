/* -------------------------------------------------------------------------
 * WasmLoader – modern ES2022+ rewrite with progressive‑enhancement, offline
 * caching, and WebAssembly.instantiateStreaming()
 * -------------------------------------------------------------------------
 * Usage:
 *   import WasmLoader from './WasmLoader.js';
 *   const loader = new WasmLoader({
 *     applicationName: 'myApp',
 *     containerElements: [document.getElementById('app')],
 *     path: '/wasm/',
 *     restartMode: 'RestartOnCrash',
 *   });
 *   loader.load();
 * ------------------------------------------------------------------------- */

 export default class WasmLoader {
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
  
    /** Begin loading Wasm (JS runtime + wasm) */
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
    #status = 'NotStarted'; // current status of the loader
    #restartCount = 0;
    #module   = null;   // Emscripten module instance once running
    #wasmCache = null;  // IndexedDB helper (lazy)
  
    /* -------------------------------------------------------------------
     * Network helpers
     * ----------------------------------------------------------------- */
    async #fetchText(url) {
      const cache = await caches.open('loader-text');
      let res = await cache.match(url);
      
      if (!res) {
        res = await fetch(url, this.#cfg.integrity ? { integrity: this.#cfg.integrity } : {});
        if (!res.ok) throw new Error(`${url} – ${res.status} ${res.statusText}`);
        await cache.put(url, res.clone());
      }
      
      return res.text();
    }
      
    /* ---------------- WASM caching pipeline --------------------------- */
    async #getOrCompileWasm(url){
      const idb = await this.#openWasmDB();

      console.log('idb', idb);
      // 1) compiled module cache
      const cached = await idb.get(url);
      console.log('cached', cached);
      if (cached instanceof WebAssembly.Module) {
        console.log(`[WASM Cache] ✅ Found compiled module in IndexedDB cache`);
        return cached;
      } else if (cached instanceof ArrayBuffer) {
        // Handle ArrayBuffer fallback case - compile it now
        console.log(`[WASM Cache] ✅ Found ArrayBuffer in cache, compiling...`);
        try {
          const module = await WebAssembly.compile(cached);
          console.log(`[WASM Cache] Compiled successfully from cached ArrayBuffer`);
          return module;
        } catch (compileErr) {
          console.error(`[WASM Cache] Failed to compile cached ArrayBuffer:`, compileErr);
          // Fall through to fetch new copy
        }
      }

      console.log(`[WASM Cache] ❌ No cached module found, fetching from network`);


      // 2) fetch (Cache‑Storage first)
      const module = await this.#fetchAndCompile(url);

      // 3) compile only (no instantiation)
      // let module;
      // if (WebAssembly.compileStreaming){
      //   module = await WebAssembly.compileStreaming(response.clone());
      // } else {
      //   module = await WebAssembly.compile(await response.clone().arrayBuffer());
      // }

      // 4) store compiled module (best‑effort)
      try {
        console.log(`[WASM Cache] Storing compiled module in IndexedDB for ${url}`);
        await idb.set(url, module).catch(err => {
          console.warn(`[WASM Cache] Failed to store in IndexedDB:`, err);
        });
        console.log(`[WASM Cache] Storage operation completed`);
      } catch (storeErr) {
        console.warn(`[WASM Cache] Exception during storage:`, storeErr);
        // Continue despite error - this is best-effort
      }
      return module;
    }
  
    async #fetchAndCompile(url){
      const cache    = await caches.open('loader-wasm');
      let   response = await cache.match(url);            

      /* ---------- fetch + cache on a cold start ---------- */
      if (!response) {
        response = await fetch(url, { integrity: this.#cfg.integrity });
        if (!response.ok) {
          throw new Error(`Failed to fetch ${url}: ${response.status} ${response.statusText}`);
        }
        await cache.put(url, response.clone());           // raw bytes saved for next launch
      }

      /* ---------- compile (streaming if possible) ---------- */
      try {
        if (WebAssembly.compileStreaming) {
          return await WebAssembly.compileStreaming(response.clone());
        }
        const buf = await response.arrayBuffer();         // Safari ≤ 16
        return await WebAssembly.compile(buf);
      } catch (err) {
        console.error(`WASM compile failed for ${url}:`, err);
        throw new Error(`Failed to compile WebAssembly module from ${url}`);
      }
    }
  
    /* ---------------- IndexedDB helper ------------------------------- */
    async #openWasmDB() {
      if (this.#wasmCache) return this.#wasmCache;

      const db = await new Promise((ok,err) => {
        const req = indexedDB.open('LoaderWasmCache', 1);
        req.onupgradeneeded = () => req.result.createObjectStore('wasm');
        req.onsuccess = () => ok(req.result);
        req.onerror = () => err(req.error);
      });

      const wrap = {
        get: (k) => new Promise(r => {
          console.log(`[IDB] Attempting to get ${k}`);
          const request = db.transaction('wasm').objectStore('wasm').get(k);
          request.onsuccess = e => {
            const result = e.target.result;
            const resultType = result ? 
              (result instanceof WebAssembly.Module ? 'WebAssembly.Module' : 
              result instanceof ArrayBuffer ? 'ArrayBuffer' : typeof result) : 'null';
            console.log(`[IDB] Got result type: ${resultType}`);
            r(result || null);
          };
          request.onerror = e => {
            console.error(`[IDB] Error getting ${k}:`, e.target.error);
            r(null);
          };
        }),
        set: (k, v) => new Promise((resolve, reject) => {
          const valueType = v instanceof WebAssembly.Module ? 'WebAssembly.Module' : 
                          v instanceof ArrayBuffer ? 'ArrayBuffer' : typeof v;
          console.log(`[IDB] Setting ${k} with type: ${valueType}`);
          
          try {
            const tx = db.transaction('wasm', 'readwrite');
            const store = tx.objectStore('wasm');
            const request = store.put(v, k);
            
            request.onsuccess = () => {
              console.log(`[IDB] Successfully stored ${k}`);
              resolve();
            };
            request.onerror = (e) => {
              console.error(`[IDB] Error storing ${k}:`, e.target.error);
              reject(e.target.error);
            };
          } catch (err) {
            console.error(`[IDB] Exception in set operation:`, err);
            reject(err);
          }
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
            console.log("Is wasmModule a WebAssembly.Module?", wasmModule instanceof WebAssembly.Module);
          WebAssembly.instantiate(wasmModule, imports)
            .then(instance => {
              // For pre-compiled modules, we get the instance directly
              console.log("WASM instantiated successfully");
              return cb(instance, wasmModule);
            })
            .catch(err => {
              console.error("WASM instantiation failed:", err);
              this.#handleAbort(err);
            });
          return {};
        },
        print:  cfg.stdoutEnabled ? console.log : ()=>{},
        printErr: cfg.stderrEnabled ? console.error : ()=>{},
        onAbort: (m)=>this.#handleAbort(m),
        quit:    (c,e)=>this.#handleQuit(c,e),
        preRun: [m => {
          if (!m.ENV) m.ENV = {};
          if (cfg.environment) Object.assign(m.ENV, cfg.environment);
        }],
        setStatus:(txt)=>{ if(txt.startsWith('Running')) this.#setStatus('Running'); },
      };


      //window.eval(jsSource);
    
      try {
        // Define global Module that the Emscripten code will use
        window.Module = modCfg;
        
        // Execute the JS code directly rather than trying to import it as a module
        const scriptElement = document.createElement('script');
        const scriptBlob = new Blob([jsSource], { type: 'text/javascript' });
        const scriptURL = URL.createObjectURL(scriptBlob);
        
        // Wait for the script to load and execute
        await new Promise((resolve, reject) => {
          scriptElement.onload = resolve;
          scriptElement.onerror = (e) => reject(new Error("Failed to load WASM JS: " + e));
          scriptElement.src = scriptURL;
          document.head.appendChild(scriptElement);
        });
        
        // Store the Module instance
        this.#module = window.Module;
        this.#setStatus('Running');
        
        // Clean up
        document.head.removeChild(scriptElement);
        URL.revokeObjectURL(scriptURL);
        
      } catch (err) {
        console.error("Error initializing Emscripten module:", err);
        this.#handleAbort(err);
        throw err;
      } 


      // const blobURL = URL.createObjectURL(new Blob([jsSource],{type:'text/javascript'}));
      // const moduleFactory = (await import(blobURL)).default;
      // URL.revokeObjectURL(blobURL);
  
      // this.#module = await moduleFactory(modCfg);
      this.#setStatus('Running');
    }
  
    /* ---------------- Error / exit handling -------------------------- */
    #handleAbort(msg){
      console.error('WasmLoader abort:',msg);
      this.#setStatus('Error');
      this.#cfg.onAbort?.(msg);
    }
  
    #handleQuit(code,exc){
      if(code===0) return; // clean exit
      console.warn('WasmLoader quit:',code,exc);
      this.#setStatus('Exited');
      this.#cfg.onQuit?.(code,exc);
      if(this.#cfg.restartMode==='RestartOnCrash') this.#tryRestart();
    }
  
    #tryRestart(){
      if(++this.#restartCount>this.#cfg.restartLimit) return;
      this.load().catch(e=>console.error('Restart failed:',e));
    }
  
    /* ---------------- Status ----------------------------------------- */
    #setStatus(s){
      if(this.#status===s) return;
      this.#status = s;
      this.#cfg.statusChanged?.(s);
    }
  }
  