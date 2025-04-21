

window.onresize = function() {};


/* export function createScript(url, onError) {
  var script = document.createElement('script');
  script.src = url;
  script.async = true;
  script.onerror = onError || function() { console.error('Failed to load ' + url); };
  document.head.appendChild(script);
} */

async function loadScripts(url) {

  await import(url).catch((error) => { //import './es6.Wt.js';
    console.error('Error loading script:', error);
  });
  /*
// Wt.js
window.WtReady = new Promise(resolve => {
  window.Wt = { init: () => console.log('Wt initialized') };
  resolve();
});

// main.js
(async () => {
  await window.WtReady;
  console.log('main.js running, Wt class available:', window.Wt);
})();
*/
/*   createScript('/Wt.js?v=_$_version_$_');
  createScript(url);  */
}

// Boot process in an IIFE
(() => {
  const win = window;
  const doc = document;

  // Configuration variables
  const SCRIPT_ID = '_$_SCRIPT_ID_$_';
  const SELF_URL = '_$_SELF_URL_$_';
  const INTERNAL_PATH = '_$_INTERNAL_PATH_$_';
  const PATH_INFO = '_$_PATH_INFO_$_';
  const AJAX_CANONICAL_URL = '_$_AJAX_CANONICAL_URL_$_';
  let no_replace = _$_RELOAD_IS_NEWSESSION_$_;
  // const cookies = _$_COOKIE_CHECKS_$_;
  // const USE_COOKIES = _$_USE_COOKIES_$_;
   const params = new URLSearchParams(window.location.search);

  // Generate a random number
  const rand = () => Math.round(Math.random() * 1000000) + _$_RANDOMSEED_$_;

   // Returns an array of [key, value] pairs for all query params
   // const getParams = () => [...new URLSearchParams(window.location.search).entries()];

   // // Returns the value of a specific query param or null if it doesn't exist
   // const getParameter = (name) => new URLSearchParams(window.location.search).get(name);

   // Updates/sets a param and returns a new query string with the hash appended
   const createUrl = (name, value) => {
     params.set(name, value);
     return `?${params}${window.location.hash}`;
   };

  // Gather client info (timezone, screen size)
  const tzOffset = (new Date()).getTimezoneOffset();
  let clientInfo = `&tz=${-tzOffset}&scrW=${screen.width}&scrH=${screen.height}`;

  if (Intl?.DateTimeFormat?.().resolvedOptions().timeZone) {
    clientInfo += `&tzS=${encodeURIComponent(Intl.DateTimeFormat().resolvedOptions().timeZone)}`;
  }

  // Calculate deployment path
  let deployPath = decodeURIComponent(win.location.pathname);
  if (PATH_INFO.length > 0) {
    const pathIndex = deployPath.lastIndexOf(PATH_INFO);
    if (pathIndex !== -1) {
      deployPath = deployPath.substring(0, pathIndex) + deployPath.substring(pathIndex + PATH_INFO.length);
    }
  }
  const deployPathInfo = `&deployPath=${encodeURIComponent(deployPath)}`;

   const inOneSecond = new Date();
   inOneSecond.setTime(inOneSecond.getTime() + 1000);

   if(_$_COOKIE_CHECKS_$_){
       // client-side cookie support
       const testcookie = "jscookietest=valid;SameSite=Lax";
       doc.cookie = testcookie;
       no_replace = no_replace || (_$_USE_COOKIES_$_ && doc.cookie.indexOf(testcookie) !== -1);
       doc.cookie = testcookie + ";expires=Thu, 01 Jan 1970 00:00:00 GMT;SameSite=Lax";

       // server-side cookie support
       doc.cookie = "WtTestCookie=ok;path=/;expires=" + inOneSecond.toGMTString() + ";SameSite=Lax";
   }
   // webgl-check
   if (_$_WEBGL_DETECT_$_&&window.WebGLRenderingContext) {
     otherInfo += (()=>{const c = doc.createElement("canvas"); try { return !!(c.getContext("webgl2") || c.getContext("webgl")); } catch(e) { return false; } })() ? "&webGL=true" : "&webGL=false";
   }

   /*
    * Java's weird session encoding could put the path not in the end, e.g.
    * /hello/internalpath;jsessionid=xyz
    */
   let hash = decodeURIComponent(window.location.hash.slice(1).split('?')[0] || ''); //probablement faux

  // Construct script URL with AJAX and HTML5 history support
  let hashInfo = '';
  if (hash.length > 1 && hash.startsWith('/')) {
    hashInfo = `&_=${encodeURIComponent(hash)}`;
  }

   if(!no_replace && params.get("wtd") !== "_$_SESSION_ID_$_")
     setUrl(createUrl("wtd", "_$_SESSION_ID_$_"));
   else {
     let canonicalUrl = _$_AJAX_CANONICAL_URL_$_;
     let hashInfo = "";
     if (hash.length > 1 && hash.charAt(0) === "/") {
       hashInfo = "&_=" + encodeURIComponent(hash);
     }

       const allInfo = hashInfo + otherInfo + htmlHistoryInfo + deployPathInfo;
       loadScript(selfUrl + allInfo + "&request=script&rand=" + rand());
   }

  const scriptUrl = `${SELF_URL}&sid=${SCRIPT_ID}${hashInfo}${clientInfo}&htmlHistory=true${deployPathInfo}&request=script&rand=${rand()}`;

  // Load the script
  loadScripts(scriptUrl);
})();
