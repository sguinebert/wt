//import { computePosition, offset, flip, shift } from './vendor/floating-ui/floating-ui.core.browser.min.mjs';

// Types and interfaces
interface WtCoreConfig {
  [key: string]: any;
}

interface Dimensions {
  width: number;
  height: number;
}

interface ViewportInfo {
  width: number;
  height: number;
  scrollX: number;
  scrollY: number;
}

interface Coordinates {
  x: number;
  y: number;
}

interface PageCoordinates extends Coordinates {}

interface WidgetCoordinates extends Coordinates {}

interface PointerInfo {
  id: number;
  type: string;
  isPrimary: boolean;
  pressure: number;
  position: {
    client: Coordinates;
    page: Coordinates;
    screen: Coordinates;
    widget?: Coordinates;
  };
  target: EventTarget | null;
  timestamp: number;
}

interface TimerInfo {
  timerId: number;
  repeat: number;
  handler: () => void;
}

interface EventHandlerMap {
  [eventType: string]: (event: Event) => void;
}

interface ConnectionConfig {
  heartbeat?: number;
}

interface EventQueueOptions {
  max?: number;
  delay?: number;
}

interface AppConfig {
  appId?: string;
  deployPath?: string;
  sessionUrl?: string;
  selfUrl?: string;
  keepAlive?: number;
  maxFormDataSize?: number;
  idleTimeout?: number | null;
  indicatorTimeout?: number;
  serverPushTimeout?: number;
  wsPath?: string;
  wsId?: string;
  no_reload?: boolean;
  randomSeed?: number;
  webGLDetect?: boolean;
  sessionId?: string;
  scriptId?: string;
  [key: string]: any;
}

interface EventData {
  object: HTMLElement | string | null;
  signal: string;
  event: Event | null;
  feedback: boolean;
  evAckId: number;
  id?: string;
  name?: string;
  args?: any[];
  payload?: any;
}

interface UserEvent {
  signal: string;
  id: string;
  name: string | { name: string; eventObject: any; event: Event | null };
  object: any;
  event: Event | null;
  args: any[];
  feedback: boolean;
  evAckId: number;
  payload?: any;
}

interface TimerEventOptions {
  id: string;
  delay: number;
  repeat?: number;
  callback?: (element?: HTMLElement) => void;
  context?: any;
}

const graphemes = (str: string): string[] => [...str];              // code‑point / grapheme array
const toUnits = (str: string, cpIndex: number): number => graphemes(str).slice(0, cpIndex).join('').length;
const toPoints = (str: string, cuIndex: number): number => graphemes(str.slice(0, cuIndex)).length;

export class WtCore {
  cfg: WtCoreConfig;
  buttons: number;
  lastButtonUp: number;
  mouseDragging: number;
  captureElement: HTMLElement | null;
  timers: Map<string, any>;
  isAndroid: boolean = false;
  isWebKit: boolean = false;
  isGecko: boolean = false;
  isIOS: boolean = false;
  styleSheet: CSSStyleSheet | null = null;
  currentPath: string = '';
  Horizontal: string = 'horizontal'; // Constant used in positionAtWidget

  constructor(config: WtCoreConfig = {}) {
    this.cfg = config;
    this.buttons = 0;
    this.lastButtonUp = 0;
    this.mouseDragging = 0;
    this.captureElement = null;
    this.timers = new Map<string, any>(); // Store timers for cleanup
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

  scrollHistory(): void {
    window.scrollTo(window.history.state?.pageXOffset || 0, window.history.state?.pageYOffset || 0);
  }
  
  // Utility method to simulate document.ready
  ready(cb: () => void): void {
    const ready = (callback: () => void) => 
      document.readyState === 'loading' 
        ? document.addEventListener('DOMContentLoaded', () => callback()) 
        : callback();
    ready(cb);
  }

  isEmptyObject(obj: object): boolean {
    return Object.keys(obj).length === 0;
  }

  dragged(): boolean { //only used in winteractwidget.cpp
    return this.mouseDragging > 2;
  }

  arrayRemove(array: any[], from: number, to?: number): number {
    //return array.splice(from, (to ?? from) - from + 1); //ES11
    const rest = array.slice((to || from) + 1 || array.length);
    array.length = from < 0 ? array.length + from : from;
    return array.push(...rest);
  }

  addAll<T>(array1: T[], array2: T[]): void {
    array1.push(...array2);
  }

  initBrowserDetection(): void {
    const agent = navigator.userAgent.toLowerCase();
    this.isAndroid = /safari/.test(agent) && /android/.test(agent);
    this.isWebKit = /applewebkit/.test(agent);
    this.isGecko = /gecko/.test(agent) && !this.isWebKit;
    this.isIOS = /iphone|ipad|ipod/.test(agent);
  }
  
  fitToWindow(element: HTMLElement, x: number, y: number, rightx?: number, bottomy?: number): void {
    // Reset positioning styles
    element.style.left = element.style.right = element.style.top = element.style.bottom = 'auto';
    
    // Get element dimensions
    const dimensions: Dimensions = {
      width: element.offsetWidth,
      height: element.offsetHeight
    };
    
    // Consider max dimensions for dynamic widgets
    if (!element.classList.contains("Wt-tooltip")) {
      dimensions.width = this.px(element, "maxWidth") || dimensions.width;
      dimensions.height = this.px(element, "maxHeight") || dimensions.height;
    }
    
    // Get parent and viewport information
    const offsetParent = element.offsetParent as HTMLElement;
    if (!offsetParent) return;
    
    const parentCoords = this.widgetPageCoordinates(offsetParent);
    const viewport: ViewportInfo = {
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
      
      rightx = (rightx || 0) - parentCoords.x + scrollX;
      x = offsetParent.clientWidth - (rightx + this.px(element, "marginRight"));
      hside = 1;
    } else {
      // Fits to right of x - adjust for parent offset
      const scrollX = offsetParent === document.body ? 0 : offsetParent.scrollLeft;
      x = x - parentCoords.x + scrollX - this.px(element, "marginLeft");
    }
  
    // Determine vertical positioning
    let vside = 0; // 0 = top, 1 = bottom
    if (dimensions.height > viewport.height) {
      // Taller than viewport - align with top edge
      y = viewport.scrollY;
    } else if (y + dimensions.height > viewport.scrollY + viewport.height) {
      // Too far below - position above bottomy
      if ((bottomy || 0) > viewport.scrollY + viewport.height) {
        bottomy = viewport.scrollY + viewport.height;
      }
      
      const scrollY = offsetParent === document.body ? window.scrollY : offsetParent.scrollTop;
      
      bottomy = (bottomy || 0) - parentCoords.y + scrollY;
      y = offsetParent.clientHeight - 
          (bottomy + this.px(element, "marginBottom") + this.px(element, "borderBottomWidth"));
      vside = 1;
    } else {
      // Fits below y - adjust for parent offset
      const scrollY = offsetParent === document.body ? 0 : offsetParent.scrollTop;
      y = y - parentCoords.y + scrollY - 
      this.px(element, "marginTop") + this.px(element, "borderTopWidth");
    }
  
    // Apply final positioning
    const sides = [['left', 'right'], ['top', 'bottom']];
    (element.style as any)[sides[0][hside]] = `${x}px`;
    (element.style as any)[sides[1][vside]] = `${y}px`;
  }

  //replace a dom element with a new one or just append its innerHTML
  setHtml(element: HTMLElement, html: string, append: boolean = false): void {
    if(append)
      element.insertAdjacentHTML('beforeend', html);
    else if (!html.includes('<') && !html.includes('&') && element.childNodes.length === 1 && element.firstChild?.nodeType === Node.TEXT_NODE)
        (element.firstChild as Text).textContent = html; // Update text node directly (more efficient +30%)
    else {
        //saveReparented(element);
        element.innerHTML = html;
    }
  }

  saveReparented = (el: HTMLElement): void => { //deprecated ?
    const root = document.querySelector('.Wt-domRoot') ?? document.body;
    el.querySelectorAll('.wt-reparented').forEach(node => root.append(node)); // append() moves, no manual remove needed
  };

  remove(id: string): void {
    const e = this.getElement(id);
    if (e) {
      //saveReparented(e);
      e.parentNode?.removeChild(e);
    }
  }

  replaceWith(w1Id: string, w2: HTMLElement): void {
    const element = this.$(w1Id);
    if (!element) return;
    
    element.replaceWith(w2);

    /* Reapply client-side validation, bootstrap applys validation classes
       also outside the element into its ancestors */
    if ((w2 as any).wtValidate) {
      setTimeout(() => {
        (this as any).validate(w2);
      }, 0);
    }
  }

  unstub(from: HTMLElement, to: HTMLElement, methodDisplay: number = 0): void {
    const fs = from.style;
    const ts = to.style;
    if (methodDisplay === 1) {
      fs.display && (ts.display = fs.display);
    } else {
      ['position', 'left', 'visibility'].forEach(p => ts[p as any] = fs[p as any]);
    }
    ['height', 'width'].forEach(p => fs[p as any] && (ts[p as any] = fs[p as any]));
    ts.boxSizing = fs.boxSizing || getComputedStyle(from).boxSizing;
  }

  navigateInternalPath = (e: Event, path: string): void => {
    const ev = e || window.event;
    if ((ev as MouseEvent).ctrlKey || (ev as MouseEvent).metaKey || (ev as MouseEvent).button > 0) return;   // let "open‑in‑new‑tab" etc. through
    history.pushState({path}, '', path);                     // update URL bar
    ev.preventDefault();                                     // stop full reload
    ev.stopPropagation();                                    // bubble no further
    // TODO: invoke your own router/render logic here
  }
  
  ajaxInternalPaths = (basePath: string = '/'): void => {
    const base = new URL(basePath, document.baseURI);        // absolute base once
    document.querySelectorAll('a.Wt-ip').forEach(a => {
      // 1. strip any "wtd" tracking parameter
      const rawHref = (a.getAttribute('href') ?? '')
                        .replace(/([?&])wtd.*$/, '');
      // 2. resolve …/../ and relatives with URL
      const abs  = new URL(rawHref, base);
      let  path  = abs.pathname + abs.search;                // internal path only
      path = path.replace(/^\/?_=/, '/');                    // kill "ugly" /?_=
      // 3. hydrate anchor & hook click
      (a as HTMLAnchorElement).href = abs.href;              // normalised <a href="">
      a.addEventListener('click', ev => this.navigateInternalPath(ev, path), {passive:false});
      a.classList.remove('Wt-ip');
    });
  }
  
  getElement(id: string | HTMLElement): HTMLElement | null {
    if (id instanceof HTMLElement) return id;
    return document.getElementById(id);
  }

  $(id: string | HTMLElement): HTMLElement | null {
    return this.getElement(id);
  }

  cancelEvent(event: Event, cancelType: number = 3): void {
    if (cancelType & 0x2) event.preventDefault();
    if (cancelType & 0x1) event.stopPropagation();
  }

  filter(edit: HTMLInputElement, tokens: string): void {
    const regex = new RegExp(tokens); // Create RegExp once
    edit.addEventListener('beforeinput', (e: InputEvent) => {
      if (e.data && !regex.test(e.data))
        e.preventDefault();
    });
  }

  widgetPageCoordinates(obj: HTMLElement, reference: HTMLElement = document.documentElement): Coordinates {
    const rect = obj.getBoundingClientRect();
    const refRect = reference.getBoundingClientRect();
    return {
      x: rect.left - refRect.left + window.pageXOffset,
      y: rect.top - refRect.top + window.pageYOffset
    };
  }

  widgetCoordinates(obj: HTMLElement, e: Event): Coordinates {
    const rect = obj.getBoundingClientRect();
    const rel = this.pageCoordinates(e);
    return {
      x: rel.x - rect.left - window.pageXOffset,
      y: rel.y - rect.top - window.pageYOffset
    };
  }

  pageCoordinates(event: Event): Coordinates {
    const e = (event as TouchEvent).touches?.[0] || 
              (event as TouchEvent).changedTouches?.[0] || 
              event as MouseEvent;
    return { x: e.pageX || 0, y: e.pageY || 0 };
  }

  wheelDelta(e: WheelEvent): number {
    return Math.sign(e.deltaY);
  } //only used in WGLWidget.js

  debounce(callback: (...args: any[]) => void, wait: number = 100): (...args: any[]) => void {
    let timeout: number | undefined;
    return (...args: any[]): void => {
      clearTimeout(timeout);
      timeout = window.setTimeout(() => callback(...args), wait);
    };
  }

  rafDebounce(fn: (...args: any[]) => void, wait: number = 100): (...args: any[]) => void {
    let rafId = 0;
    return (...args: any[]): void => {
      if (rafId) cancelAnimationFrame(rafId);
      rafId = requestAnimationFrame((): void => { rafId = 0; fn(...args); });
    };
  }

  setUnicodeSelectionRange(elem: HTMLInputElement | HTMLTextAreaElement, start: number, end: number, dir?: string): void {
    const startCU = toUnits(elem.value, start);
    const endCU = toUnits(elem.value, end);
    elem.setSelectionRange(startCU, endCU, dir as 'forward' | 'backward' | 'none' | undefined);      // built‑in API
  }

  getUnicodeSelectionRange(el: HTMLInputElement | HTMLTextAreaElement): { start: number; end: number } {
    return {
      start: toPoints(el.value, el.selectionStart || 0),
      end: toPoints(el.value, el.selectionEnd || 0)
    };
  }

  getSelectionRange(elem: HTMLInputElement | HTMLTextAreaElement): { start: number; end: number } {
    return { start: elem.selectionStart || 0, end: elem.selectionEnd || 0 };
  }

  setSelectionRange(elem: HTMLInputElement | HTMLTextAreaElement, start: number, end: number): void {
    //setSelectionCP(elem, start, end);
    start = Math.max(0, Math.min(start, elem.value.length));
    end = Math.max(start, Math.min(end, elem.value.length));
    elem.focus();
    elem.setSelectionRange(start, end);
  }

  /* style methods */
  css(element: HTMLElement): CSSStyleDeclaration {
    return getComputedStyle(element);
  }

  px(element: HTMLElement, prop: string): number {
    return parseFloat(this.css(element)[prop as any]) || 0;
  }

  pxSelf = (el: HTMLElement, prop: string): number => this.px(el, prop); //deprecated 
  pctSelf = (el: HTMLElement, prop: string): number => this.px(el, prop); //deprecated 
  styleAttribute = (prop: string): string => prop; //deprecated 

  inlinePx(element: HTMLElement, prop: string): number {
    return parseFloat(element.style[prop as any] as string) || 0;
  }

  boxSizing(element: HTMLElement): boolean {
    return this.css(element).boxSizing === 'border-box';
  }

  isHidden(el: HTMLElement): boolean {
    return el.offsetParent === null;
  }

  innerWidth(el: HTMLElement): number {
    return el.clientWidth;
  }

  innerHeight(el: HTMLElement): number {
    return el.clientHeight;
  }

  hide(o: string | HTMLElement): void {
    const element = this.getElement(o);
    if (element) element.style.display = "none";
  }

  inline(o: string | HTMLElement): void {
    const element = this.getElement(o);
    if (element) element.style.display = "inline";
  }

  block(o: string | HTMLElement): void {
    const element = this.getElement(o);
    if (element) element.style.display = "block";
  }

  show(o: string | HTMLElement, s: string): void {
    const element = this.getElement(o);
    if (element) element.style.display = s;
  }

  target = (e: Event | null): EventTarget | null => e?.target || null;

  addCss(selector: string, style: string): void {
    if (!this.styleSheet) {
      const styleElement = document.createElement('style');
      document.head.appendChild(styleElement);
      this.styleSheet = styleElement.sheet;
    }
    if (this.styleSheet) {
      this.styleSheet.insertRule(`${selector} { ${style} }`, this.styleSheet.cssRules.length);
    }
  }

  addStyleSheet(u: string, m?: string): void {
    const l = document.createElement('link');
    l.rel = 'stylesheet';
    l.href = u;
    if (m && m !== 'all') l.media = m;
    document.head.append(l);
  }

  removeStyleSheet(u: string): void {
    document.querySelectorAll(`link[rel=stylesheet][href="${u}"]`).forEach(l => l.remove());
    Array.from(document.styleSheets).forEach(s => {
      try {
        Array.from(s.cssRules || []).forEach((r, i) => {
          if (r.cssText === `@import url("${u}");`) s.deleteRule(i);
        });
      } catch (e) {}
    });
  }

  positionAtWidget(id: string, atId: string, orientation: string, delta: number = 0): void {
    const w = this.getElement(id);
    const atw = this.getElement(atId);
    if (!atw || !w) return;

    const { x: atX, y: atY } = this.widgetPageCoordinates(atw);
    let x: number, y: number, rightx: number, bottomy: number;

    w.style.position = "absolute";
    if (this.css(w).display === "none") w.style.display = "block";

    if (orientation === this.Horizontal) {
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

    let p: HTMLElement = atw.parentNode as HTMLElement;
    while (p && !p.classList.contains("Wt-domRoot")) {
      if ((p as any).wtReparentBarrier) break;
      if (
        this.css(p).display !== "inline" &&
        p.clientHeight > 100 &&
        (["scroll", "auto"].includes(getComputedStyle(p).overflowY) && p.scrollHeight > p.clientHeight ||
         ["scroll", "auto"].includes(getComputedStyle(p).overflowX) && p.scrollWidth > p.clientWidth)
      ) break;
      p = p.parentNode as HTMLElement;
      if (!p) break;
    }

    if (!p) return;

    const posP = this.css(p).position;
    if (!["absolute", "relative"].includes(posP)) p.style.position = "relative";

    w.parentNode?.removeChild(w);
    p.appendChild(w);
    w.classList.add("wt-reparented");

    this.fitToWindow(w, x, y, rightx, bottomy);
    w.style.visibility = "";
  }
  
  positionXY(id: string, x: number, y: number): void {
    const w = this.getElement(id);
    if (!w) return;

    if (!this.isHidden(w)) {
      w.style.display = "block";
      this.fitToWindow(w, x, y);
    }
  }

  toggleClass(el: HTMLElement, className: string, enable?: boolean): void {
    el.classList.toggle(className, enable);
  }

  capture(e: Event): void {}
  releaseCapture(e: Event): void {}

  startPointerCapture(el: HTMLElement, downEvent: PointerEvent, onMove: (e: PointerEvent) => void, onEnd?: (e: PointerEvent) => void): void {
    const id = downEvent.pointerId;
    el.setPointerCapture(id);                  // native capture
  
    const move = (e: PointerEvent): void => {
      if(e.pointerId === id) onMove(e);
    };
    
    const up = (e: PointerEvent): void => {
      if (e.pointerId !== id) return;
      el.releasePointerCapture(id);
      el.removeEventListener('pointermove', move as EventListener);
      el.removeEventListener('pointerup', up as EventListener);
      onEnd?.(e);
    };
  
    el.addEventListener('pointermove', move as EventListener, {passive:false});
    el.addEventListener('pointerup', up as EventListener, {passive:false});
  }

  getByClass(className: string, parent: ParentNode = document): NodeListOf<Element> {
    return parent.querySelectorAll(`.${className}`);
  }

  // Replacement for enableInternalPaths
  enableInternalPaths(initialPath: string): void {
    window.history.replaceState({ path: initialPath }, "", initialPath);
    this.currentPath = initialPath;

    window.addEventListener("popstate", (event) => {
      const newPath = window.location.pathname;
      this.currentPath = newPath;
      this.update(null, "path", null, true); // Trigger your app's update logic
    });
  }

  navigate(newPath: string): void {
    window.history.pushState({ path: newPath }, "", newPath);
    this.currentPath = newPath;
    this.update(null, "path", null, true); // Trigger your app's update logic
  }

  // This is defined in WtApp but referenced here
  update(element: HTMLElement | null, signalName: string, event: Event | null, feedback: boolean): void {
    // Implementation is in WtApp
  }
}

export class GlobalEventManager {
  #handlers = new Map<HTMLElement, Map<string, (event: Event) => void>>();

  constructor() {
    ['keydown', 'keyup'].forEach(event =>
      document.addEventListener(event, e => this.#handleEvent(event, e), { capture: true })
    );
  }

  bind(event: string, id: string, handler: (event: Event) => void): void {
    const el = document.getElementById(id);
    if (!el) return;
    
    if (!this.#handlers.has(el)) {
      this.#handlers.set(el, new Map());
    }
    this.#handlers.get(el)?.set(event, handler);
  }

  #handleEvent(eventType: string, event: Event): void {
    if (!event.target || ['DIV', 'BODY', 'HTML'].includes((event.target as Element).tagName)) {
      for (const [el, handlers] of this.#handlers.entries()) {
        if (document.contains(el)) {
          const handler = handlers.get(eventType);
          if (handler) handler(event);
        }
      }
    }
  }

  cleanup(id: string): void {
    const el = document.getElementById(id);
    if (el) this.#handlers.delete(el);
  }
}

/* connection.js ----------------------------------------------------------- */
export class Connection {
  #url: string;                // URL for the connection
  #socket: WebSocket | null = null;
  #sse: EventSource | null = null;
  #tries: number = 0;
  #readyResolve!: () => void;
  #keepAlive?: number | undefined;
  #lastPong?: number;
  #onMsg: (status: number, msg: string | null) => void;
  ready: Promise<void>;
  heartbeat: number = 30_000; // heartbeat interval in ms
  crossDomain: boolean;

  constructor(baseUrl: string, onMsg: (status: number, msg: string | null) => void) {
    this.#url = baseUrl;   // store URL
    this.#onMsg = onMsg;   // store callback
    this.ready = new Promise(res => { this.#readyResolve = res; });
    this.#openWebSocket(baseUrl);
    this.crossDomain = baseUrl.includes('://') && new URL(baseUrl).host !== window.location.host;
  }

  /* ---------- public API ----------- */
  async send(data: string, timeout: number = 30000, method: string = 'POST'): Promise<void> {
    if (this.#socket?.readyState === 1) {
      this.#socket.send(data);
      return;
    }

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
        const text = await response.text();
        this.#onMsg(0, text); // OK
      } else {
        this.#onMsg(1, null); // Error
      }
    } catch {
      this.#onMsg(1, null); // Error
    }
  }

  close(): void {
    this.#socket?.close();
    this.#sse?.close();
    if (this.#keepAlive) clearInterval(this.#keepAlive);
  }

  /* ---------- internals ------------ */
  #openWebSocket(relativeUrl: string): void {
    const baseUrl = window.location.origin;
    console.log('openWebSocket', baseUrl, relativeUrl);
    const url = new URL(relativeUrl, baseUrl);
    url.protocol = url.protocol.replace('http', 'ws');
    url.searchParams.set('request', 'ws');

    try {
      this.#socket = new WebSocket(url);
    } catch (e) {
      console.warn('WS ctor failed:', e);
      this.#openSSE(baseUrl);
      return;
    }

    this.#socket.onopen    = () => { this.#tries = 0; this.#readyResolve(); this.#startHeartbeat(); };
    this.#socket.onmessage = e  => this.#onMsg(0, e.data);
    this.#socket.onerror   = () => this.#reconnect(baseUrl);
    this.#socket.onclose   = () => this.#reconnect(baseUrl);
  }

  #startHeartbeat(): void {
    if (this.#keepAlive) clearInterval(this.#keepAlive);
    this.#lastPong = Date.now();
    this.#keepAlive = window.setInterval(() => {
      if (this.#socket?.readyState !== WebSocket.OPEN) {
        if (this.#keepAlive) clearInterval(this.#keepAlive);
        this.#keepAlive = undefined;
        return;
      }
      this.#socket.send('ping');
      if (Date.now() - (this.#lastPong || 0) > this.heartbeat * 1.5) this.#socket.close();
    }, this.heartbeat);
  }

  #openSSE(baseUrl: string): void {
    const url = `${baseUrl}&signal=sse`;
    this.#sse = new EventSource(url);
    this.#sse.onopen    = () => { this.#readyResolve(); };
    this.#sse.onmessage = e  => this.#onMsg(0, e.data);
    this.#sse.onerror   = () => this.#reconnect(baseUrl);
  }

  #reconnect(baseUrl: string): void {
    if (this.#socket) {
      this.#socket.close();
      this.#socket = null;
    }
    
    if (this.#sse) {
      this.#sse.close();
      this.#sse = null;
    }

    const delay = Math.min(2 ** ++this.#tries * 500, 120_000);
    console.log('reconnect in', delay, 'ms');
    setTimeout(() => this.#openWebSocket(baseUrl), delay);
  }
}

export class EventQueue {
  #buf: any[] = [];
  #max: number;
  #delay: number;
  #timer: number | null = null;
  #conn: Connection;

  constructor(conn: Connection, { max = 10_000, delay = 40 }: EventQueueOptions = {}) {
    this.#conn = conn;
    this.#max = max;
    this.#delay = delay;
  }

  push(evt: any): void {
    this.#buf.push(evt);
    if (this.#buf.length > this.#max)
      throw new Error('too many pending events');
    this.#scheduleFlush();
  }

  #scheduleFlush(): void {
    if (!this.#timer) {
      this.#timer = window.setTimeout(() => this.flush(), this.#delay);
    }
  }

  async flush(): Promise<void> {
    if (this.#timer) {
      clearTimeout(this.#timer);
      this.#timer = null;
    }
    
    if (!this.#buf.length) return;

    const payload = JSON.stringify(this.#buf);
    this.#buf.length = 0;

    console.log('sending', payload.length, 'bytes');
    console.log('sending', payload);

    //await this.#conn.ready;
    this.#conn.send(payload);
  }

  hasUnsent(): boolean {
    return this.#buf.length > 0;
  }
}

export default class WtApp {        
  #cfg: AppConfig;
  #wevt: GlobalEventManager;
  #conn: Connection;
  #queue: EventQueue;
  #libraryPromises: Map<string, Promise<void>> = new Map(); 
  #loadingLibraries: Set<string> = new Set();
  #updatePending: boolean = false;
  #hasQuit: boolean = false;
  #downX: number = 0;
  #downY: number = 0;
  #quitmsg: string | null = null;
  id: string;
  WT: WtCore;
  _p_: WtApp;
  activePointers: Map<number, PointerInfo> = new Map();
  ackUpdateId: number = 0;
  timers: Map<string, any> = new Map();
  keepAliveTimer: number;

  constructor(config: AppConfig = {}) {
    this.id = config.appId || 'Wt';
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
    
    this.#wevt = new GlobalEventManager();
    this.#conn = new Connection(this.#cfg.sessionUrl || '', this.#handleResponse.bind(this));
    this.#queue = new EventQueue(this.#conn);

    this._p_ = this;
    this.WT = new WtCore(this.#cfg);
    
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
    
    this.keepAliveTimer = window.setInterval(
      () => this.update(null, 'keepAlive', null, false), 
      (this.#cfg.keepAlive || 60) * 1000
    );
    
    this.#init();
  }

  #rand = (): number => (Math.random() * 1e6 | 0) + (this.#cfg.randomSeed || 0);

  #hasWebGL(): boolean {
    const c = document.createElement('canvas');
    return !!(c.getContext('webgl2') || c.getContext('webgl'));
  }

  /**
   * Checks if cookies are enabled in the browser by setting and reading a test cookie.
   * @private
   */
  #isCookieEnabled(): void {
    const testCookie = 'jscookietest=valid';
    document.cookie = `${testCookie}; SameSite=Lax`;
    this.#cfg.no_reload = this.#cfg.no_reload || document.cookie.includes(testCookie);
    document.cookie = `${testCookie}; expires=Thu, 01 Jan 1970 00:00:00 GMT; SameSite=Lax`;
  }

  async #init(): Promise<void> {
    /* 1️⃣ ensure ?wtd=sessionId param is present once */
    const urlObj = new URL(location.href);
    if (!urlObj.searchParams.has('wtd')) {
      urlObj.searchParams.set('wtd', this.#cfg.sessionId || '');
      history.replaceState(null, '', urlObj);
      return; // browser reloads with param, our job is done
    }
    /*  check cookies */
    this.#isCookieEnabled();

    /* 2️⃣ build loader-script URL with diagnostics */
    const jsUrl = new URL(this.#cfg.selfUrl || '', location.origin);
    jsUrl.searchParams.set('sid', this.#cfg.scriptId || '');
    jsUrl.searchParams.set('rand', this.#rand().toString());
    jsUrl.searchParams.set('scrW', screen.width.toString());
    jsUrl.searchParams.set('scrH', screen.height.toString());
    jsUrl.searchParams.set('tz', (-new Date().getTimezoneOffset()).toString());
    const tzName = Intl?.DateTimeFormat()?.resolvedOptions()?.timeZone;
    if (tzName) jsUrl.searchParams.set('tzS', tzName);
    if (this.#cfg.webGLDetect && this.#hasWebGL()) jsUrl.searchParams.set('webGL', 'true');

    try {
      const res = await fetch(jsUrl);
      if (!res.ok) {
        console.error('Failed to load script:', res.statusText);
        return;
      }
      const scriptText = await res.text();
      this.#doJavaScript(scriptText);
    } catch (error) {
      console.error('Error fetching initial script:', error);
    }
  }

  async preload(uris: string[], type: 'image' | 'fetch' = 'image'): Promise<any[]> {
    const promises = uris.map(uri => (type === 'image' ? 
      new Promise<HTMLImageElement | null>(r => { 
        const img = new Image(); 
        img.onload = () => r(img); 
        img.onerror = () => r(null); 
        img.src = uri; 
      }) :
      fetch(uri).then(r => r.ok ? r.arrayBuffer() : null).catch(() => null)
    ));
    return (await Promise.allSettled(promises))
      .map(r => (r as PromiseFulfilledResult<any>).value)
      .filter(v => v);
  }

  isSymbolDefined = (symbol: string | null): boolean => {
    if (!symbol) return false;
    return !!symbol.split('.').reduce((o, p) => o?.[p], window);
  }

  isScriptLoaded(path: string): boolean {
    return !!document.querySelector(`script[src="${path}"]`);
  }

  async loadScript(path: string, symbol?: string | null, tries: number = 2): Promise<void> {
    if (symbol && this.isSymbolDefined(symbol)) return;
    if (this.isScriptLoaded(path)) return;

    for (let attempt = 1; attempt <= tries; attempt++) {
      try {
        await new Promise<void>((resolve, reject) => {
          const script = document.createElement('script');
          script.src = path;
          script.async = true;
          script.crossOrigin = 'anonymous';

          script.onload = () => resolve();
          script.onerror = () => reject(new Error(`Failed to load script: ${path}`));

          document.head.appendChild(script);
        });
        return;
      } catch (error) {
        if (attempt === tries) throw error;
        await new Promise(resolve => setTimeout(resolve, 100 * 2 ** (attempt - 1)));
      }
    }
  }

  async loadLibrary(path: string, symbol?: string | null, tries: number = 2): Promise<void> {
    // Check for existing load
    if (this.#libraryPromises.has(path)) {
      return this.#libraryPromises.get(path);
    }

    // Check if script is already loaded or symbol is defined
    if (this.isScriptLoaded(path) || (symbol && this.isSymbolDefined(symbol))) {
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
          if (e instanceof SyntaxError && (e.message as string).includes('Unexpected token')) {
            await this.loadScript(path, symbol, tries);
          } else {
            throw e; // Rethrow network/CORS errors
          }
        }
        this.#loadingLibraries.delete(path);
      } catch (error) {
        this.#loadingLibraries.delete(path);
        const err = { 'error-description': `Fatal error: failed loading ${path}` };
        this.#sendError(err, err['error-description']);
        this.quit();
        throw error;
      }
    })();

    this.#libraryPromises.set(path, promise);
    return promise;
  }

  async #doJavaScript(js: string): Promise<void> {
    if(!js) return;
    const blob = new Blob(['export default async function(Wtc, Wt){', js, '}'], { type: 'text/javascript' });
    const url = URL.createObjectURL(blob); 
    try {
      const module = await import(url);
      await module.default(this.WT, this); 
    } catch (err) {
      console.error("Error importing module:", err);
    }
    finally {
      URL.revokeObjectURL(url);
    }
    
    if (typeof this.doAutoJavaScript === 'function') {
      this.doAutoJavaScript();
    }
  }
  
  // This would be added by the server
  doAutoJavaScript?(): void;
  
  trackPointer(element: HTMLElement): void {
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
        const pointer = this.activePointers.get(e.pointerId)!;
        pointer.position.client = { x: Math.round(e.clientX), y: Math.round(e.clientY) };
        pointer.position.page = { x: Math.round(e.pageX), y: Math.round(e.pageY) };
        pointer.position.screen = { x: Math.round(e.screenX), y: Math.round(e.screenY) };
        pointer.pressure = e.pressure || 0;
        pointer.timestamp = Date.now();
      }
    });
    
    // Remove pointer when it's lifted or canceled
    const removePointer = (e: PointerEvent) => {
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

  setPath(path: string): void {
    history.pushState(null, '', path);
  }

  addTimerEvent({ id, delay, repeat = -1, callback, context = null }: TimerEventOptions): number {
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
  
    const timerId = repeat === -1 ? 
      window.setTimeout(handler, delay) : 
      window.setInterval(handler, repeat);
      
    this.timers.set(id, { timerId, repeat, handler });
  
    return timerId;
  }
  
  clearTimer(id: string): void {
    const t = this.timers.get(id);
    if (t) {
      (t.repeat === -1 ? clearTimeout : clearInterval)(t.timerId);
      this.timers.delete(id);
    }
  }
  
  clearAllTimers(): void {
    this.timers.forEach((_, id) => this.clearTimer(id));
  }

  propagateSize = (element: HTMLElement, width: number, height: number): void => {
    width = width === -1 ? element.offsetWidth : width;
    height = height === -1 ? element.offsetHeight : height;  
    
    if (((element as any).wtWidth !== width) || ((element as any).wtHeight !== height)) {
      (element as any).wtWidth = width;
      (element as any).wtHeight = height;
      
      // Only send valid dimensions
      if (width >= 0 && height >= 0) {
        this.emit(element, "resized", Math.round(width), Math.round(height));
      }
    }
  };

  update(element: HTMLElement | null, signalName: string, event: Event | null, feedback: boolean): void {
    const eventData: EventData = {
      object: element,
      signal: signalName,
      event,
      feedback,
      evAckId: this.ackUpdateId || 0,
    };
    this.#queue.push(this.encodeEvent(eventData)); 
    this.#queue.flush(); 
  }

  emit(obj: HTMLElement | string, config: string | { name: string; eventObject?: any; event?: Event | null }, ...Args: any[]): void {
    const userEvent: UserEvent = {
      signal: 'user',
      id: typeof obj === 'string' ? obj : obj.id || '',
      name: typeof config === 'string' ? config : config.name,
      object: typeof config === 'string' ? null : config.eventObject || null,
      event: typeof config === 'string' ? null : config.event || null,
      args: Args.map(a => a?.toDateString?.() ?? a),
      feedback: true,
      evAckId: this.ackUpdateId
    };
    this.#queue.push(this.encodeEvent(userEvent));
    this.#queue.flush();
  }

  encodeEvent(event: EventData | UserEvent): any {
    // Create structured JSON payload
    const payload: any = {
      signal: event.signal,
      evAckId: event.evAckId
    };
    
    // Add widget info if present
    if ('id' in event && event.id) {
      payload.widget = {
        id: event.id,
        name: event.name,
        args: 'args' in event ? event.args || [] : []
      };
    }
    
    // Process form data
    const form = (event.object as HTMLElement)?.closest?.('form');
    if (form) {
      payload.formData = Object.fromEntries([...new FormData(form as HTMLFormElement)].map(
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
    const eventData: any = payload.eventData = {};
    
    // Event metadata
    if (e.type) eventData.type = e.type;
    
    // Find target with ID
    const target = this.findTargetWithId(e.target as HTMLElement);
    if (target?.id) eventData.targetId = target.id;
    
    // Only handle pointer events (no fallback for older browsers)
    if ('pointerId' in e) {
      const pe = e as PointerEvent;
      eventData.pointer = {
        id: pe.pointerId,
        type: pe.pointerType,
        isPrimary: pe.isPrimary,
        pressure: pe.pressure || 0,
        position: {
          client: { x: Math.round(pe.clientX), y: Math.round(pe.clientY) },
          page: { x: Math.round(pe.pageX), y: Math.round(pe.pageY) },
          screen: { x: Math.round(pe.screenX), y: Math.round(pe.screenY) }
        },
        button: pe.button || 0,
        buttons: pe.buttons || 0
      };
      
      // Calculate widget-relative coordinates if needed
      if (event.object && (event.object as Node).nodeType !== 9) {
        const rect = (event.object as HTMLElement).getBoundingClientRect();
        eventData.pointer.position.widget = {
          x: Math.round(pe.clientX - rect.left),
          y: Math.round(pe.clientY - rect.top)
        };
        
        // Add scroll information if available
        if ('scrollLeft' in (event.object as HTMLElement)) {
          const el = event.object as HTMLElement;
          eventData.scroll = {
            x: Math.round(el.scrollLeft),
            y: Math.round(el.scrollTop),
            width: Math.round(el.clientWidth),
            height: Math.round(el.clientHeight)
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
          .filter(p => this.findTargetWithId(p.target as HTMLElement) === target)
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
        identifier: pe.pointerId,
        type: pe.pointerType,
        isPrimary: pe.isPrimary,
        position: {
          client: { x: Math.round(pe.clientX), y: Math.round(pe.clientY) },
          page: { x: Math.round(pe.pageX), y: Math.round(pe.pageY) },
          screen: { x: Math.round(pe.screenX), y: Math.round(pe.screenY) }
        },
        pressure: pe.pressure || 0
      }];
      
      // Add information for common multi-touch gestures
      if (eventData.touches.length >= 2) {
        const points = eventData.touches.map((t: any) => t.position.page);
        
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
        dx: Math.round(pe.pageX - this.#downX),
        dy: Math.round(pe.pageY - this.#downY)
      };
      
      // Wheel information
      if ('deltaY' in e) {
        const we = e as unknown as WheelEvent;
        eventData.wheel = { 
          deltaY: Math.round(we.deltaY),
          deltaMode: we.deltaMode
        };
      }
    }
    
    // Keyboard information
    if ('key' in e) {
      const ke = e as KeyboardEvent;
      eventData.keyboard = {
        key: ke.key,
        code: ke.code,
        modifiers: {
          alt: ke.altKey || false,
          ctrl: ke.ctrlKey || false,
          meta: ke.metaKey || false,
          shift: ke.shiftKey || false
        }
      };
    }
    
    // Store payload
    event.payload = payload;
    return event;
  }

  getFormElementValue(el: HTMLElement): any {
    // Custom value encoder
    if ((el as any).wtEncodeValue)
      return (el as any).wtEncodeValue(el);
    
    const input = el as HTMLInputElement;
    
    // Handle different element types
    switch(input.type) {
      case 'select-multiple':
        const select = el as HTMLSelectElement;
        return [...select.selectedOptions].map(opt => opt.value);
        
      case 'checkbox':
      case 'radio':
        return input.indeterminate || input.style.opacity === '0.5' ? 
          'indeterminate' : 
          input.checked ? input.value : undefined;
        
      case 'file':
        return undefined;
        
      default:
        // Handle text inputs
        if (el.classList.contains('Wt-edit-emptyText'))
          return '';
        
        // Handle WTextEdit
        (el as any).ed?.save();
        
        const value = input.value;
        
        // Add selection information if focused
        if (document.activeElement === el) {
          return {
            value,
            selection: {
              start: input.selectionStart,
              end: input.selectionEnd
            }
          };
        }
        
        return value;
    }
  }

  getElement(id: string): HTMLElement | null {
    return document.getElementById(id);
  }

  findTargetWithId(target: HTMLElement | null): HTMLElement | null {
    while (target && !target.id && target.parentNode) {
      target = target.parentNode as HTMLElement;
    }
    return target;
  }

  #handleResponse(status: number, msg: string | null): void {
    if (status === 0 && msg) {
      try {
        this.#doJavaScript(msg);
      } catch (e: any) {
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
  }

  startIdleTimeout(): void {
    if (!this.#cfg.idleTimeout) return;
    
    const reset = () => {
      clearTimeout(this.timers.get('idle')?.timerId);
      const logout = () => this.#conn.send('{"signal":"user", "id": "Wt-idleTimeout"}');
      const timerId = setTimeout(logout, this.#cfg.idleTimeout ?? 0);
      this.timers.set('idle', { timerId, repeat: -1, handler: logout });
    };
    
    ['wheel', 'pointerdown', 'keydown', 'visibilitychange'].forEach(e =>
      document.addEventListener(e, reset, { passive: true })
    );
    
    reset();
  }

  #sendError(err: any, msg: string): void {
    const error = {
      signal: 'error',
      id: 'Wt-error',
      name: 'error',
      object: null,
      event: null,
      args: [err],
      feedback: false,
      evAckId: this.ackUpdateId
    };
    this.#queue.push(this.encodeEvent(error));
    this.#queue.flush();
  }

  quit(): void {
    this.timers.forEach(t => clearTimeout(t.timerId));
    this.#hasQuit = true;
    this.#conn.close();
    const tr = this.WT.$("Wt-timers");
    if (tr) {
      this.WT.setHtml(tr, "", false);
    }
  }

  setTitle(title: string): void {
    document.title = title;
  }
}
