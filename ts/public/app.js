
console.log('test');
/** @type {import('./test.esm.js')} */
const { initHelloWorld } = await import(new URL('./test.esm.js', window.location.href).href);
/** @type {import('../src/js/PopupWindow.esm.js')} */
const { openPopupWindow } = await import(new URL('/js/PopupWindow.esm.js', window.location.href).href);
/** @type {import('../src/js/ToolTip.esm.js')} */
const { Tooltip } = await import(new URL('/js/ToolTip.esm.js', window.location.href).href);   

initHelloWorld();

