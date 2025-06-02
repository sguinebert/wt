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

async function unregisterServiceWorkers() {
  if (!navigator.serviceWorker) return false;
  
  try {
    // Get all registered service workers
    const registrations = await navigator.serviceWorker.getRegistrations();
    
    // Unregister each one
    await Promise.all(
      registrations.map(registration => {
        console.log('[SW] Unregistering:', registration.scope);
        return registration.unregister();
      })
    );
    
    console.log('[SW] All service workers unregistered');
    window.swReg = null;
    return true;
  } catch (err) {
    console.error('[SW] Error unregistering service workers:', err);
    return false;
  }
}
//await unregisterServiceWorkers();

  if(navigator.serviceWorker && (!window.swReg || !window.swReg.installing)) {
    /* -----------------------------------------------------------
    * Register the service worker
    * This will cache the assets listed in the service worker
    * --------------------------------------------------------- */
      //window.swReg = await navigator.serviceWorker.register('./js/wasm-sw.js', { scope: '/js/' });
      window.swReg = await navigator.serviceWorker.register('./wasm-sw.js', { scope: '/' });
      console.log('Service Worker registered:', window.swReg);

  // Only wait for controllerchange if the page isn't already controlled
    // if (!navigator.serviceWorker.controller) {
    //   console.log("Waiting for service worker to control the page...");
      
    //   // Add timeout to prevent infinite hanging
    //   await Promise.race([
    //     new Promise(resolve => {
    //       navigator.serviceWorker.addEventListener('controllerchange', () => {
    //         console.log("Service worker now controlling page");
    //         resolve();
    //       });
    //     }),
    //     new Promise(resolve => {
    //       // Timeout after 3 seconds
    //       setTimeout(() => {
    //         console.warn("Service worker controllerchange timeout - continuing anyway");
    //         resolve();
    //       }, 3000);
    //     })
    //   ]);
    // } else {
    //   console.log("Page already controlled by service worker:", 
    //               navigator.serviceWorker.controller.scriptURL);
    // }
  }

  /*  Now you can postMessage to register url to catch and cache, 
  * swReg.active.postMessage({ type: 'CACHE_ASSETS', urls: ['/module.wasm'] }); */
  // -------------------------------------------------------------------------



 export default class WasmLoader {
    /* --------------------------------------------------- private fields */
    #sw;                // service worker active?
    #cfg;               // normalised user config
    #status = 'NotStarted'; // current status of the loader
    #restartCount = 0;
    #module   = null;   // Emscripten module instance once running
    #wasmCache = null;  // IndexedDB helper (lazy)

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
      this.#sw = !!navigator.serviceWorker && !!window.swReg && !!window.swReg.active;
      if (!this.#cfg.applicationName) {
        throw new Error('applicationName is required in WasmLoader config');
      }
      if (!this.#cfg.path) {
        throw new Error('path is required in WasmLoader config');
      }
      if (this.#cfg.restartMode !== 'DoNotRestart' && this.#cfg.restartMode !== 'RestartOnExit' && this.#cfg.restartMode !== 'RestartOnCrash') {
        throw new Error('Invalid restartMode in WasmLoader config');
      }
      const base = this.#cfg.path.endsWith('/') ? this.#cfg.path : `${this.#cfg.path}/`;
      const app = this.#cfg.applicationName;
      this.jsUrl = new URL(`${base}${app}.js`, window.location.href).href;
      this.wasmUrl = new URL(`${base}${app}.wasm`, window.location.href).href;
      if(this.#sw) {
        // Register the service worker to cache assets
        window.swReg.active.postMessage({
          type: 'CACHE_ASSETS',
          urls: [
                  this.wasmUrl,
                  this.jsUrl
                ],
        });
      }

    }

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
       //const jsSource = await this.#loadScriptFile(`${base}${app}.js`);
       //const wasmModule = await this.#getOrCompileWasm(`${base}${app}.wasm`);
        // const [jsSource, wasmModule] = await Promise.all([
        //   this.#loadScriptFile(`${base}${app}.js`),
        //   this.#getOrCompileWasm(`${base}${app}.wasm`),
        // ]);
        const wasmModule = {
          url: this.wasmUrl,
          memory: new WebAssembly.Memory({ 
            initial: this.#cfg.initialMemory || 32,
            maximum: this.#cfg.maximumMemory || undefined
          }),
          table: new WebAssembly.Table({ 
            initial: this.#cfg.initialTable || 0, 
            element: 'anyfunc' 
          })
        };
    
        await this.#bootEmscripten(this.jsUrl, wasmModule);
      } catch (e) {
        this.#handleAbort(e);
        throw e;
      }
    }
  

  
    /* -------------------------------------------------------------------
     * Network helpers
     * ----------------------------------------------------------------- */
    // async #fetchText(url) {
    //   let res = await fetch(url, this.#cfg.integrity ? { integrity: this.#cfg.integrity } : {});
    //   if (!res.ok) {
    //     throw new Error(`Failed to fetch ${url}: ${res.status} ${res.statusText}`);
    //   }
    //   return res.text();
    // }
    // async #loadScriptFile(fileUrl) {
    //   // Create script element
    //   const scriptElement = document.createElement('script');
      
    //   // Return promise that resolves when script loads
    //   await new Promise((resolve, reject) => {
    //     scriptElement.onload = resolve;
    //     scriptElement.onerror = (e) => reject(new Error(`Failed to load script from ${fileUrl}: ${e}`));
        
    //     // Set src to direct file URL instead of blob
    //     scriptElement.src = fileUrl;
    //     document.head.appendChild(scriptElement);
    //   });
      
    //   // Script has loaded - clean up
    //   return scriptElement;
    // }
      
    /* ---------------- WASM caching pipeline --------------------------- */
  
    // async #fetchAndCompile(url){
    //   const cache    = await caches.open('loader-wasm');
    //   let   response = await cache.match(url);            

    //   /* ---------- fetch + cache on a cold start ---------- */
    //   if (!response) {
    //     response = await fetch(url, { integrity: this.#cfg.integrity });
    //     if (!response.ok) {
    //       throw new Error(`Failed to fetch ${url}: ${response.status} ${response.statusText}`);
    //     }
    //     await cache.put(url, response.clone());           // raw bytes saved for next launch
    //   }

    //   /* ---------- compile (streaming if possible) ---------- */
    //   try {
    //     if (WebAssembly.compileStreaming) {
    //       return await WebAssembly.compileStreaming(response.clone());
    //     }
    //     const buf = await response.arrayBuffer();         // Safari ≤ 16
    //     return await WebAssembly.compile(buf);
    //   } catch (err) {
    //     console.error(`WASM compile failed for ${url}:`, err);
    //     throw new Error(`Failed to compile WebAssembly module from ${url}`);
    //   }
    // }

    /* small utility: inject a <script> and return when onload fires */
    #loadScriptTag(src) {
      return new Promise((resolve, reject) => {
        const s = document.createElement('script');
        s.src = src;
        s.async = true;
        s.onload = resolve;
        s.onerror = reject;
        document.head.appendChild(s);
      });
    }
    /* utility: wait for the classic stub’s runtime to finish initialising */
    #waitForRuntime(Module) {
      return new Promise((res) => {
        if (Module.calledRun || Module._main) {          // already ready
          res();
        } else if (typeof Module.onRuntimeInitialized === 'function') {
          const old = Module.onRuntimeInitialized;
          Module.onRuntimeInitialized = (...args) => {
            old(...args);
            res();
          };
        } else {
          // very old stubs: poll
          const t = setInterval(() => {
            if (Module.calledRun) { clearInterval(t); res(); }
          }, 16);
        }
      });
    }
    /* ---------------- Boot Emscripten runtime ------------------------- */
    async #bootEmscripten(jsUrl, wasmModule){
      console.log('WasmLoader: booting Emscripten runtime', jsUrl, wasmModule);
      if(this.#cfg.qt)
        return this.qtLoad(this.#cfg);
      if (!jsUrl || !wasmModule) {
        throw new Error('JS source or WASM module is missing');
      }
      const envImports = {
        env: {
          memory: wasmModule.memory ?? new WebAssembly.Memory({ initial: 32 }),
          table:  wasmModule.table  ?? new WebAssembly.Table({ initial: 0, element: 'anyfunc' }),
          ...this.#cfg.environment,
        }
      };

      /* -----------------------------------------------------------------
      * STEP 1 — try dynamic import  (MODULARIZE / EXPORT_ES6 builds)
      * ----------------------------------------------------------------- */
      try {
        const m = await import(/* @vite-ignore */ jsUrl);
        if (typeof m.default === 'function') {
          this.#module = await m.default(envImports);   // factory returns promise
          this.#setStatus('Running');
          return;
        }
      } catch (err) {
        // Syntax-error or MIME-error means it wasn’t an ES-module -> fall through
        console.debug('WasmLoader: dynamic import failed, falling back:', err.message);
      }

      /* -----------------------------------------------------------------
      * STEP 2 — FALLBACK inject <script>  (monolithic stub builds)
      * ----------------------------------------------------------------- */
      try {
        await this.#loadScriptTag(jsUrl);
        if (window.Module) {                         // global stub
          // await this.#waitForRuntime(window.Module);
          // this.#module = window.Module;
          this.#setStatus('Running');
          return;
        }
      } catch (err) {
        console.debug('WasmLoader: script-tag path failed, will try standalone:', err.message);
      }

      /* -----------------------------------------------------------------
      * STEP 3 — FALLBACK stand-alone .wasm  (STANDALONE_WASM or Qt helper)
      * ----------------------------------------------------------------- */
      const { instance } = await WebAssembly.instantiateStreaming(fetch(wasmModule.url), envImports);
      this.#module = instance.exports;
      this.#setStatus('Running');
    }
   /**
     * Load and initialize a Qt WASM application
     * @param {Object} config - Configuration object for Qt WASM app
     * @returns {Promise<Object>} The instantiated module
     */
    async qtLoad(config) {
      // Validate config
      if (!config?.qt?.entryFunction || typeof config.qt.entryFunction !== 'function') {
        throw new Error('config.qt.entryFunction is required and must be a function');
      }

      // Apply defaults and prepare config
      config.qt.qtdir ??= 'qt';
      config.qt.preload ??= [];
      
      // Move Qt-specific properties to emscripten-compatible locations
      config.qtContainerElements = config.qt.containerElements;
      config.qtFontDpi = config.qt.fontDpi;
      delete config.qt.containerElements;
      delete config.qt.fontDpi;
      
      // Save original values
      const { noInitialRun = false, arguments: originalArgs } = config;
      config.noInitialRun = true; // Take control of main() execution
      
      // Set up circuit breaker for handling instantiation failures
      let circuitBreakerReject;
      const circuitBreaker = new Promise((_, reject) => { circuitBreakerReject = reject; });
      
      // Configure WebAssembly instantiation if module is provided
      if (config.qt.module) {
        config.instantiateWasm = async (imports, successCallback) => {
          try {
            const module = await config.qt.module;
            successCallback(await WebAssembly.instantiate(module, imports), module);
          } catch (e) {
            circuitBreakerReject(e);
          }
        };
      }
      
      // Fetch and prepare preload files
      const filesToPreload = await Promise.all(
        config.qt.preload.map(async path => {
          const response = await fetch(path);
          if (!response.ok) throw new Error(`Could not fetch preload file: ${path}`);
          return response.json();
        })
      ).then(results => results.flat());
      
      // Set up preRun to handle environment and file preloading
      const qtPreRun = instance => {
        // Verify ENV export if environment variables are used
        if (config.qt.environment && Object.keys(config.qt.environment).length > 0) {
          const envDescriptor = Object.getOwnPropertyDescriptor(instance, 'ENV');
          if (typeof envDescriptor?.value !== 'object') {
            throw new Error(
              'ENV must be exported if environment variables are passed, ' +
              'add it to QT_WASM_EXTRA_EXPORTED_METHODS CMake target property'
            );
          }
          
          // Copy environment variables
          Object.assign(instance.ENV, config.qt.environment);
        }
        
        // Handle file preloading
        if (filesToPreload.length > 0) {
          if (typeof instance.FS !== 'object') {
            throw new Error('FS must be exported if preload is used');
          }
          
          for (const file of filesToPreload) {
            // Create directory structure
            const parts = file.destination.split('/');
            const filename = parts.pop();
            const dir = parts.join('/');
            
            // Ensure directories exist
            let path = '/';
            for (const part of parts.filter(Boolean)) {
              path += part + '/';
              try {
                instance.FS.mkdir(path);
              } catch (error) {
                if (error.errno !== 20) throw error; // EEXIST = 20
              }
            }
            
            // Create the file
            const source = file.source.replace('$QTDIR', config.qt.qtdir);
            instance.FS.createPreloadedFile(dir, filename, source, true, true);
          }
        }
      };
      
      // Add to preRun hooks
      config.preRun = [...(config.preRun || []), qtPreRun];
      
      // Set up event handlers
      config.onRuntimeInitialized = () => {
        config.onRuntimeInitialized?.();
        config.qt.onLoaded?.();
      };
      
      // Configure file locator
      const originalLocateFile = config.locateFile;
      config.locateFile = filename => {
        const locatedFilename = originalLocateFile?.(filename) ?? filename;
        return locatedFilename.startsWith('libQt6') 
          ? `${config.qt.qtdir}/lib/${locatedFilename}` 
          : locatedFilename;
      };
      
      // Set up exit handling
      let onExitCalled = false;
      const handleExit = (details) => {
        if (onExitCalled) return;
        onExitCalled = true;
        config.qt.onExit?.(details);
      };
      
      config.onExit = code => {
        config.onExit?.();
        handleExit({ code, crashed: false });
      };
      
      config.onAbort = text => {
        config.onAbort?.();
        handleExit({ text, crashed: true });
      };
      
      // Initialize and run
      try {
        const instance = await Promise.race([
          circuitBreaker,
          config.qt.entryFunction(config)
        ]);
        
        // Call main if original config didn't disable it
        if (!noInitialRun) instance.callMain(originalArgs);
        
        return instance;
      } catch (e) {
        // Normal exit via app.exec()
        if (e === "unwind") return;
        
        // Handle crash
        handleExit({ text: e.message, crashed: true });
        throw e;
      }
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
  