
console.log('test');
/** @type {import('./test.esm.js')} */
const { initHelloWorld } = await import(new URL('./test.esm.js', window.location.href).href);
/** @type {import('../src/js/PopupWindow.esm.js')} */
const { openPopupWindow } = await import(new URL('/js/PopupWindow.esm.js', window.location.href).href);
/** @type {import('../src/js/ToolTip.esm.js')} */
const { Tooltip } = await import(new URL('/js/ToolTip.esm.js', window.location.href).href);   
/** @type {import('../src/js/WasmLoader.esm.js')} */
const WasmLoader  = (await import(new URL('/js/WasmLoader.esm.js', window.location.href).href)).default;   


// Wait for DOM to be ready
//document.addEventListener('DOMContentLoaded', async () => {
  console.log('WASM TEST DOM fully loaded and parsed');
  // Set up logging
  const logElement = document.getElementById('log') || createLogElement();
  
  const log = (message, className) => {
    const line = document.createElement('div');
    line.textContent = message;
    if (className) line.className = className;
    logElement.appendChild(line);
    console.log(message);
  };
  
  log('Initializing WebAssembly loader...');
  
  // Configure the loader
  // Note: 'path' should be a directory, not a file
  const loader = new WasmLoader({
    applicationName: 'wasm-helloworld', // This will look for 'helloworld.wasm'
    path: './',                    // Directory where the WASM file is located
    restartMode: 'DoNotRestart',
    stdoutEnabled: true,           // Show stdout in console
    environment: {                 // Optional environment variables
      TOTAL_MEMORY: 16777216      // 16MB
    }
  });
  
  try {
    log('Loading WebAssembly module...');
    const startTime = performance.now();
    
    await loader.load();
    
    const duration = performance.now() - startTime;
    log(`Module loaded in ${duration.toFixed(2)}ms`, 'success');
    
    // Access the Emscripten module
    if (loader.module) {
      log('Module is ready, you can call exported functions now');
      
      // Example of calling a function (if it exists)
      if (typeof loader.module._hello === 'function') {
        loader.module._hello();
      }
    }
  } catch (error) {
    log(`Error loading WebAssembly: ${error.message}`, 'error');
    console.error(error);
  }
  
  // Helper to create a log element if it doesn't exist
  function createLogElement() {
    const el = document.createElement('div');
    el.id = 'log';
    el.style.cssText = 'font-family:monospace;white-space:pre-wrap;padding:10px;background:#f5f5f5;border:1px solid #ddd;margin:10px 0;max-height:300px;overflow:auto;';
    document.body.appendChild(el);
    return el;
  }
//});
