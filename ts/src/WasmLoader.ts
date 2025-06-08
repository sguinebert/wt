/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║  Copyright (c) 2025 Sylvain Guinebert - Paris, France         ║
 * ║  MIT License                                                  ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *//* -------------------------------------------------------------------------
 * WasmLoader – modern ES2022+ rewrite with progressive‑enhancement, offline
 * caching, and WebAssembly.instantiateStreaming()
 * -------------------------------------------------------------------------
 * Usage:
 *   import WasmLoader from './WasmLoader';
 *   const loader = new WasmLoader({
 *     applicationName: 'myApp',
 *     containerElements: [document.getElementById('app')],
 *     path: '/wasm/',
 *     restartMode: 'RestartOnCrash',
 *   });
 *   loader.load();
 * ------------------------------------------------------------------------- */

// Type definitions
interface ServiceWorkerRegistration {
  scope: string;
  unregister(): Promise<boolean>;
}

interface QtConfig {
  entryFunction: Function;
  qtdir?: string;
  preload?: string[];
  containerElements?: HTMLElement[];
  fontDpi?: number;
  module?: Promise<WebAssembly.Module>;
  environment?: Record<string, string>;
  onLoaded?: () => void;
  onExit?: (details: {code?: number, text?: string, crashed: boolean}) => void;
}

interface WasmLoaderConfig {
  applicationName: string;
  path: string;
  restartMode?: 'DoNotRestart' | 'RestartOnExit' | 'RestartOnCrash';
  restartLimit?: number;
  stdoutEnabled?: boolean;
  stderrEnabled?: boolean;
  environment?: Record<string, any>;
  containerElements?: HTMLElement[];
  qt?: QtConfig;
  initialMemory?: number;
  maximumMemory?: number;
  initialTable?: number;
  integrity?: string;
  onAbort?: (message: any) => void;
  onQuit?: (code: number, exception: any) => void;
  statusChanged?: (status: string) => void;
}

interface WasmModule {
  url: string;
  memory: WebAssembly.Memory;
  table: WebAssembly.Table;
}

type WasmLoaderStatus = 'NotStarted' | 'Loading' | 'Running' | 'Error' | 'Exited';

async function unregisterServiceWorkers(): Promise<boolean> {
  if (!navigator.serviceWorker) return false;
  
  try {
    // Get all registered service workers
    const registrations = await navigator.serviceWorker.getRegistrations();
    
    // Unregister each one
    await Promise.all(
      registrations.map((registration: ServiceWorkerRegistration) => {
        console.log('[SW] Unregistering:', registration.scope);
        return registration.unregister();
      })
    );
    
    console.log('[SW] All service workers unregistered');
    (window as any).swReg = null;
    return true;
  } catch (err) {
    console.error('[SW] Error unregistering service workers:', err);
    return false;
  }
}
//await unregisterServiceWorkers();

if(navigator.serviceWorker && (!(window as any).swReg || !(window as any).swReg.installing)) {
  /* -----------------------------------------------------------
  * Register the service worker
  * This will cache the assets listed in the service worker
  * --------------------------------------------------------- */
    //window.swReg = await navigator.serviceWorker.register('./js/wasm-sw.js', { scope: '/js/' });
    (window as any).swReg = await navigator.serviceWorker.register('./wasm-sw.js', { scope: '/' });
    /*  Now you can postMessage to register url to catch and cache, 
* swReg.active.postMessage({ type: 'CACHE_ASSETS', urls: ['/module.wasm'] }); */
// -------------------------------------------------------------------------
}



export default class WasmLoader {
  /* --------------------------------------------------- private fields */
  #sw: boolean;                // service worker active?
  #cfg: WasmLoaderConfig;      // normalised user config
  #status: WasmLoaderStatus = 'NotStarted'; // current status of the loader
  #restartCount: number = 0;
  #module: any = null;         // Emscripten module instance once running
  #wasmCache: any = null;      // IndexedDB helper (lazy)
  
  // Public properties
  jsUrl: string;
  wasmUrl: string;

  /* -------------------------------------------------------------------
   * Constructor / private state ( # fields )
   * ----------------------------------------------------------------- */
  /** @param {WasmLoaderConfig} cfg */
  constructor(cfg: Partial<WasmLoaderConfig> = {}) {
    this.#cfg = {
      path: '',
      restartMode: 'DoNotRestart',     // DoNotRestart | RestartOnExit | RestartOnCrash
      restartLimit: 5,
      stdoutEnabled: true,
      stderrEnabled: true,
      environment: {},
      ...cfg,
    } as WasmLoaderConfig;
    
    this.#sw = !!navigator.serviceWorker && !!(window as any).swReg && !!(window as any).swReg.active;
    
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
      (window as any).swReg.active.postMessage({
        type: 'CACHE_ASSETS',
        urls: [
                this.wasmUrl,
                this.jsUrl
              ],
      });
    }
  }

  /* -------------------------------------------------------------------
   * Public API
   * ----------------------------------------------------------------- */
  get module(): any { return this.#module; }
  get status(): WasmLoaderStatus { return this.#status; }
  get wasmSupported(): boolean { return typeof WebAssembly !== 'undefined'; }
  get webglSupported(): boolean {
    try {
      const c = document.createElement('canvas');
      return !!window.WebGLRenderingContext && !!(c.getContext('webgl') || c.getContext('experimental-webgl'));
    } catch { return false; }
  }

  /** Begin loading Wasm (JS runtime + wasm) */
  async load(): Promise<void> {
    if (!this.wasmSupported) throw new Error('WebAssembly not supported');
    if (!this.webglSupported) throw new Error('WebGL not supported');

    this.#setStatus('Loading');

    const base = this.#cfg.path.endsWith('/') ? this.#cfg.path : `${this.#cfg.path}/`;
    const app  = this.#cfg.applicationName;

    try {
      const wasmModule: WasmModule = {
        url: this.wasmUrl,
        memory: new WebAssembly.Memory({ 
          initial: this.#cfg.initialMemory || 32,
          ...(this.#cfg.maximumMemory !== undefined ? { maximum: this.#cfg.maximumMemory } : {})
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

  /* small utility: inject a <script> and return when onload fires */
  #loadScriptTag(src: string): Promise<void> {
    return new Promise((resolve, reject) => {
      const s = document.createElement('script');
      s.src = src;
      s.async = true;
      s.onload = () => resolve();
      s.onerror = reject;
      document.head.appendChild(s);
    });
  }
  
  /* utility: wait for the classic stub's runtime to finish initialising */
  #waitForRuntime(Module: any): Promise<void> {
    return new Promise((res) => {
      if (Module.calledRun || Module._main) {          // already ready
        res();
      } else if (typeof Module.onRuntimeInitialized === 'function') {
        const old = Module.onRuntimeInitialized;
        Module.onRuntimeInitialized = (...args: any[]) => {
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
  async #bootEmscripten(jsUrl: string, wasmModule: WasmModule): Promise<void> {
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
      // Syntax-error or MIME-error means it wasn't an ES-module -> fall through
      console.debug('WasmLoader: dynamic import failed, falling back:', (err as Error).message);
    }

    /* -----------------------------------------------------------------
    * STEP 2 — FALLBACK inject <script>  (monolithic stub builds)
    * ----------------------------------------------------------------- */
    try {
      await this.#loadScriptTag(jsUrl);
      if ((window as any).Module) {                         // global stub
        // await this.#waitForRuntime(window.Module);
        // this.#module = window.Module;
        this.#setStatus('Running');
        return;
      }
    } catch (err) {
      console.debug('WasmLoader: script-tag path failed, will try standalone:', (err as Error).message);
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
   * @param {WasmLoaderConfig} config - Configuration object for Qt WASM app
   * @returns {Promise<any>} The instantiated module
   */
  async qtLoad(config: WasmLoaderConfig): Promise<any> {
    // Validate config
    if (!config?.qt?.entryFunction || typeof config.qt.entryFunction !== 'function') {
      throw new Error('config.qt.entryFunction is required and must be a function');
    }

    // Apply defaults and prepare config
    if (!config.qt.qtdir) config.qt.qtdir = 'qt';
    if (!config.qt.preload) config.qt.preload = [];
    
    // Move Qt-specific properties to emscripten-compatible locations
    const qtConfig: any = config;
    qtConfig.qtContainerElements = config.qt.containerElements;
    qtConfig.qtFontDpi = config.qt.fontDpi;
    delete config.qt.containerElements;
    delete config.qt.fontDpi;
    
    // Save original values
    const noInitialRun = (qtConfig.noInitialRun as boolean) ?? false;
    const originalArgs = qtConfig.arguments;
    qtConfig.noInitialRun = true; // Take control of main() execution
    
    // Set up circuit breaker for handling instantiation failures
    let circuitBreakerReject: (reason?: any) => void;
    const circuitBreaker = new Promise<never>((_, reject) => { circuitBreakerReject = reject; });
    
    // Configure WebAssembly instantiation if module is provided
    if (config.qt.module) {
      qtConfig.instantiateWasm = async (imports: WebAssembly.Imports, successCallback: Function) => {
        try {
          const module = await config.qt?.module;
          successCallback(await WebAssembly.instantiate(module as WebAssembly.Module, imports), module);
        } catch (e) {
          circuitBreakerReject(e);
        }
      };
    }
    
    // Fetch and prepare preload files
    interface PreloadFile {
      source: string;
      destination: string;
    }

    const filesToPreload: PreloadFile[] = await Promise.all(
      (config.qt.preload || []).map(async (path: string) => {
        const response = await fetch(path);
        if (!response.ok) throw new Error(`Could not fetch preload file: ${path}`);
        return response.json();
      })
    ).then(results => results.flat());
    
    // Set up preRun to handle environment and file preloading
    const qtPreRun = (instance: any) => {
      // Verify ENV export if environment variables are used
      if (config.qt?.environment && Object.keys(config.qt.environment).length > 0) {
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
          const filename = parts.pop() as string;
          const dir = parts.join('/');
          
          // Ensure directories exist
          let path = '/';
          for (const part of parts.filter(Boolean)) {
            path += part + '/';
            try {
              instance.FS.mkdir(path);
            } catch (error: any) {
              if (error.errno !== 20) throw error; // EEXIST = 20
            }
          }
          
          // Create the file
          const source = file.source.replace('$QTDIR', config.qt?.qtdir || 'qt');
          instance.FS.createPreloadedFile(dir, filename, source, true, true);
        }
      }
    };
    
    // Add to preRun hooks
    qtConfig.preRun = [...(qtConfig.preRun || []), qtPreRun];
    
    // Set up event handlers
    qtConfig.onRuntimeInitialized = () => {
      if (qtConfig.onRuntimeInitialized) qtConfig.onRuntimeInitialized();
      if (config.qt?.onLoaded) config.qt.onLoaded();
    };
    
    // Configure file locator
    const originalLocateFile = qtConfig.locateFile;
    qtConfig.locateFile = (filename: string) => {
      const locatedFilename = originalLocateFile ? originalLocateFile(filename) : filename;
      return locatedFilename.startsWith('libQt6') 
        ? `${config.qt?.qtdir}/lib/${locatedFilename}` 
        : locatedFilename;
    };
    
    // Set up exit handling
    let onExitCalled = false;
    const handleExit = (details: {code?: number, text?: string, crashed: boolean}) => {
      if (onExitCalled) return;
      onExitCalled = true;
      if (config.qt?.onExit) config.qt.onExit(details);
    };
    
    qtConfig.onExit = (code: number) => {
      if (qtConfig.onExit) qtConfig.onExit();
      handleExit({ code, crashed: false });
    };
    
    qtConfig.onAbort = (text: string) => {
      if (qtConfig.onAbort) qtConfig.onAbort();
      handleExit({ text, crashed: true });
    };
    
    // Initialize and run
    try {
      const instance = await Promise.race([
        circuitBreaker,
        config.qt.entryFunction(qtConfig)
      ]);
      
      // Call main if original config didn't disable it
      if (!noInitialRun) instance.callMain(originalArgs);
      
      return instance;
    } catch (e) {
      // Normal exit via app.exec()
      if (e === "unwind") return;
      
      // Handle crash
      handleExit({ text: (e as Error).message, crashed: true });
      throw e;
    }
  }

  /* ---------------- Error / exit handling -------------------------- */
  #handleAbort(msg: any): void {
    console.error('WasmLoader abort:', msg);
    this.#setStatus('Error');
    if (this.#cfg.onAbort) this.#cfg.onAbort(msg);
  }

  #handleQuit(code: number, exc: any): void {
    if(code === 0) return; // clean exit
    console.warn('WasmLoader quit:', code, exc);
    this.#setStatus('Exited');
    if (this.#cfg.onQuit) this.#cfg.onQuit(code, exc);
    if(this.#cfg.restartMode === 'RestartOnCrash') this.#tryRestart();
  }

  #tryRestart(): void {
    if(++this.#restartCount > (this.#cfg.restartLimit || 5)) return;
    this.load().catch(e => console.error('Restart failed:', e));
  }

  /* ---------------- Status ----------------------------------------- */
  #setStatus(s: WasmLoaderStatus): void {
    if(this.#status === s) return;
    this.#status = s;
    if (this.#cfg.statusChanged) this.#cfg.statusChanged(s);
  }
}
