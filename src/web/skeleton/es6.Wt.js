class WtCore {
  constructor(config = {}) {
    this.config = config;
    this.buttons = 0;
    this.lastButtonUp = 0;
    this.mouseDragging = 0;
    this.captureElement = null;
    this.firedTarget = null;
    this.timers = new Map(); // Store timers for cleanup
    this.initBrowserDetection();
  }

  // Utility method to simulate document.ready
  ready(cb) {
    const ready = cb => document.readyState === 'loading' ? document.addEventListener('DOMContentLoaded', cb) : cb();
  }

  isEmptyObject(obj) {
    return Object.keys(obj).length === 0;
  }

  button(event) {
    const type = event.type;
    if (!['mouseup', 'mousedown', 'click', 'dblclick'].includes(type)) return 0;
    return { 0: 1, 1: 2, 2: 4 }[event.button] || 0;
  }

  mouseDown(event) {
    this.buttons |= this.button(event);
  }

  mouseUp(event) {
    this.lastButtonUp = this.button(event);
    this.buttons &= ~this.lastButtonUp;
    setTimeout(() => { this.mouseDragging = 0; }, 5);
  }

  dragged() {
    return this.mouseDragging > 2;
  }

  drag() {
    this.mouseDragging += 1;
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

  initAjaxComm(url, handler) {
    const crossDomain = url.includes('://') && new URL(url).host !== window.location.host;

    const createRequest = async (method, url) => {
      const response = await fetch(url, {
        method,
        credentials: crossDomain ? 'include' : 'same-origin',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      });
      return response;
    };

    return {
      async sendUpdate(data, userData, _id, _timeout, isPoll) {
        try {
          const response = await createRequest('POST', url);
          if (response.ok && response.headers.get('Content-Type')?.startsWith('text/javascript')) {
            handler(0, await response.text(), userData); // OK
          } else if (isPoll && response.status === 504) {
            handler(2, null, userData); // Timeout
          } else {
            handler(1, null, userData); // Error
          }
        } catch {
          handler(1, null, userData); // Error
        }
      },
      responseReceived: () => {},
      cancel: () => {url = null;},
      setUrl: (newUrl) => { url = newUrl; },
    };
  }

  setHtml(element, html, append = false) {
    if(append) {
      const parser = new DOMParser();
      const doc = parser.parseFromString(`<div>${html}</div>`, 'application/xhtml+xml');
      const div = doc.documentElement;
      const fragment = document.createDocumentFragment();
      Array.from(div.childNodes).forEach(node => fragment.appendChild(node.cloneNode(true)));
      //if (!append) element.innerHTML = '';
      element.appendChild(fragment);
    }
    else element.innerHTML = html;
  }

  getElement(id) {
    return document.getElementById(id) || Array.from(window.frames)
      .map(frame => frame.document.getElementById(id))
      .find(el => el) || null;
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
    element.addEventListener('beforeinput', (event) => {
      if (event.data && !regex.test(event.data))
        event.preventDefault();
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

  wheelDelta(e){return e.deltaY;}

  normalizeWheel(e) { //!!!only used in WCarteseianChart
    const [L, P] = [40, 800], {deltaX = 0, deltaY = 0, deltaMode = 0} = e;
    const x = deltaMode === 1 ? deltaX * L : deltaMode === 2 ? deltaX * P : deltaX, y = deltaMode === 1 ? deltaY * L : deltaMode === 2 ? deltaY * P : deltaY;
    return {spinX: x ? Math.sign(x) : 0, spinY: y ? Math.sign(y) : 0, pixelX: x, pixelY: y};
  }


  //    window.history.scrollRestoration = "auto";
  window.history.scrollRestoration = "manual";
  scrollHistory() {
    window.scrollTo(window.history.state?.pageXOffset || 0, window.history.state?.pageYOffset || 0);
  }
  debounce(callback, wait = 100){
    let timeout;
    return (...args) => {
      clearTimeout(timeout);
      timeout = setTimeout(() => callback(...args), wait);
    };
  };
  window.addEventListener("scroll", debounce(() => {
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
  setUnicodeSelectionRange(elem, start, end) {
    const value = elem.value;
    let newStart = start, newEnd = end;
    for (let i = 0, count = 0; i < value.length && count < end; i++) {
      if (count < start) newStart++;
      if (count < end) newEnd++;
      count++;
      if (value.codePointAt(i) > 0xFFFF) count++; // Surrogate pair
    }
    setSelectionRange(elem, newStart, newEnd);
  }
  getUnicodeSelectionRange(elem) {
    const value = elem.value;
    let start = elem.selectionStart;
    let end = elem.selectionEnd;
    for (let i = 0; i < value.length && i < end; i++) {
      if (value.codePointAt(i) > 0xFFFF) { // Surrogate pair
        if (i < start) start--;
        if (i < end) end--;
      }
    }
    return { start, end };
  }
  getSelectionRange(elem) {
    return { start: elem.selectionStart, end: elem.selectionEnd };
  }
  setSelectionRange(elem, start, end) {
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

  inlinePx(element, prop) {
    return parseFloat(element.style[prop]) || 0;
  }
  isBorderBox(element) {
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
    WT.getElement(o).style.display = "none";
  };
  inline(o) {
    WT.getElement(o).style.display = "inline";
  };
  block(o) {
    WT.getElement(o).style.display = "block";
  };
  show(o, s) {
    WT.getElement(o).style.display = s;
  };

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
  // addStyleSheet(uri, media = '') {
  //   const link = document.createElement('link');
  //   link.rel = 'stylesheet';
  //   link.href = uri;
  //   if (media && media !== 'all') {
  //     link.media = media;
  //   }
  //   document.head.appendChild(link);
  //   return link;
  // }
  fitToWindow(e, x, y) {
    e.style.position = 'absolute';
    e.style.left = e.style.right = e.style.top = e.style.bottom = '';
    e.style.left = `${x}px`;
    e.style.top = `${y}px`;
    const rect = e.getBoundingClientRect();
    if (rect.right > window.innerWidth) e.style.right = '0px', e.style.left = '';
    if (rect.bottom > window.innerHeight) e.style.bottom = '0px', e.style.top = '';
  }
  positionXY(id, x, y) {
    const w = WT.getElement(id);

    if (!WT.isHidden(w)) {
      w.style.display = "block";
      WT.fitToWindow(w, x, y, x, y);
    }
  };

  toggleClass(element, className, enable) {
    element.classList.toggle(className, enable);
  }
  /* END - style methods */


  /* this block is probably deprecated - we could use simpler widget logic */

  capture(element, handlers = {}) {
    if (!element) return;
    const events = [
      ['pointermove', handlers.move],
      ['pointerup', handlers.up]
      //, ['pointerleave', handlers.leave] // Optional, for mouse-out cases
    ];
    events.forEach(([type, handler]) => {
      if (handler) {
        element.addEventListener(type, handler, { capture: true });
      }
    });
    element.classList.add('unselectable');
  }

  releaseCapture(element) {
    element.classList.remove('unselectable');
  }

  getByClass(className, parent = document) {
    return parent.querySelectorAll(`.${className}`);
  }

  // capture(element) {
  //   this.captureElement = element;
  //   document.body.classList.toggle('unselectable', !!element);
  //   document.body.onselectstart = element ? () => false : null;
  // }

  // target(event) {
  //   return this.firedTarget || event.target || event.srcElement || null;
  // }

  // Improved addTimerEvent
  addTimerEvent({ id, delay, repeat = -1, callback, context = null }) {
    // Validate inputs
    if (!id || typeof delay !== 'number' || delay < 0) {
      throw new Error('Invalid parameters for addTimerEvent: id and delay are required');
    }

    // Retrieve element if id is provided
    const element = id ? this.getElement(id) : null;

    // Use provided callback or default to element's onclick
    const action = callback || (element && element.onclick ? element.onclick.bind(element) : () => {});

    // Clear existing timer for this id, if any
    this.clearTimer(id);

    // Create a timer handler
    const handler = () => {
      try {
        action.call(context || element || this, element);
        if (repeat === -1) {
          this.timers.delete(id); // Clean up one-shot timer
        }
      } catch (error) {
        console.error(`Error in timer event for ${id}:`, error);
      }
    };

    // Schedule the timer
    const timerId = repeat === -1
      ? setTimeout(handler, delay)
      : setInterval(handler, repeat > 0 ? repeat : delay);

    // Store timer metadata
    this.timers.set(id, { timerId, repeat, handler });

    return timerId; // Allow external cleanup if needed
  }

  // Helper to clear a specific timer
  clearTimer(id) {
    const timerData = this.timers.get(id);
    if (timerData) {
      if (timerData.repeat === -1) {
        clearTimeout(timerData.timerId);
      } else {
        clearInterval(timerData.timerId);
      }
      this.timers.delete(id);
    }
  }

  // Clean up all timers (e.g., on app shutdown)
  clearAllTimers() {
    this.timers.forEach((_, id) => this.clearTimer(id));
  }

  // Replacement for enableInternalPaths
  enableInternalPaths(initialPath) {
    // Set the initial path without adding a history entry
    window.history.replaceState({ path: initialPath }, "", initialPath);
    this.currentPath = initialPath;

    // Listen for back/forward navigation
    window.addEventListener("popstate", (event) => {
      const newPath = window.location.pathname;
      this.currentPath = newPath;
      this.update(null, "path", null, true); // Trigger your app's update logic
    });
  }

  // Helper method to navigate programmatically
  navigate(newPath) {
    window.history.pushState({ path: newPath }, "", newPath);
    this.currentPath = newPath;
    this.update(null, "path", null, true); // Trigger your app's update logic
  }

}

class WtApp {
  constructor(config = {}) {
    this.config = {
      deployPath: config.deployPath || '',
      sessionUrl: config.sessionUrl || '',
      keepAlive: config.keepAlive || 60,
      maxFormDataSize: config.maxFormDataSize || 1024 * 1024,
      idleTimeout: config.idleTimeout || null,
      indicatorTimeout: config.indicatorTimeout || 500,
      serverPushTimeout: config.serverPushTimeout || 30000,
      wsPath: config.wsPath || '/ws',
      wsId: config.wsId || '',
      ...config,
    };
    this.wt = new WtCore(this.config);
    this.comm = this.wt.initAjaxComm(this.config.sessionUrl, this.handleResponse.bind(this));
    this.pendingEvents = [];
    this.sentEvents = [];
    this.hasQuit = false;
    this.load();
  }

  // trackPointer(element, onMove) {
  //   let startPos;
  //   element.addEventListener('pointerdown', e => {
  //     startPos = { x: e.pageX, y: e.pageY };
  //   });
  //   element.addEventListener('pointermove', e => {
  //     if (startPos) onMove({ x: e.pageX - startPos.x, y: e.pageY - startPos.y });
  //   });
  // }

  setPath(path) {
    history.pushState(null, '', path);
  }

  load() {
    document.addEventListener('mousedown', this.wt.mouseDown.bind(this.wt));
    document.addEventListener('mouseup', this.wt.mouseUp.bind(this.wt));
    this.keepAliveTimer = setInterval(() => this.update(null, 'keepAlive', null, false), this.config.keepAlive * 1000);
  }

  update(element, signalName, event, feedback) {
    const eventData = {
      object: element,
      signal: signalName,
      event,
      feedback,
      evAckId: this.ackUpdateId || 0,
    };
    this.pendingEvents.push(this.encodeEvent(eventData));
    this.scheduleUpdate();
  }

  encodeEvent(event) {
    // const mods = ['altKey', 'ctrlKey', 'metaKey', 'shiftKey']
    //   .filter(k => event[k])
    //   .map(k => `${k}=1`);
    const result = [`signal=${event.signal}`];
    if (event.object?.id) result.push(`id=${event.object.id}`);
    return { data: result, feedback: event.feedback, evAckId: event.evAckId };
  }

  scheduleUpdate() {
    if (this.hasQuit || this.responsePending) return;
    clearTimeout(this.updateTimeout);
    this.updateTimeout = setTimeout(() => this.sendUpdate(), 51);
  }

  async sendUpdate() {
    if (this.pendingEvents.length === 0) return;
    const { result } = this.encodePendingEvents(this.config.maxFormDataSize);
    this.responsePending = true;
    await this.comm.sendUpdate(`request=jsupdate${result}`, null, this.ackUpdateId, -1, false);
    this.responsePending = false;
  }

  encodePendingEvents(maxLength) {
    let result = '';
    let feedback = false;
    let i = 0;
    for (; i < this.pendingEvents.length; i++) {
      const eventData = this.pendingEvents[i].data.join('&');
      if (result.length + eventData.length < maxLength) {
        result += (i > 0 ? '&e' + i : '&') + eventData;
        feedback = feedback || this.pendingEvents[i].feedback;
      } else break;
    }
    this.sentEvents = this.sentEvents.concat(this.pendingEvents.slice(0, i));
    this.pendingEvents = this.pendingEvents.slice(i);
    return { feedback, result };
  }

  handleResponse(status, msg) {
    if (status === 0 && msg) eval(msg); // Simplified for brevity
    this.sentEvents = [];
    this.responsePending = false;
    if (this.pendingEvents.length > 0) this.sendUpdate();
  }

  emit(object, config, ...args) {
    this.update(object, 'user', null, true);
  }

  /*  */
  startKeepAlive() {
    this.timers.keepAlive = setInterval(() =>
      fetch('/api/ping'), this.config.keepAliveInterval //update(null, "keepAlive", null, false);
    );
  }

  startIdleTimeout() {
    const reset = () => {
      clearTimeout(this.timers.idle);
      this.timers.idle = setTimeout(() =>
        fetch('/api/logout'), this.config.idleTimeout
      );
    };
    ['wheel', 'pointerdown', 'keydown'].forEach(e =>
      document.addEventListener(e, reset, { passive: true })
    );
    reset();
  }

  quit() {
    Object.values(this.timers).forEach(clearTimeout);
    fetch('/api/logout');
  }

  setTitle(title) {
    document.title = title;
  }

}

// Usage
const config = {
  deployPath: '/app',
  sessionUrl: '/session',
  keepAlive: 60,
  maxFormDataSize: 1024 * 1024,
  idleTimeout: 3600,
  indicatorTimeout: 500,
  serverPushTimeout: 30000,
  wsPath: '/ws',
  wsId: 'unique-id',
  innerHtml: true,
};
const app = new WtApp(config);
window.Wt = app;
