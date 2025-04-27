/* ------------------------------------------------------------------
   leafletMap.es2022.js  ⚡️ v1.0.0 — 2025‑04‑26
   ------------------------------------------------------------------
   A modern, **framework‑agnostic** drop‑in replacement for the legacy
   *WLeafletMap* widget.

   ✦ ES 2022 class (export + default)  
   ✦ Optional *import* of Leaflet – falls back to global `L`  
   ✦ Pointer‑safe pan/zoom & responsive resize (ResizeObserver)  
   ✦ API parity with legacy (`addTileLayer()`, `addMarker()`, …)  
   ✦ Z‑index stacking for pop‑ups/dialogs  
   ✦ `onInteraction` callback for zoom / pan observers  
   ✦ Tiny footprint – < 2 k min+gzip (without Leaflet)
   ------------------------------------------------------------------ */

/** resolve Leaflet namespace no‑matter‑how it was loaded */
const Leaflet = /** @type {typeof import('leaflet')} */ (globalThis.L ?? await import('leaflet'));

/**
 * @typedef {import('leaflet').LatLngExpression}    LatLngExpr
 * @typedef {import('leaflet').LatLngTuple}         LatLng
 * @typedef {import('leaflet').PolylineOptions}     PolylineOpts
 * @typedef {import('leaflet').CircleMarkerOptions} CircleOpts
 * @typedef {import('leaflet').MarkerOptions}       MarkerOpts
 * @typedef {import('leaflet').MapOptions}          MapOpts
 */

export class LeafletMap {
  /**
   * @param {HTMLElement|string} container  – DOM node or selector
   * @param {{
   *   position?: LatLngExpr,
   *   zoom?    : number,
   *   mapOpts? : MapOpts,
   *   baseZ?   : number,
   * }} [cfg]
   */
  constructor (container, cfg = {}) {
    if (typeof container === 'string') container = document.querySelector(container);
    if (!(container instanceof HTMLElement)) throw new TypeError('LeafletMap › container must be an HTMLElement');

    const {
      position = [0,0],
      zoom     = 13,
      mapOpts  = /** @type {MapOpts} */ ({}),
      baseZ    = this.#detectBaseZ(container)
    } = cfg;

    // ——— internal -------------------------------------------------------
    this.#el        = container;
    this.#markers   = new Map();           // id → Leaflet.Marker
    this.#lastZoom  = zoom;
    this.#lastPos   = Leaflet.latLng(position);

    // ——— map instance ---------------------------------------------------
    this.map = Leaflet.map(container, {
      center : position,
      zoom   : zoom,
      ...mapOpts
    });

    // adjust Leaflet pane stacking if embedded in modal/popup
    if (baseZ) this.#bumpZ(baseZ);

    // listeners (zoom / pan) -------------------------------------------
    this.map.on('zoomend',  () => this.#handleZoom());
    this.map.on('moveend',  () => this.#handlePan());

    // responsive --------------------------------------------------------
    this.#ro = new ResizeObserver(() => this.resize());
    this.#ro.observe(container);
  }

  /* ------------------------------------------------------------------- */
  // ——— legacy‑compat API ———

  /** @param {string} tmpl @param {any} opts */
  addTileLayer (tmpl, opts = {}) {
    return Leaflet.tileLayer(tmpl, opts).addTo(this.map);
  }

  /** programmatic zoom */
  zoom (level) {
    this.#lastZoom = level;
    this.map.setZoom(level);
  }

  /** smooth pan */
  panTo (lat, lng) {
    const ll = [lat, lng];
    this.#lastPos = Leaflet.latLng(ll);
    this.map.panTo(ll);
  }

  /** @param {LatLng[]} points */
  addPolyline (points, opts = /** @type {PolylineOpts} */ ({})) {
    return Leaflet.polyline(points, opts).addTo(this.map);
  }

  addCircle (centre, opts = /** @type {CircleOpts} */ ({})) {
    return Leaflet.circle(centre, opts).addTo(this.map);
  }

  /**
   * @param {string|number} id
   * @param {LatLngExpr|import('leaflet').Marker} marker
   * @param {MarkerOpts} [opts]
   */
  addMarker (id, marker, opts) {
    const m = marker instanceof Leaflet.Marker ? marker : Leaflet.marker(marker, opts);
    m.addTo(this.map);
    this.#markers.set(id, m);
    return m;
  }

  removeMarker (id) {
    const m = this.#markers.get(id);
    if (m) { this.map.removeLayer(m); this.#markers.delete(id); }
  }

  /** @param {LatLngExpr} ll */
  moveMarker (id, ll) {
    this.#markers.get(id)?.setLatLng(ll);
  }

  /** manual resize hook (for tabs, hidden panes, …) */
  resize () { this.map.invalidateSize(); }

  /** legacy encoder  → `{ position:[lat,lng], zoom }` */
  encodeValue () {
    const c = this.map.getCenter();
    return { position:[c.lat,c.lng], zoom:this.map.getZoom() };
  }

  /** destroy and clean‑up */
  destroy (){
    this.#ro.disconnect();
    this.map.remove();
    this.#markers.clear();
  }

  /* ------------------------------------------------------------------- */
  // ——— helpers ———
  #handleZoom(){
    const z = this.map.getZoom();
    if (z !== this.#lastZoom){
      this.#onInteract?.('zoom', z);
      this.#lastZoom = z;
    }
  }
  #handlePan(){
    const c = this.map.getCenter();
    if (c.lat!==this.#lastPos.lat || c.lng!==this.#lastPos.lng){
      this.#onInteract?.('pan', c);
      this.#lastPos = c;
    }
  }
  /** crude search for parent popup/modal z‑index */
  #detectBaseZ(el){
    let p = el.parentElement;
    while(p && p!==document.body){
      if (p.wtPopup) return +getComputedStyle(p).zIndex || 0;
      p = p.parentElement;
    }
    return 0;
  }
  #bumpZ(base){
    this.map.getPane('tilePane'   ).style.zIndex = base + 200;
    this.map.getPane('overlayPane').style.zIndex = base + 400;
    this.map.getPane('shadowPane' ).style.zIndex = base + 500;
    this.map.getPane('markerPane' ).style.zIndex = base + 600;
    this.map.getPane('tooltipPane').style.zIndex = base + 650;
    this.map.getPane('popupPane'  ).style.zIndex = base + 700;
  }

  /* ------------------------------------------------------------------- */
  /** subscribe to pin / zoom updates */
  onInteraction (cb){ this.#onInteract = cb; }

  /* ------------------------------------------------------------------- */
  // ——— private fields ———
  #el;            /** @type {HTMLElement}   */
  #markers;       /** @type {Map<string|number, import('leaflet').Marker>} */
  #lastZoom;      /** @type {number}        */
  #lastPos;       /** @type {import('leaflet').LatLng} */
  #onInteract;    /** @type {(type:'zoom'|'pan',payload:any)=>void|undefined} */
  #ro;            /** @type {ResizeObserver} */
}

