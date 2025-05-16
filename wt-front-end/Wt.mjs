//import {computePosition} from './popper.min.js';

const graphemes = str => [...str];              // code‑point / grapheme array
const toUnits = (str, cpIndex) => graphemes(str).slice(0, cpIndex).join('').length;
const toPoints = (str, cuIndex) => graphemes(str.slice(0, cuIndex)).length;

export class WtCore {
  constructor(config = {}) {
    this.cfg = config;
    this.buttons = 0;
    this.lastButtonUp = 0;
    this.mouseDragging = 0;
    this.captureElement = null;
    //this.firedTarget = null;
    this.timers = new Map(); // Store timers for cleanup
    this.initBrowserDetection();

    //window.history.scrollRestoration = "auto";
    window.history.scrollRestoration = "manual";
    window.addEventListener("scroll", this.rafDebounce(() => {
      if (history.state) {
        const newState = {...history.state};
        newState.scrollX = window.pageXOffset;
        newState.scrollY = window.pageYOffset;
        history.replaceState(newState, document.title);
      }
    }, 100));
    window.addEventListener('popstate', () => {
      const {scrollX = 0, scrollY = 0 } = history.state || {};
      window.scrollTo(scrollX, scrollY);
    });
  }

  scrollHistory() {
    window.scrollTo(window.history.state?.pageXOffset || 0, window.history.state?.pageYOffset || 0);
  }
  // Utility method to simulate document.ready
  ready(cb) {
    const ready = cb => document.readyState === 'loading' ? document.addEventListener('DOMContentLoaded', cb) : cb();
  }

  isEmptyObject(obj) {
    return Object.keys(obj).length === 0;
  }

  dragged() { //only used in winteractwidget.cpp
    return this.mouseDragging > 2;
  }

  arrayRemove(array, from, to) {
    //return array.splice(from, (to ?? from) - from + 1); //ES11
    const rest = array.slice((to || from) + 1 || array.length);
    array.length = from < 0 ? array.length + from : from;
    return array.push(...rest);
  }

  addAll(array1, array2) {
    array1.push(...array2);
  }

  initBrowserDetection() {
    const agent = navigator.userAgent.toLowerCase();
    this.isAndroid = /safari/.test(agent) && /android/.test(agent);
    this.isWebKit = /applewebkit/.test(agent);
    this.isGecko = /gecko/.test(agent) && !this.isWebKit;
    this.isIOS = /iphone|ipad|ipod/.test(agent);
  }
  // fitToWindow(element, desiredX, desiredY) {
  //   // 1. Reset all four logical insets so previous runs don’t leak
  //   ['insetInlineStart','insetInlineEnd','insetBlockStart','insetBlockEnd']
  //     .forEach(p => element.style[p] = 'auto');
  
  //   // 2. Element & viewport geometry (logical names)
  //   const box       = element.getBoundingClientRect();
  //   const { inlineSize: vpI, blockSize: vpB } =
  //         document.documentElement.getBoundingClientRect();
  
  //   /* ---------- inline axis ---------- */
  //   let start = desiredX;
  //   if (start + box.width > vpI)          // spill right (or left in RTL)
  //     start = Math.max(0, vpI - box.width);
  
  //   /* ---------- block axis ---------- */
  //   let top = desiredY;
  //   if (top + box.height > vpB)           // spill bottom
  //     top = Math.max(0, vpB - box.height);
  
  //   // 3. Commit with logical props
  //   element.style.insetInlineStart = `${start}px`;
  //   element.style.insetBlockStart  = `${top}px`;
  // }
  
  fitToWindow(element, x, y, rightx, bottomy){
    // Reset positioning styles
    element.style.left = element.style.right = element.style.top = element.style.bottom = 'auto';
    
    // Get element dimensions
    const dimensions = {
      width: element.offsetWidth,
      height: element.offsetHeight
    };
    
    // Consider max dimensions for dynamic widgets
    if (!element.classList.contains("Wt-tooltip")) {
      dimensions.width = this.WT.px(element, "maxWidth") || dimensions.width;
      dimensions.height = this.WT.px(element, "maxHeight") || dimensions.height;
    }
    
    // Get parent and viewport information
    const offsetParent = element.offsetParent;
    if (!offsetParent) return;
    
    const parentCoords = this.WT.widgetPageCoordinates(offsetParent);
    const viewport = {
      width: window.innerWidth,
      height: window.innerHeight,
      scrollX: window.scrollX,
      scrollY: window.scrollY
    };
  
    // Determine horizontal positioning
    let hside = 0; // 0 = left, 1 = right
    if (dimensions.width > viewport.width) {
      // Wider than viewport - align with left edge
      x = viewport.scrollX;
    } else if (x + dimensions.width > viewport.scrollX + viewport.width) {
      // Too far right - position left from rightx
      const scrollX = offsetParent === document.body ? window.scrollX : offsetParent.scrollLeft;
      
      rightx = rightx - parentCoords.x + scrollX;
      x = offsetParent.clientWidth - (rightx + this.WT.px(element, "marginRight"));
      hside = 1;
    } else {
      // Fits to right of x - adjust for parent offset
      const scrollX = offsetParent === document.body ? 0 : offsetParent.scrollLeft;
      x = x - parentCoords.x + scrollX - this.WT.px(element, "marginLeft");
    }
  
    // Determine vertical positioning
    let vside = 0; // 0 = top, 1 = bottom
    if (dimensions.height > viewport.height) {
      // Taller than viewport - align with top edge
      y = viewport.scrollY;
    } else if (y + dimensions.height > viewport.scrollY + viewport.height) {
      // Too far below - position above bottomy
      if (bottomy > viewport.scrollY + viewport.height) {
        bottomy = viewport.scrollY + viewport.height;
      }
      
      const scrollY = offsetParent === document.body ? window.scrollY : offsetParent.scrollTop;
      
      bottomy = bottomy - parentCoords.y + scrollY;
      y = offsetParent.clientHeight - 
          (bottomy + this.WT.px(element, "marginBottom") + this.WT.px(element, "borderBottomWidth"));
      vside = 1;
    } else {
      // Fits below y - adjust for parent offset
      const scrollY = offsetParent === document.body ? 0 : offsetParent.scrollTop;
      y = y - parentCoords.y + scrollY - 
      this.WT.px(element, "marginTop") + this.WT.px(element, "borderTopWidth");
    }
  
    // Apply final positioning
    const sides = [['left', 'right'], ['top', 'bottom']];
    element.style[sides[0][hside]] = `${x}px`;
    element.style[sides[1][vside]] = `${y}px`;
  }
  //replace a dom element with a new one or just append its innerHTML
  setHtml(element, html, append = false) {
    if(append)
      element.insertAdjacentHTML('beforeend', html);
    else if (!html.includes('<') && !html.includes('&') && element.childNodes.length === 1 && element.firstChild.nodeType === Node.TEXT_NODE)
        element.firstChild.textContent = html; // Update text node directly (more efficient +30%)
    else {
        //WT.saveReparented(element);
        element.innerHTML = html;
    }
  }
  // If a modern codebase now renders all those overlays directly 
  // to a stable root node (a React/Vue portal, document.body, or .Wt‑domRoot) 
  // and never leaves them inside volatile layout sub‑trees, you can drop both the class and saveReparented().
  saveReparented = el => { //deprecated ?
    const root = document.querySelector('.Wt-domRoot') ?? document.body;
    el.querySelectorAll('.wt-reparented').forEach(node => root.append(node)); // append() moves, no manual remove needed
  };
  remove(id) {
    const e = this.WT.getElement(id);
    if (e) {
      //WT.saveReparented(e);
      e.parentNode.removeChild(e);
    }
  }
  replaceWith(w1Id, w2) {
    this.WT.$(w1Id).replaceWith(w2);

    /* Reapply client-side validation, bootstrap applys validation classes
       also outside the element into its ancestors */
    if (w2.wtValidate && this.WT.validate) {
      setTimeout(function() {
        this.WT.validate(w2);
      }, 0);
    }
  }
  unstub(from, to, methodDisplay = 0){
    const fs = from.style;
    const ts = to.style;
    if (methodDisplay === 1) {
      fs.display && (ts.display = fs.display);
    } else {
      ['position', 'left', 'visibility'].forEach(p => ts[p] = fs[p]);
    }
    ['height', 'width'].forEach(p => fs[p] && (ts[p] = fs[p]));
    ts.boxSizing = fs.boxSizing || getComputedStyle(from).boxSizing;
  };
  navigateInternalPath = (e, path) => {
    const ev = e || window.event;
    if (ev.ctrlKey || ev.metaKey || ev.button > 0) return;   // let “open‑in‑new‑tab” etc. through
    history.pushState({path}, '', path);                     // update URL bar
    ev.preventDefault();                                     // stop full reload
    ev.stopPropagation();                                    // bubble no further
    // TODO: invoke your own router/render logic here
  };
  
  ajaxInternalPaths = (basePath = '/') => {
    const base = new URL(basePath, document.baseURI);        // absolute base once
    document.querySelectorAll('a.Wt-ip').forEach(a => {
      // 1. strip any “wtd” tracking parameter
      const rawHref = (a.getAttribute('href') ?? '')
                        .replace(/([?&])wtd.*$/, '');
      // 2. resolve …/../ and relatives with URL
      const abs  = new URL(rawHref, base);
      let  path  = abs.pathname + abs.search;                // internal path only
      path = path.replace(/^\/?_=/, '/');                    // kill “ugly” /?_=
      // 3. hydrate anchor & hook click
      a.href = abs.href;                                     // normalised <a href="">
      a.addEventListener('click', ev => navigateInternalPath(ev, path), {passive:false});
      a.classList.remove('Wt-ip');
    });
  };
  
  getElement(id) {
    return document.getElementById(id);
  }

  $(id) {
    return this.getElement(id);
  }

  cancelEvent(event, cancelType = 3) {
    if (cancelType & 0x2) event.preventDefault();
    if (cancelType & 0x1) event.stopPropagation();
  }

  filter(edit, tokens) { // vs onbeforeinput="event.data && !/^[0-9]$/.test(event.data) && event.preventDefault()">
    const regex = new RegExp(tokens); // Create RegExp once
    edit.addEventListener('beforeinput', (e) => {
      if (e.data && !regex.test(e.data))
        e.preventDefault();
    });
  }

  /* this block is probably deprecated - we could use simpler widget logic */
  widgetPageCoordinates(obj, reference = document.documentElement) {
    const rect = obj.getBoundingClientRect();
    const refRect = reference.getBoundingClientRect();
    return {
      x: rect.left - refRect.left + window.pageXOffset,
      y: rect.top - refRect.top + window.pageYOffset
    };
  }

  widgetCoordinates(obj, e) {
    const rect = obj.getBoundingClientRect();
    const rel = pageCoordinates(e);
    return {
      x: rel.x - rect.left - window.pageXOffset,
      y: rel.y - rect.top - window.pageYOffset
    };
  }

  pageCoordinates(event) {
    const e = event.touches?.[0] || event.changedTouches?.[0] || event;
    return { x: e.pageX || 0, y: e.pageY || 0 };
  }

  wheelDelta(e){return Math.sign(e.deltaY);} //only used in WGLWidget.js

  // normalizeWheel(e) { //!!!only used in WCarteseianChart
  //   const [L, P] = [40, 800], {deltaX = 0, deltaY = 0, deltaMode = 0} = e;
  //   const x = deltaMode === 1 ? deltaX * L : deltaMode === 2 ? deltaX * P : deltaX, y = deltaMode === 1 ? deltaY * L : deltaMode === 2 ? deltaY * P : deltaY;
  //   return {spinX: x ? Math.sign(x) : 0, spinY: y ? Math.sign(y) : 0, pixelX: x, pixelY: y};
  // }

  debounce(callback, wait = 100){
    let timeout;
    return (...args) => {
      clearTimeout(timeout);
      timeout = setTimeout(() => callback(...args), wait);
    };
  }
  rafDebounce(fn) {
    let rafId = 0;
    return (...args) => {
      if (rafId) cancelAnimationFrame(rafId);
      rafId = requestAnimationFrame(()=>{ rafId = 0; fn(...args); });
    };
  }

  setUnicodeSelectionRange(elem, start, end) {
    const startCU = toUnits(el.value, start);
    const endCU = toUnits(el.value, end);
    el.setSelectionRange(startCU, endCU, dir);      // built‑in API
  }
  getUnicodeSelectionRange(el) {
    return {
      start: toPoints(el.value, el.selectionStart),
      end: toPoints(el.value, el.selectionEnd)
    };
  }
  getSelectionRange(elem) {
    return { start: elem.selectionStart, end: elem.selectionEnd };
  }
  setSelectionRange(elem, start, end) {
    //setSelectionCP(elem, start, end);
    start = Math.max(0, Math.min(start, elem.value.length));
    end = Math.max(start, Math.min(end, elem.value.length));
    elem.focus();
    elem.setSelectionRange(start, end);
  }

  /* style methods */
  css(element) {
    return getComputedStyle(element);
  }

  px(element, prop) {
    return parseFloat(css(element)[prop]) || 0;
  }
  pxSelf  = (el, prop) => px(el, prop); //deprecated 
  pctSelf = (el, prop) => this.px(el, prop); //deprecated 
  styleAttribute = prop => prop; //deprecated 
  inlinePx(element, prop) {
    return parseFloat(element.style[prop]) || 0;
  }
  boxSizing(element) {
    return css(element).boxSizing === 'border-box';
  }
  isHidden(el) {
    return el.offsetParent === null;
  }
  innerWidth(el) {
    return el.clientWidth;
  }
  innerHeight(el) {
    return el.clientHeight;
  }
  hide(o) {
    this.WT.getElement(o).style.display = "none";
  }
  inline(o) {
    this.WT.getElement(o).style.display = "inline";
  }
  block(o) {
    this.WT.getElement(o).style.display = "block";
  }
  show(o, s) {
    this.WT.getElement(o).style.display = s;
  }
  target = e => e?.target || null;

  addCss(selector, style) { //not sure it is correct
    if (!this.styleSheet) {
      const style = document.createElement('style');
      document.head.appendChild(style);
      this.styleSheet = style.sheet;
    }
    this.styleSheet.insertRule(`${selector} { ${style} }`, this.styleSheet.cssRules.length);
  }

  addStyleSheet(u,m){l=document.createElement('link'),l.rel='stylesheet',l.href=u,m&&m!=='all'&&(l.media=m),document.head.append(l)};

  removeStyleSheet(u) {
    document.querySelectorAll(`link[rel=stylesheet][href="${u}"]`).forEach(l => l.remove());
    Array.from(document.styleSheets).forEach(s => {
      try {
        Array.from(s.cssRules || []).forEach((r, i) => {
          if (r.cssText === `@import url("${u}");`) s.deleteRule(i);
        });
      } catch (e) {}
    });
  }

  positionAtWidget(id, atId, orientation, delta = 0) {
    const w = this.WT.getElement(id);
    const atw = this.WT.getElement(atId);
    if (!atw || !w) return;

    const { x: atX, y: atY } = this.WT.widgetPageCoordinates(atw);
    let x, y, rightx, bottomy;

    w.style.position = "absolute";
    if (this.WT.css(w, "display") === "none") w.style.display = "block";

    if (orientation === this.WT.Horizontal) {
      x = atX + atw.offsetWidth;
      y = atY + delta;
      rightx = atX;
      bottomy = atY + atw.offsetHeight - delta;
    } else {
      x = atX;
      y = atY + atw.offsetHeight;
      rightx = atX + atw.offsetWidth;
      bottomy = atY;
    }

    let p = atw.parentNode;
    while (!p.classList.contains("Wt-domRoot")) {
      if (p.wtReparentBarrier) break;
      if (
        this.WT.css(p, "display") !== "inline" &&
        p.clientHeight > 100 &&
        (["scroll", "auto"].includes(getComputedStyle(p).overflowY) && p.scrollHeight > p.clientHeight ||
         ["scroll", "auto"].includes(getComputedStyle(p).overflowX) && p.scrollWidth > p.clientWidth)
      ) break;
      p = p.parentNode;
    }

    const posP = this.WT.css(p, "position");
    if (!["absolute", "relative"].includes(posP)) p.style.position = "relative";

    w.parentNode.removeChild(w);
    p.appendChild(w);
    w.classList.add("wt-reparented");

    this.WT.fitToWindow(w, x, y, rightx, bottomy);
    w.style.visibility = "";
  }
  
  positionXY(id, x, y) {
    const w = this.WT.getElement(id);

    if (!this.WT.isHidden(w)) {
      w.style.display = "block";
      this.WT.fitToWindow(w, x, y);
    }
  }

  toggleClass(el, className, enable) {
    el.classList.toggle(className, enable);
  }
  /* END - style methods */
  /* this block is probably deprecated - we could use simpler widget logic */
  capture(e){}
  releaseCapture(e){}

  startPointerCapture(el, downEvent, onMove, onEnd) {
    const id = downEvent.pointerId;
    el.setPointerCapture(id);                  // native capture
  
    //document.body.classList.add('dragging');   // CSS disables selection
  
    const move = e => {if(e.pointerId === id) onMove(e);};
    const up   = e => {
      if (e.pointerId !== id) return;
      el.releasePointerCapture(id);
      el.removeEventListener('pointermove', move);
      el.removeEventListener('pointerup',   up);
      //document.body.classList.remove('dragging');
      onEnd?.(e);
    };
  
    el.addEventListener('pointermove', move, {passive:false});
    el.addEventListener('pointerup', up, {passive:false});
  }

  getByClass(className, parent = document) {
    return parent.querySelectorAll(`.${className}`);
  }

  // Replacement for enableInternalPaths
  enableInternalPaths(initialPath) {
    window.history.replaceState({ path: initialPath }, "", initialPath);
    this.currentPath = initialPath;

    window.addEventListener("popstate", (event) => {
      const newPath = window.location.pathname;
      this.currentPath = newPath;
      this.update(null, "path", null, true); // Trigger your app's update logic
    });
  }

  navigate(newPath) {
    window.history.pushState({ path: newPath }, "", newPath);
    this.currentPath = newPath;
    this.update(null, "path", null, true); // Trigger your app's update logic
  }

}

export class GlobalEventManager {
  #handlers = new WeakMap();

  constructor() {
    ['keydown', 'keyup'].forEach(event =>
      document.addEventListener(event, e => this.#handleEvent(event, e), { capture: true })
    );
  }

  bind(event, id, handler) {
    const el = document.getElementById(id);
    if (!el) return;
    this.#handlers.set(el, (this.#handlers.get(el) ?? new Map()).set(event, handler));
  }

  #handleEvent(eventType, event) {
    if (!event.target || ['DIV', 'BODY', 'HTML'].includes(event.target.tagName)) {
      for (const el of this.#handlers.keys()) {
        if (document.contains(el)) {
          this.#handlers.get(el)?.get(eventType)?.(event);
        }
      }
    }
  }

  cleanup(id) {
    const el = document.getElementById(id);
    if (el) this.#handlers.delete(el);
  }
}
/* connection.js ----------------------------------------------------------- */
export class Connection {
  #url;                // URL for the connection
  #socket;              // WebSocket or null
  #sse;                 // EventSource or null
  #tries = 0;
  #readyResolve;
  #keepAlive;
  #lastPong;
  #onMsg; // callback for incoming messages
  ready = new Promise(res => { this.#readyResolve = res; });

  constructor(baseUrl, onMsg) {
    this.heartbeat = 30_000; // heartbeat interval in ms
    this.#url = baseUrl;   // store URL
    this.#onMsg = onMsg;        // store callback
    this.#openWebSocket(baseUrl);
    this.crossDomain = baseUrl.includes('://') && new URL(baseUrl).host !== window.location.host;
  }

  /* ---------- public API ----------- */
  async send(data, timeout = 30000, method = 'POST') {
    if (this.#socket?.readyState === 1)
      return this.#socket.send(data); // WebSocket is open

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), timeout);

    try {
      const response = await fetch(this.#url, {
        method,
        credentials: this.crossDomain ? 'include' : 'same-origin',
        headers: { 'Content-Type': 'text/json' },
        signal: controller.signal,
        body: data,
      });
      clearTimeout(timeoutId);

      if (response.ok && response.headers.get('Content-Type')?.startsWith('text/javascript')) {
        this.#onMsg(0, await response.text()); // OK
      } else {
        this.#onMsg(1, null); // Error
      }
    } catch {
      this.#onMsg(1, null); // Error
    }
  }
  send(data)            { this.#socket?.send(data); }
  close()               { this.#socket?.close(); this.#sse?.close(); this.#keepAlive && clearInterval(this.#keepAlive); }

  /* ---------- internals ------------ */
  #openWebSocket(relativeUrl) {
    const baseUrl = window.location.origin;
    console.log('openWebSocket', baseUrl, relativeUrl);
    const url = new URL(relativeUrl, baseUrl);
    url.protocol = url.protocol.replace('http', 'ws');
    url.searchParams.set('request', 'ws');

    try {
      this.#socket = new WebSocket(url);
    } catch (e) {
      console.warn('WS ctor failed:', e);
      return this.#openSSE(baseUrl);
    }

    this.#socket.onopen    = () => { this.#tries = 0; this.#readyResolve(); this.#startHeartbeat(); };
    this.#socket.onmessage = e  => this.#onMsg(0,e.data);
    this.#socket.onerror   = this.#socket.onclose = () => this.#reconnect(baseUrl);
  }
  #startHeartbeat() {
    this.#keepAlive && clearInterval(this.#keepAlive);
    this.#lastPong = Date.now();
    this.#keepAlive = setInterval(() => {
      if (this.#socket?.readyState !== WebSocket.OPEN) {
        clearInterval(this.#keepAlive);
        this.#keepAlive = null;
        return;
      }
      this.#socket.send('ping');
      if (Date.now() - this.#lastPong > this.heartbeat * 1.5) this.#socket.close();
    }, this.heartbeat);
  }

  #openSSE(baseUrl) {
    this.#sse = new EventSource(`${baseUrl}&signal=sse`);
    this.#sse.onopen    = () => { this.#readyResolve(); };
    this.#sse.onmessage = e  => this.#onMsg(0, e.data);
    this.#sse.onerror   = () => this.#reconnect(baseUrl);
  }

  #reconnect(baseUrl) {
    this.#socket?.close(); this.#socket = null;
    this.#sse?.close();    this.#sse    = null;

    const delay = Math.min(2 ** ++this.#tries * 500, 120_000);
    console.log('reconnect in', delay, 'ms');
    setTimeout(() => this.#openWebSocket(baseUrl), delay);
  }
}

export class EventQueue {
  #buf = [];
  #max;
  #delay;
  #timer;
  #conn;
  constructor(conn, {max = 10_000, delay = 40} = {}) {
    this.#conn = conn; this.#max = max; this.#delay = delay;
  }
  push(evt) {
    this.#buf.push(evt);
    if (this.#buf.length > this.#max)
      throw new Error('too many pending events');
    this.#scheduleFlush();
  }
  #scheduleFlush() {
    if (!this.#timer) this.#timer = setTimeout(() => this.flush(), this.#delay);
  }
  async flush() {
    clearTimeout(this.#timer); this.#timer = null;
    if (!this.#buf.length) return;

    const payload = JSON.stringify(this.#buf);
    this.#buf.length = 0;

    console.log('sending', payload.length, 'bytes');
    console.log('sending', payload);

    //await this.#conn.ready;
    this.#conn.send(payload);
  }
  hasUnsent() { return this.#buf.length > 0; }
}

export default class WtApp {        
  #cfg;
  #wevt = new GlobalEventManager();
  #conn;// = new Connection(this.#cfg.sessionUrl, this.#handleResponse);
  #queue;// = new EventQueue(this.#conn);
  #libraryPromises = new Map(); // Tracks loading Promises by path
  #loadingLibraries = new Set(); // Tracks currently loading libraries
  #updatePending = false; // Debounces sendUpdate calls
  #hasQuit = false;
  #downX = 0;
  #downY = 0;
  #quitmsg = null;
  constructor(config = {}) {
    this.id=config.appId||'Wt';
    this.#cfg = {
      deployPath: config.deployPath || '',
      sessionUrl: config.sessionUrl || `/${config.selfUrl}`,
      keepAlive: config.keepAlive || 60,
      maxFormDataSize: config.maxFormDataSize || 1024 * 1024,
      idleTimeout: config.idleTimeout || null,
      indicatorTimeout: config.indicatorTimeout || 500,
      serverPushTimeout: config.serverPushTimeout || 30000,
      wsPath: config.wsPath || '/ws',
      wsId: config.wsId || '',
      no_reload: config.no_reload || false,
      ...config,
    };
    this.#conn = new Connection(this.#cfg.sessionUrl, this.#handleResponse);
    this.#queue = new EventQueue(this.#conn);

    this._p_ = this;
    this.WT = new WtCore(this.config);
    this.activePointers = new Map();
    //this.load();

    window.addEventListener('beforeunload', () => {
      if (this.#hasQuit) return;
      if(this.#quitmsg) {
        alert(this.#quitmsg);
      }
    }, { capture: true, once: true });
    window.addEventListener('unload', () => {
      if (this.#hasQuit) return;
      this.#hasQuit = true;
      this.quit();
    }, { capture: true, once: true });
    this.keepAliveTimer = setInterval(() => this.update(null, 'keepAlive', null, false), this.#cfg.keepAlive * 1000);
    this.#init();
  }

  #rand = () => (Math.random() * 1e6 | 0) + this.#cfg.randomSeed;

  #hasWebGL() {
    const c = document.createElement('canvas');
    return !!(c.getContext('webgl2') || c.getContext('webgl'));
  }

  /**
   * Checks if cookies are enabled in the browser by setting and reading a test cookie.
   * @private
   * @returns {boolean} True if cookies are enabled, false otherwise
   */
  #isCookieEnabled() {
    const testCookie = 'jscookietest=valid';
    document.cookie = `${testCookie}; SameSite=Lax`;
    this.#cfg.no_reload = this.#cfg.no_reload || document.cookie.includes(testCookie);
    document.cookie = `${testCookie}; expires=Thu, 01 Jan 1970 00:00:00 GMT; SameSite=Lax`;
    //return isEnabled;
  }


  async #init() {
    /* 1️⃣ ensure ?wtd=sessionId param is present once */
    const urlObj = new URL(location.href);
    if (!urlObj.searchParams.has('wtd')) {
      urlObj.searchParams.set('wtd', this.#cfg.sessionId);
      history.replaceState(null, '', urlObj);
      return; // browser reloads with param, our job is done
    }
    /*  check cookies */
    this.#isCookieEnabled();



    /* 2️⃣ build loader-script URL with diagnostics */
    const jsUrl = new URL(this.#cfg.selfUrl, location.origin);
    jsUrl.searchParams.set('sid', this.#cfg.scriptId);
    jsUrl.searchParams.set('rand', this.#rand());
    jsUrl.searchParams.set('scrW', screen.width);
    jsUrl.searchParams.set('scrH', screen.height);
    jsUrl.searchParams.set('tz', -new Date().getTimezoneOffset());
    const tzName = Intl?.DateTimeFormat()?.resolvedOptions()?.timeZone;
    if (tzName) jsUrl.searchParams.set('tzS', tzName);
    if (this.#cfg.webGLDetect && this.#hasWebGL()) jsUrl.searchParams.set('webGL', 'true');

    const res = await fetch(jsUrl);
    if (!res.ok) {
      console.error('Failed to load script:', res.statusText);
      //this.sendError({ 'error-description': `Failed to load script: ${res.statusText}` });
      return;
    }
    const scriptText = await res.text(); //mainscript to build app page
    this.#doJavaScript(scriptText);
  }

  async preload(uris, type = 'image') {
    const promises = uris.map(uri => (type === 'image' ? 
      new Promise(r => { const img = new Image(); img.onload = () => r(img); img.onerror = () => r(null); img.src = uri; }) :
      fetch(uri).then(r => r.ok ? r.arrayBuffer() : null).catch(() => null)
    ));
    return (await Promise.allSettled(promises)).map(r => r.value).filter(v => v);
  }

  isSymbolDefined = symbol => !!symbol && !!symbol.split('.').reduce((o, p) => o?.[p], window);

  isScriptLoaded(path) {
    return !!document.querySelector(`script[src="${path}"]`);
  }

  async loadScript(path, symbol, tries = 2) {
    if (symbol && this.isSymbolDefined(symbol)) return;
    if (this.isScriptLoaded(path)) return;

    for (let attempt = 1; attempt <= tries; attempt++) {
      try {
        await new Promise((resolve, reject) => {
          const script = document.createElement('script');
          script.src = path;
          script.async = true;
          script.crossOrigin = 'anonymous'; // Match import() CORS for cache reuse

          script.onload = () => resolve();
          script.onerror = () => reject(new Error(`Failed to load script: ${path}`));

          document.head.appendChild(script);
        });
        return;
      } catch (error) {
        if (attempt === tries) throw error;
        await new Promise(resolve => setTimeout(resolve, 100 * 2 ** (attempt - 1))); // Exponential backoff
      }
    }
  }

  async loadLibrary(path, symbol, tries = 2) {
    // Check for existing load
    if (this.#libraryPromises.has(path)) {
      return this.#libraryPromises.get(path);
    }

    // Check if script is already loaded or symbol is defined
    if (this.isScriptLoaded(path) || (symbol && this.isSymbolDefined(symbol))) {
      //scheduleUpdate();
      return Promise.resolve();
    }

    this.#loadingLibraries.add(path);
    const promise = (async () => {
      try {
        try {
          // Try ES Module; browser caches response in HTTP cache
          await import(path);
        } catch (e) {
          // Fallback to <script> for non-modules, reusing HTTP cache
          if (e instanceof SyntaxError && e.message.includes('Unexpected token')) {
            await this.loadScript(path, symbol, tries);
          } else {
            throw e; // Rethrow network/CORS errors
          }
        }
        this.#loadingLibraries.delete(path);
        //if (this.#loadingLibraries.size === 0) scheduleUpdate();
      } catch (error) {
        this.#loadingLibraries.delete(path);
        //if (this.#loadingLibraries.size === 0) scheduleUpdate();
        const err = { 'error-description': `Fatal error: failed loading ${path}` };
        this.#sendError(err, err['error-description']);
        this.quit();
        throw error;
      }
    })();

    this.#libraryPromises.set(path, promise);
    return promise;
  }

  // #doJavaScript(js){
  //   if (js) new Function(js)(); // vs eval(js); //!!!eval 
  //   this.doAutoJavaScript?.();//this === appInstance && appInstance?._p_?.doAutoJavaScript();
  // }
  //Content-Security-Policy: script-src 'self' blob: https://trusted.cdn.com; object-src 'none'; base-uri 'self'; report-uri /csp-violation-report-endpoint;
  async #doJavaScript(js){
    if(!js) return;
    const blob = new Blob(['export default function(Wtc, Wt){', js, '}'], { type: 'text/javascript' });
    const url = URL.createObjectURL(blob); 
    const module = await import(url).catch(err => {console.error("Import failed:", err)}).then(() => URL.revokeObjectURL(url)); 
    module?.default(this.WTc, this);//this === appInstance && appInstance?._p_?.doAutoJavaScript();
    this.doAutoJavaScript?.();
  }
  
  trackPointer(element) {
    element.addEventListener('pointerdown', e => {
      this.#downX = e.pageX;
      this.#downY = e.pageY;
      this.activePointers.set(e.pointerId, {
        id: e.pointerId,
        type: e.pointerType,
        isPrimary: e.isPrimary,
        pressure: e.pressure || 0,
        position: {
          client: { x: Math.round(e.clientX), y: Math.round(e.clientY) },
          page: { x: Math.round(e.pageX), y: Math.round(e.pageY) },
          screen: { x: Math.round(e.screenX), y: Math.round(e.screenY) }
        },
        target: e.target,
        timestamp: Date.now()
      });
      // Ensure we capture pointer events even if they move outside the element
      if (element.setPointerCapture) {
        element.setPointerCapture(e.pointerId);
      }
    });
    // Update pointer position when it moves
    element.addEventListener('pointermove', e => {
      if (this.activePointers.has(e.pointerId)) {
        const pointer = this.activePointers.get(e.pointerId);
        pointer.position.client = { x: Math.round(e.clientX), y: Math.round(e.clientY) };
        pointer.position.page = { x: Math.round(e.pageX), y: Math.round(e.pageY) };
        pointer.position.screen = { x: Math.round(e.screenX), y: Math.round(e.screenY) };
        pointer.pressure = e.pressure || 0;
        pointer.timestamp = Date.now();
      }
    });
    // Remove pointer when it's lifted or canceled
    const removePointer = e => {
      if (element.releasePointerCapture) {
        try {
          element.releasePointerCapture(e.pointerId);
        } catch (err) {
          // Ignore errors if pointer was already released
        }
      }
      this.activePointers.delete(e.pointerId);
    };
    
    element.addEventListener('pointerup', removePointer);
    element.addEventListener('pointercancel', removePointer);
    element.addEventListener('pointerleave', removePointer);
  }

  setPath(path) {
    history.pushState(null, '', path);
  }
  addTimerEvent({ id, delay, repeat = -1, callback, context = null }) {
    if (!id || typeof delay !== 'number' || delay < 0 || (repeat !== -1 && (typeof repeat !== 'number' || repeat <= 0))) {
      throw new Error('Invalid parameters: id, delay (non-negative number), and repeat (either -1 or positive number) are required');
    }
  
    const element = this.getElement(id);
    const action = callback || (element?.onclick?.bind(element) ?? (() => {}));
  
    this.clearTimer(id);
  
    const handler = () => {
      try {
        action.call(context ?? element ?? this, element);
        if (repeat === -1) this.timers.delete(id);
      } catch (error) {
        console.error(`Error in timer event for ${id}:`, error);
      }
    };
  
    const timerId = repeat === -1 ? setTimeout(handler, delay) : setInterval(handler, repeat);
    this.timers.set(id, { timerId, repeat, handler });
  
    return timerId;
  }
  
  clearTimer(id) {
    const t = this.timers.get(id);
    if (t) {
      (t.repeat === -1 ? clearTimeout : clearInterval)(t.timerId);
      this.timers.delete(id);
    }
  }
  
  clearAllTimers() {
    this.timers.forEach((_, id) => this.clearTimer(id));
  }
  propagateSize = (element, width, height) => {
    width = width === -1 ? element.offsetWidth : width;
    height = height === -1 ? element.offsetHeight : height;  
    if ((element.wtWidth !== width) || (element.wtHeight !== height)) {
      element.wtWidth = width;
      element.wtHeight = height;
      
      // Only send valid dimensions
      if (width >= 0 && height >= 0) {
        this.emit(element, "resized", Math.round(width), Math.round(height));
      }
    }
  };

  update(element, signalName, event, feedback) {
    const eventData = {
      object: element,
      signal: signalName,
      event,
      feedback,
      evAckId: this.ackUpdateId || 0,
    };
    this.#queue.push(this.encodeEvent(eventData)); 
    this.#queue.flush(); 
  }

  emit(obj, config, ...Args) {
    const userEvent = {
      signal: 'user',
      id: obj.id ?? obj,
      name: config.name  ?? config,
      object: config.eventObject ?? null,
      event: config.event        ?? null,
      args: Args.map(a => a?.toDateString?.() ?? a),
      feedback : true,
      evAckId: this.ackUpdateId
    };
    this.#queue.push(this.encodeEvent(userEvent));
    this.#queue.flush();                       // prompt flush (like scheduleUpdate)
  }

encodeEvent(event) {
  // Create structured JSON payload
  const payload = {
    signal: event.signal,
    evAckId: event.evAckId
  };
  
  // Add widget info if present
  if (event.id) {
    payload.widget = {
      id: event.id,
      name: event.name,
      args: event.args || []
    };
  }
  
  // Process form data
  const form = event.object?.closest?.('form');
  if (form) {
    payload.formData = Object.fromEntries([...new FormData(form)].map(
      ([k, v]) => [k, v instanceof File ? { name: v.name, type: v.type } : v]
    ));
  }
  
  // Track active element
  if (document.activeElement?.id) {
    payload.focus = document.activeElement.id;
  }
  
  // Add internal path data
  if (window.location.hash) {
    payload.path = window.location.hash.substring(1);
  }
  
  // If no DOM event, return early
  if (!event.event) {
    event.payload = payload;
    return event;
  }
  
  // Add DOM event data
  const e = event.event;
  const eventData = payload.eventData = {};
  
  // Event metadata
  if (e.type) eventData.type = e.type;
  
  // Find target with ID
  const target = this.findTargetWithId(e.target);
  if (target?.id) eventData.targetId = target.id;
  
  // Only handle pointer events (no fallback for older browsers)
  if (e.pointerId !== undefined) {
    eventData.pointer = {
      id: e.pointerId,
      type: e.pointerType,
      isPrimary: e.isPrimary,
      pressure: e.pressure || 0,
      position: {
        client: { x: Math.round(e.clientX), y: Math.round(e.clientY) },
        page: { x: Math.round(e.pageX), y: Math.round(e.pageY) },
        screen: { x: Math.round(e.screenX), y: Math.round(e.screenY) }
      },
      button: e.button || 0,
      buttons: e.buttons || 0
    };
    
    // Calculate widget-relative coordinates if needed
    if (event.object && event.object.nodeType !== 9) {
      const rect = event.object.getBoundingClientRect();
      eventData.pointer.position.widget = {
        x: Math.round(e.clientX - rect.left),
        y: Math.round(e.clientY - rect.top)
      };
      
      // Add scroll information if available
      if ('scrollLeft' in event.object) {
        eventData.scroll = {
          x: Math.round(event.object.scrollLeft),
          y: Math.round(event.object.scrollTop),
          width: Math.round(event.object.clientWidth),
          height: Math.round(event.object.clientHeight)
        };
      }
    }
    
    // Multi-touch data (similar to TouchList API)
    const allPointers = Array.from(this.activePointers.values());
    
    // All active touches (equivalent to e.touches)
    eventData.touches = allPointers.map(p => ({
      identifier: p.id,
      type: p.type,
      isPrimary: p.isPrimary,
      position: p.position,
      pressure: p.pressure
    }));
    
    // Touches on the target element (equivalent to e.targetTouches)
    if (target) {
      eventData.targetTouches = allPointers
        .filter(p => this.findTargetWithId(p.target) === target)
        .map(p => ({
          identifier: p.id,
          type: p.type,
          isPrimary: p.isPrimary,
          position: p.position,
          pressure: p.pressure
        }));
    }
    
    // Just the current touch (equivalent to e.changedTouches)
    eventData.changedTouches = [{
      identifier: e.pointerId,
      type: e.pointerType,
      isPrimary: e.isPrimary,
      position: {
        client: { x: Math.round(e.clientX), y: Math.round(e.clientY) },
        page: { x: Math.round(e.pageX), y: Math.round(e.pageY) },
        screen: { x: Math.round(e.screenX), y: Math.round(e.screenY) }
      },
      pressure: e.pressure || 0
    }];
    
    // Add information for common multi-touch gestures
    if (eventData.touches.length >= 2) {
      const points = eventData.touches.map(t => t.position.page);
      
      // Calculate pinch/zoom info
      if (points.length >= 2) {
        const dx = points[0].x - points[1].x;
        const dy = points[0].y - points[1].y;
        const distance = Math.sqrt(dx * dx + dy * dy);
        
        eventData.gesture = {
          pointerCount: points.length,
          distance: distance,
          // Can add more gesture data as needed
        };
      }
    }
    
    // Drag information
    eventData.drag = {
      dx: Math.round(e.pageX - this.#downX),
      dy: Math.round(e.pageY - this.#downY)
    };
    
    // Wheel information
    if (e.deltaY !== undefined) {
      eventData.wheel = { 
        deltaY: Math.round(e.deltaY),
        deltaMode: e.deltaMode
      };
    }
  }
  
  // Keyboard information
  if (e.key) {
    eventData.keyboard = {
      key: e.key,
      code: e.code,
      modifiers: {
        alt: e.altKey || false,
        ctrl: e.ctrlKey || false,
        meta: e.metaKey || false,
        shift: e.shiftKey || false
      }
    };
  }
  
  // Store payload
  event.payload = payload;
  return event;
}
/**
 * Gets form element value based on type
 */
getFormElementValue(el) {
  // Custom value encoder
  if (el.wtEncodeValue)
    return el.wtEncodeValue(el);
  
  // Handle different element types
  switch(el.type) {
    case 'select-multiple':
      return [...el.selectedOptions].map(opt => opt.value);
      
    case 'checkbox':
    case 'radio':
      return el.indeterminate || el.style.opacity === '0.5' ? 'indeterminate' : el.checked ? el.value : undefined;
      
    case 'file':
      return undefined;
      
    default:
      // Handle text inputs
      if (el.classList.contains('Wt-edit-emptyText'))
        return '';
      
      // Handle WTextEdit
      el.ed?.save();
      
      const value = el.value;
      
      // Add selection information if focused
      if (document.activeElement === el) {
        return {
          value,
          selection: {
            start: el.selectionStart,
            end: el.selectionEnd
          }
        };
      }
      
      return value;
  }
}

    /**
     * Finds nearest parent with ID
     */
    findTargetWithId(target) {
        while (target && !target.id && target.parentNode) {
            target = target.parentNode;
        }
        return target;
    }

  #handleResponse(status, msg) {
    if (status === 0 && msg)             
      try {
        this.#doJavaScript(msg); //eval(msg); // Simplified for brevity
      } catch (e) {
        const err = {
          exception_code: e.code || "unknown",
          exception_description: e.message || "No description",
          exception_js: msg,
          stack: e.stack || "No stack trace"
        };
        this.#sendError(err, `Wt internal error; code: ${e.code || "unknown"}, description: ${e.message || "No description"}`);
        throw e;
      }
  }

  startIdleTimeout() {
    const reset = () => {
      clearTimeout(this.timers.idle);
      const logout   = () => this.#conn.send('{"signal":"user", "id": "Wt-idleTimeout"}');
      this.timers.idle = setTimeout(() =>
        logout, this.#cfg.idleTimeout
      );
    };
    ['wheel', 'pointerdown', 'keydown', 'visibilitychange'].forEach(e =>
      document.addEventListener(e, reset, { passive: true/* , signal: aborter.signal */ })
    );
    reset();
  }

  #sendError(err, msg) {
    const error = {
      signal: 'error',
      id: 'Wt-error',
      name: 'error',
      args: [err],
      feedback: false,
      evAckId: this.ackUpdateId
    };
    this.#queue.push(this.encodeEvent(error));
    this.#queue.flush(); // Force flush to send error immediately
    //console.error(msg, err);
  }

  quit() {
    Object.values(this.timers).forEach(clearTimeout);
    this.#hasQuit = true;
    this.#conn.cancel();
    const tr = this.WT.$("Wt-timers");
    if (tr) {
      this.WT.setHtml(tr, "", false);
    }
  }

  setTitle(title) {
    document.title = title;
  }
}
