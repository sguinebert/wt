
import Sortable from '../vendor/sortablejs/sortable.core.esm.js';
// Make Sortable globally accessible for SlickGrid plugins
window.Sortable = Sortable;
import {
  Editors,
  Formatters,
  Aggregators, // this for grouping
  SlickGlobalEditorLock,
  SlickRowSelectionModel,
  SlickColumnPicker,
  SlickDataView,
  //SlickRemoteModel, SlickEvent,
  SlickGridMenu,
  SlickGridPager,
  SlickGrid, 
  SlickGroupItemMetadataProvider, //this for grouping
  Utils
} from '../vendor/slickgrid/slick.grid.esm.min.js';
// Create global Slick namespace for internal SlickGrid references
window.Slick = window.Slick || {
  Editors,
  Formatters,
  Data: { 
    DataView: SlickDataView,
    GroupItemMetadataProvider: SlickGroupItemMetadataProvider,
    //RemoteModel: SlickRemoteModel
  },
  Controls: {
    Grid: SlickGrid
  },
  RowSelectionModel: SlickRowSelectionModel,
  ColumnPicker: SlickColumnPicker,
  GridMenu: SlickGridMenu,
  Plugins: {},
  GridPager: SlickGridPager,
  GlobalEditorLock: SlickGlobalEditorLock,
  //Event: SlickEvent 

};
export { 
  Editors, 
  Formatters, 
  Aggregators
};

/**
 * Factory for creating common formatters
 */
export const WtFormatters = {
  /**
   * Format a value as currency
   * @param {string} prefix - Currency symbol, default '$'
   * @param {number} decimals - Number of decimal places, default 2
   */
  currency: (prefix = '$', decimals = 2) => (row, cell, value) => {
    if (value == null) return '';
    return `${prefix}${parseFloat(value).toFixed(decimals)}`;
  },
  
  /**
   * Format a value as a percentage
   * @param {boolean} includeBar - Whether to include a visual bar, default true
   */
  percentage: (includeBar = true) => (row, cell, value) => {
    if (value == null) return '';
    const percent = parseFloat(value);
    if (includeBar) {
      return `<div class="wt-percent-bar" style="width:${percent}%"></div>${percent.toFixed(1)}%`;
    }
    return `${percent.toFixed(1)}%`;
  },
  
  /**
   * Format a value as a sparkline chart
   * @param {string} field - Field containing array of values
   * @param {Object} options - Sparkline options
   */
  sparkline: (field, options = {}) => (row, cell, value, columnDef, dataContext) => {
    if (!dataContext[field] || !Array.isArray(dataContext[field])) return '';
    
    const svgElem = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svgElem.setAttributeNS(null, 'width', options.width || 100);
    svgElem.setAttributeNS(null, 'height', options.height || 20);
    svgElem.setAttributeNS(null, 'stroke-width', options.strokeWidth || 1.5);
    svgElem.classList.add('wt-sparkline');
    
    // If sparkline.js is available, use it
    if (window.sparkline && typeof window.sparkline.sparkline === 'function') {
      window.sparkline.sparkline(svgElem, dataContext[field], options);
    } else {
      // Simple fallback implementation
      const values = dataContext[field];
      const min = Math.min(...values);
      const max = Math.max(...values);
      const range = max - min || 1;
      const width = options.width || 100;
      const height = options.height || 20;
      const points = values.map((v, i) => {
        const x = (i / (values.length - 1)) * width;
        const y = height - ((v - min) / range) * height;
        return `${x},${y}`;
      }).join(' ');
      
      const path = document.createElementNS('http://www.w3.org/2000/svg', 'polyline');
      path.setAttributeNS(null, 'points', points);
      path.setAttributeNS(null, 'fill', 'none');
      path.setAttributeNS(null, 'stroke', options.color || '#1e5d90');
      svgElem.appendChild(path);
    }
    
    return svgElem.outerHTML;
  }
};

/**
 * WTableView – ES 2022 wrapper around SlickGrid for Wt frontend
 * ----------------------------------------------------------------
 * The back‑end must expose an endpoint (or callback) returning JSON
 *   {
 *     items:   [ {...}, ... ],
 *     done:    Boolean,
 *     nextKey: Any          // optional, for cursor‑based paging
 *   }
 *
 * dataUrl can be either
 *   • a **string** (base URL) – paging parameters are auto‑appended, or
 *   • a Wt.emit **function** that receives an object with
 *
 * Example:
 *   const tv = new WTableView({
 *     el: '#grid',
 *     dataUrl: ({cursor,limit}) => `/api/data?sid=${Wt.sessionId}&cursor=${cursor??''}&limit=${limit}`,
 *     columns,
 *     pagerElement: '#pager',
 *   });
 */
export default class WTableView {
  // ───────── private state ─────────
  #el;
  #columns;
  #options;

  #dataUrl;            // string | fn
  #pageSize;
  #cursor = null;
  #done   = false;

  #data = [];
  #existingIds = new Set();
  #dataView;
  #grid;
  #resizeObserver;
  #model;

  #onScrolled;
  #onColumnResized;
  #onDropEvent;
  #onRowDropEvent;

  // advanced customized options
  #groupItemMetadataProvider;
  #aggregators;
  #columnGroups;
  #frozenColumns;
  #frozenRows;
  #headerRowHeight;
  #footerRowHeight;
  #showHeaderRow;
  #showFooterRow;
  #validators;
  #treeOptions;

  #maxRowCount = 0;
  #dataLoadedSubscribers = [];// Subscribers for data loaded events (debugging)

  // ───────── ctor ─────────
  constructor({
    el,
    columns = [],
    options = {},

    dataUrl,           // string or ({cursor,limit}) => url
    pageSize = 100,

    pagerElement = null,

    // advanced customized options
    grouping = null,           // { getter, formatter, aggregators, collapsed }
    columnGroups = [],         // Array of column group definitions
    frozenColumns = -1,        // Number of columns to freeze (-1 = none)
    frozenRows = 0,            // Number of rows to freeze (0 = none)
    showHeaderRow = false,     // Show filter header row
    headerRowHeight = 30,      // Height of the header row
    showFooterRow = false,     // Show footer row with aggregates
    footerRowHeight = 30,      // Height of the footer row
    multiSelect = true,        // Allow multi-row selection
    //
    contextMenu = null,  // { items: [{ title, action, icon, enabled }] }
    enableClipboard = false,
    validators = {},  // { columnField: validatorFn(value, item) }
    treeOptions = null,  // { idField, parentIdField, expandedField }

    onScrolled,
    onColumnResized,
    onDropEvent,
    onRowDropEvent, 
    Wt = null
  }) {
    this.#el = typeof el === 'string' ? document.querySelector(el) : el;
    if (!this.#el) throw new Error('WTableView: container element not found');

    this.Wt = Wt;
    this.WT = Wt?.WT;
    // Initialize additional options with defaults
    this.#columnGroups = columnGroups;
    this.#frozenColumns = frozenColumns;
    this.#frozenRows = frozenRows;
    this.#showHeaderRow = showHeaderRow;
    this.#headerRowHeight = headerRowHeight;
    this.#showFooterRow = showFooterRow;
    this.#footerRowHeight = footerRowHeight;
    this.#validators = validators;


    this.#columns = columns;
    this.#options = {
      editable: true,
      enableCellNavigation: true,
      asyncEditorLoading: true,
      autoEdit: false,
      enableColumnReorder: true,
      forceFitColumns: false,
      syncColumnCellResize: true,
      multiColumnSort: true,
      rowHeight: 28,
      // advanced options
      frozenColumn: frozenColumns,
      frozenRow: frozenRows,
      showHeaderRow: showHeaderRow,
      headerRowHeight: headerRowHeight,
      createFooterRow: showFooterRow,
      footerRowHeight: footerRowHeight,
      multiSelect: multiSelect,
      ...options
    };

    this.#dataUrl  = dataUrl;
    this.#pageSize = Math.max(50, pageSize); // Set minimum pageSize to 50
    console.log('WTableView: pageSize', this.#pageSize);

    this.#onScrolled = onScrolled || ((left, top, width, height) => {
        Wt?.emit(this.#el, "scrolled", 
          Math.round(left), 
          Math.round(top), 
          Math.round(width), 
          Math.round(height)
        );
    });
    
    this.#onColumnResized = onColumnResized || ((columnId, width) => {
        Wt?.emit(this.#el, "columnResized", columnId, parseInt(width));
    });
    
    this.#onDropEvent = onDropEvent || ((rowIdx, columnId, sourceId, mimeType) => {
        Wt?.emit(
          this.#el,
          { name: "dropEvent", eventObject: {}, event: {} },
          rowIdx,
          columnId,
          sourceId,
          mimeType
        );
    });
    
    this.#onRowDropEvent = onRowDropEvent || ((rowIdx, columnId, sourceId, mimeType, side) => {
        Wt?.emit(
          this.#el,
          { name: "rowDropEvent", eventObject: {}, event: {} },
          rowIdx,
          columnId,
          sourceId,
          mimeType,
          side || "bottom"
        );
    });

    // Remote model
    // this.#model = new SlickRemoteModel({
    //   pageSize: this.#pageSize,
    //   loader:   ({ page, from, to, success, failure }) => this.#loader({ page, from, to, success, failure })
    // });
    this.#model = {
      //existingIds : new Set(),
      //data: [],
      pageSize: this.#pageSize,
      //loading: {}, // Track loading ranges
      //onDataLoaded: { subscribe: (fn) => this.#dataLoadedSubscribers.push(fn) },
      
      ensureData: async (from, to) => {
        const page = Math.floor(from / this.#pageSize);
        //console.log(`WTableView: ensureData called for range ${from}-${to} (page ${page})`);
        let processedCount = 0;
        try {
          // const result = await new Promise((resolve, reject) => {
          //     this.#loader({ 
          //       page,
          //       from, 
          //       to, 
          //       success: resolve, 
          //       failure: reject 
          //     });
          // });
          const result = await this.#loader({ 
            page,
            from,
            to
          });
          //console.log(`WTableView: data loaded for range ${from}-${to}`, result);

          // Process items using the configured strategy
          processedCount = this.processItems(result.items);


          // Notify subscribers
          // this.#dataLoadedSubscribers.forEach(fn => 
          //   fn(null, { from, to, pageSize: this.#pageSize })
          // );
        } catch (err) {
          console.error('WTableView data loading error:', err);
        } finally {
          return processedCount;
        }
      }
    };

    // DataView & grid ------------------------------------------------
       // Initialize grouping support
    if (grouping) {
      this.#groupItemMetadataProvider = new SlickGroupItemMetadataProvider();
      
      // Convert aggregator strings to actual aggregator instances
      if (grouping.aggregators && Array.isArray(grouping.aggregators)) {
        this.#aggregators = grouping.aggregators.map(agg => {
          if (typeof agg === 'string') {
            // Handle string-based aggregator config
            const [type, field] = agg.split(':');
            switch (type.toLowerCase()) {
              case 'sum': return new Aggregators.Sum(field);
              case 'avg': return new Aggregators.Avg(field);
              case 'min': return new Aggregators.Min(field);
              case 'max': return new Aggregators.Max(field);
              default: return null;
            }
          } else if (typeof agg === 'object') {
            // Handle object-based aggregator config { type: 'sum', field: 'amount' }
            if (!agg.type || !agg.field) return null;
            switch (agg.type.toLowerCase()) {
              case 'sum': return new Aggregators.Sum(agg.field);
              case 'avg': return new Aggregators.Avg(agg.field);
              case 'min': return new Aggregators.Min(agg.field);
              case 'max': return new Aggregators.Max(agg.field);
              default: return null;
            }
          }
          return agg; // Already an Aggregator instance
        }).filter(Boolean);
      }
    }

    // Enhanced DataView initialization
    this.#dataView = new SlickDataView({ 
      inlineFilters: true,
      groupItemMetadataProvider: this.#groupItemMetadataProvider
    });
    // Ensure columnGroups is truly empty if not needed
    this.#columnGroups = columnGroups && columnGroups.length ? columnGroups : null;

    // Setup column groups if provided
    if (this.#columnGroups) {
      this.#setupColumnGroups();
    }
    //this.#dataView.setItems(this.#model.data);
    this.#dataView.setFilterArgs({ model: this.#model });
    // Create grid
    this.#grid     = new SlickGrid(this.#el, this.#dataView, this.#columns, this.#options);

   // Register grouping plugin if needed
    if (this.#groupItemMetadataProvider) {
      this.#grid.registerPlugin(this.#groupItemMetadataProvider);
    }
    
    // Set initial grouping if provided
    if (grouping) {
      this.setGrouping(grouping);
    }
    
    // Setup header row if enabled
    if (this.#showHeaderRow) {
      this.#setupHeaderRowFilters();
    }
    
    // Setup footer row if enabled
    if (this.#showFooterRow) {
      this.#setupFooterRowAggregates();
    }  
    // Setup context menu if provided
    if (contextMenu) {
      this.#setupContextMenu(contextMenu);
    }
  // Enable clipboard functionality if requested
  if (enableClipboard) {
    this.#setupClipboard();
  }
  // Enable advanced editing if validators are provided
  if (Object.keys(validators).length) {
    this.#setupAdvancedEditing();
  }
      // Setup hierarchical data if options provided
  if (treeOptions) {
    this.#treeOptions = treeOptions;
    this.#setupTreeGrid(treeOptions);
  }
  

    // Optional plugins (guarded in case tree‑shaken out ?) -------------
    this.#grid.setSelectionModel(new SlickRowSelectionModel());
    this.#grid.registerPlugin(new SlickColumnPicker(this.#columns, this.#grid, this.#options));
    this.#grid.registerPlugin(new SlickGridMenu(this.#columns, this.#grid, this.#options));


    // Pager ---------------------------------------------------------
    if (pagerElement) {
      const pagerEl = typeof pagerElement === 'string' ? document.querySelector(pagerElement) : pagerElement;
      if (pagerEl) new SlickGridPager(this.#dataView, this.#grid, pagerEl);
    }

    // Core events ---------------------------------------------------
    this.#wireGridEvents();
    this.#initResizeObserver();

    // Initial load --------------------------------------------------
    this.updatePageSize();

  }

  /*───────── grid ↔ model ─────────*/
  #wireGridEvents() {
    this.#dataView.onRowCountChanged.subscribe(() => {
      this.#grid.updateRowCount();
      this.#grid.render();
    });

    this.#dataView.onRowsChanged.subscribe((e, args) => {
      this.#grid.invalidateRows(args.rows);
      this.#grid.render();
    });
    this.#grid.onViewportChanged.subscribe((_e, args) => {
      const vp = this.#grid.getViewport();
      const buffer = Math.floor(this.#pageSize / 2); // Buffer rows above/below
      
      // Extend the range to include a buffer
      const fromRow = Math.max(0, vp.top - buffer);
      const toRow = Math.min(vp.bottom + this.#pageSize, this.#grid.getDataLength() + this.#pageSize);
      //console.log(`WTableView: viewport changed, loading range ${fromRow}-${toRow} (buffer ${buffer})`);
      this.#model.ensureData(fromRow, toRow);
      // BACKEND SIGNAL: 
      // APP.emit(this.#el, "scrolled", vp.leftPx, vp.top, 
      //   this.#grid.getCanvasNode().clientWidth, 
      //   this.#grid.getCanvasNode().clientHeight);
      this.#onScrolled?.(vp.leftPx, vp.top, this.#grid.getCanvasNode().clientWidth, this.#grid.getCanvasNode().clientHeight);
    });

    this.#grid.onColumnsResized.subscribe(() => {
      // BACKEND SIGNAL: Replace with APP.emit() loop for all columns:
      // this.#columns.forEach(col => {
      //   APP.emit(this.#el, "columnResized", col.id, col.width);
      // });
      this.#columns.forEach(col => this.#onColumnResized?.(col.id, col.width));
    });

    // Sorting handled by DataView
    this.#grid.onSort.subscribe((_e, { sortCols }) => {
      this.#dataView.sort((a, b) => {
        for (const { sortCol, sortAsc } of sortCols) {
          const x = a[sortCol.field];
          const y = b[sortCol.field];
          if (x === y) continue;
          return sortAsc ? (x > y ? 1 : -1) : (x > y ? -1 : 1);
        }
        return 0;
      });
      this.#grid.invalidate();
      this.#grid.render();
    });

    // Row drag example (external consumers can hook)
    Sortable.create(this.#grid.getCanvasNode(), {
      animation: 150,
      handle: '.slick-cell',
      onEnd: evt => {
        // BACKEND SIGNAL: Replace with:
        // APP.emit(this.#el, { name: "rowDropEvent", eventObject: evt, event: evt.originalEvent },
        //   evt.newIndex, null, null, null, "bottom");
          this.#handleRowDrop(evt);
      }
    });
  }

  #initResizeObserver() {
    this.#resizeObserver = new ResizeObserver(() => {
      this.#grid.resizeCanvas();
      this.updatePageSize(); // Add this line
    });
    this.#resizeObserver.observe(this.#el);
  }
  /**

  // ───────── private methods ─────────
  /*───────── loader (HTTP or Wt.emit) ─────────*/
  /**
   * Load data from the configured data source
   * @private
   * @param {Object} params - Loading parameters
   * @param {number} params.page - Page number
   * @param {number} params.from - Start index
   * @param {number} params.to - End index
   * @returns {Promise<Object>} - The loaded data
   * @throws {Error} - If loading fails
   */
  async #loader({ page, from, to }) {
    try {
      if (this.#dataUrl) {
        if (typeof this.#dataUrl === 'function') {
          // Check if the function returns a string (URL) or a Promise (data)
          const result = this.#dataUrl({ page, from, to });
          
          if (result instanceof Promise) {
            // Function returns data directly via Promise
            return await result;
          } else {
            // Function returns a URL string
            const url = result;
            const res = await fetch(url, { credentials: 'same-origin' });
            
            if (!res.ok) {
              throw new Error(`HTTP ${res.status}`);
            }
            
            return await res.json();
          }
        } else {
          // Regular URL string
          const url = `${this.#dataUrl}?page=${page}&from=${from}&to=${to}`;
          const res = await fetch(url, { credentials: 'same-origin' });
          
          if (!res.ok) {
            throw new Error(`HTTP ${res.status}`);
          }
          
          return await res.json();
        }
      } else {
        // Use Wt event system with a Promise wrapper
        return new Promise((resolve, reject) => {
          this.Wt?.emit('data', { page, from, to }, resolve, reject);
        });
      }
    } catch (err) {
      console.error('WTableView loader', err);
      throw err; // Re-throw for caller to handle
    }
  }

  // ───────── Private Setup Methods ─────────
  /**
   * Calculate optimal page size based on current container dimensions
   * @private
   */
  #calculateOptimalPageSize() {
    // Get container dimensions
    const containerHeight = this.#el.clientHeight;
    
    // Calculate visible rows (account for headers/footers)
    const headerHeight = this.#options.showHeaderRow ? this.#options.headerRowHeight : 0;
    const footerHeight = this.#options.createFooterRow ? this.#options.footerRowHeight : 0;
    const slickHeaderHeight = this.#grid.getHeaderRow().height || 0; // SlickGrid header row height
    
    // Calculate available height for rows
    const availableHeight = containerHeight - headerHeight - footerHeight - slickHeaderHeight;
    
    // Calculate visible rows
    const rowHeight = this.#options.rowHeight;
    const visibleRows = Math.floor(availableHeight / rowHeight);
    
    // Add buffer (2x visible rows is a good default)
    const bufferFactor = 2.5;
    const optimalPageSize = Math.max(50, Math.ceil(visibleRows * bufferFactor));
    
    return optimalPageSize;
  }
  /**
   * Update page size based on container dimensions
   */
  updatePageSize() {
    const newPageSize = this.#calculateOptimalPageSize();
    
    // Only update if significantly different
    if (Math.abs(this.#pageSize - newPageSize) > 10) {
      //console.log(`WTableView: Adjusting page size from ${this.#pageSize} to ${newPageSize}`);
      this.#pageSize = newPageSize;
      
      // Update model pageSize
      if (this.#model) {
        this.#model.pageSize = newPageSize;
      }
      

    }
    // Refresh with new buffer size
    const vp = this.#grid.getViewport();
    const buffer = Math.floor(this.#pageSize / 2);
    const fromRow = Math.max(0, vp.top - buffer);
    const toRow = Math.min(vp.bottom + buffer, this.#grid.getDataLength() + buffer);
    this.#model.ensureData(fromRow, toRow);
    return this;
  }
  /**
   * Setup column groups
   * @private
   */
  #setupColumnGroups() {
    // Create column group headers
    const headerRowEl = document.createElement('div');
    headerRowEl.className = 'slick-column-groups';
    
    // Insert before the grid's container
    this.#el.prepend(headerRowEl);
    
    // Set up each column group
    this.#columnGroups.forEach(group => {
      const groupEl = document.createElement('div');
      groupEl.className = 'slick-column-group';
      groupEl.textContent = group.name;
      
      // Calculate group width based on columns it spans
      let groupWidth = 0;
      let left = 0;
      
      // Find the columns that belong to this group
      const groupColumns = this.#columns.filter(col => 
        group.columns.includes(col.id) || group.columns.includes(col.field)
      );
      
      // Find position of first column in group
      for (let i = 0; i < this.#columns.length; i++) {
        const col = this.#columns[i];
        if (group.columns.includes(col.id) || group.columns.includes(col.field)) {
          break;
        }
        left += col.width || 80;
      }
      
      // Calculate total width of group
      groupColumns.forEach(col => {
        groupWidth += col.width || 80;
      });
      
      // Position group header
      groupEl.style.left = `${left}px`;
      groupEl.style.width = `${groupWidth}px`;
      
      headerRowEl.appendChild(groupEl);
    });
  }
  
  /**
   * Setup header row filters
   * @private
   */
  #setupHeaderRowFilters() {
    this.#grid.onHeaderRowCellRendered.subscribe((e, { node, column }) => {
      if (!column.filterable) return;
      
      const input = document.createElement('input');
      input.type = 'text';
      input.className = 'filter-input';
      input.placeholder = `Filter ${column.name}...`;
      
      // Clear existing content
      node.innerHTML = '';
      node.appendChild(input);
      
      // Add filter handler
      input.addEventListener('input', e => {
        const filterVal = e.target.value.trim().toLowerCase();
        
        // Update filter for this column
        const columnFilters = this.#dataView.getFilterArgs().columnFilters || {};
        if (filterVal) {
          columnFilters[column.field] = filterVal;
        } else {
          delete columnFilters[column.field];
        }
        
        // Apply filters
        this.#dataView.setFilterArgs({
          ...this.#dataView.getFilterArgs(),
          columnFilters
        });
        
        // Set filter function if not already set
        if (!this.#dataView.getFilter()) {
          this.#dataView.setFilter(this.#columnFilter);
        }
        
        this.#dataView.refresh();
      });
    });
  }
  
  /**
   * Column filter function
   * @private
   */
  #columnFilter = (item, args) => {
    if (!args || !args.columnFilters) return true;
    
    const columnFilters = args.columnFilters;
    let isMatch = true;
    
    for (const field in columnFilters) {
      const filterVal = columnFilters[field];
      const itemVal = String(item[field] || '').toLowerCase();
      
      if (filterVal && !itemVal.includes(filterVal)) {
        isMatch = false;
        break;
      }
    }
    
    return isMatch;
  }
  
  /**
   * Setup footer row with aggregates
   * @private
   */
  #setupFooterRowAggregates() {
    // Update footer row when data changes
    this.#dataView.onRowCountChanged.subscribe(() => {
      this.#updateFooterRow();
    });
    
    this.#dataView.onRowsChanged.subscribe(() => {
      this.#updateFooterRow();
    });
    
    // Initial update
    this.#updateFooterRow();
  }
  
  /**
   * Update footer row with aggregate values
   * @private
   */
  #updateFooterRow() {
    if (!this.#showFooterRow) return;
    
    const totals = {};
    const items = this.#dataView.getItems();
    
    // Calculate totals for each column with an aggregate function
    this.#columns.forEach(column => {
      if (!column.aggregate) return;
      
      switch (column.aggregate.toLowerCase()) {
        case 'sum':
          totals[column.field] = items.reduce((sum, item) => {
            return sum + (parseFloat(item[column.field]) || 0);
          }, 0);
          break;
        case 'avg':
          totals[column.field] = items.reduce((sum, item) => {
            return sum + (parseFloat(item[column.field]) || 0);
          }, 0) / (items.length || 1);
          break;
        case 'min':
          totals[column.field] = Math.min(...items.map(i => parseFloat(i[column.field]) || 0));
          break;
        case 'max':
          totals[column.field] = Math.max(...items.map(i => parseFloat(i[column.field]) || 0));
          break;
        case 'count':
          totals[column.field] = items.length;
          break;
      }
    });
    
    // Update footer cells
    this.#grid.onFooterRowCellRendered.subscribe((e, { node, column }) => {
      if (!column.aggregate) return;
      
      let value = totals[column.field];
      
      // Format the value if a formatter is specified
      if (column.footerFormatter) {
        value = column.footerFormatter(totals, column);
      } else if (typeof value === 'number') {
        // Default numeric formatting
        value = value.toFixed(column.precision || 0);
      }
      
      node.innerHTML = value || '';
    });
    
    // Trigger footer row update
    this.#grid.updateFooterRow();
  }

  /**
 * Setup custom context menu
 * @private
 */
#setupContextMenu(menuConfig) {
  const menuContainer = document.createElement('div');
  menuContainer.className = 'wt-context-menu';
  menuContainer.style.display = 'none';
  document.body.appendChild(menuContainer);
  
  // Handle right-click on grid cells
  this.#grid.onContextMenu.subscribe((e, args) => {
    e.preventDefault();
    
    // Get the cell and data item
    const cell = this.#grid.getCellFromEvent(e);
    if (!cell) return;
    
    const dataItem = this.#dataView.getItem(cell.row);
    const columnDef = this.#columns[cell.cell];
    
    // Create menu items
    menuContainer.innerHTML = '';
    
    const menuItems = typeof menuConfig.items === 'function' 
      ? menuConfig.items(dataItem, columnDef)
      : menuConfig.items;
    
    menuItems.forEach(item => {
      if (item.divider) {
        const divider = document.createElement('div');
        divider.className = 'wt-context-menu-divider';
        menuContainer.appendChild(divider);
        return;
      }
      
      const menuItem = document.createElement('div');
      menuItem.className = 'wt-context-menu-item';
      
      if (item.disabled) {
        menuItem.classList.add('disabled');
      }
      
      if (item.icon) {
        const icon = document.createElement('span');
        icon.className = 'wt-context-menu-icon ' + item.icon;
        menuItem.appendChild(icon);
      }
      
      const title = document.createElement('span');
      title.textContent = item.title;
      menuItem.appendChild(title);
      
      if (!item.disabled) {
        menuItem.addEventListener('click', () => {
          menuContainer.style.display = 'none';
          item.action(dataItem, columnDef, cell);
        });
      }
      
      menuContainer.appendChild(menuItem);
    });
    
    // Position menu at cursor
    menuContainer.style.top = `${e.pageY}px`;
    menuContainer.style.left = `${e.pageX}px`;
    menuContainer.style.display = 'block';
    
    // Hide menu when clicking elsewhere
    const hideMenu = (e) => {
      if (!menuContainer.contains(e.target)) {
        menuContainer.style.display = 'none';
        document.removeEventListener('click', hideMenu);
      }
    };
    
    setTimeout(() => {
      document.addEventListener('click', hideMenu);
    }, 0);
  });
}

/**
 * Setup clipboard functionality (copy/paste)
 * @private
 */
#setupClipboard() {
  // Create a hidden textarea for clipboard operations
  const clipboardHelper = document.createElement('textarea');
  clipboardHelper.className = 'wt-clipboard-helper';
  clipboardHelper.style.cssText = 'position:absolute;top:-1000px;left:-1000px;width:1px;height:1px;';
  document.body.appendChild(clipboardHelper);
  
  // Copy handler
  this.#grid.onKeyDown.subscribe((e) => {
    // Check for Ctrl+C
    if (e.which === 67 && (e.ctrlKey || e.metaKey)) {
      const ranges = this.#grid.getSelectionModel().getSelectedRanges();
      if (!ranges || !ranges.length) return;
      
      let copyText = '';
      const range = ranges[0]; // Take the first selection range
      
      // For each row in the selection
      for (let row = range.fromRow; row <= range.toRow; row++) {
        const dataItem = this.#dataView.getItem(row);
        if (!dataItem) continue;
        
        // For each column in the selection
        const rowValues = [];
        for (let col = range.fromCell; col <= range.toCell; col++) {
          const columnDef = this.#columns[col];
          if (!columnDef) continue;
          
          let value = dataItem[columnDef.field];
          
          // If column has a formatter, try to get raw value
          if (columnDef.copyFormatter) {
            value = columnDef.copyFormatter(row, col, value, columnDef, dataItem);
          }
          
          rowValues.push(value !== undefined && value !== null ? value : '');
        }
        
        copyText += rowValues.join('\t') + '\n';
      }
      
      // Copy to clipboard
      clipboardHelper.value = copyText;
      clipboardHelper.select();
      document.execCommand('copy');
      
      // Visual feedback
      //this.#showToast('Copied to clipboard');
    }
  });
}

/**
 * Setup advanced editing with validation
 * @private
 */
#setupAdvancedEditing() {
  // Handle edit validation
  this.#grid.onValidationError.subscribe((e, { editor, cellNode, column, validationResults }) => {
    const tooltip = document.createElement('div');
    tooltip.className = 'wt-validation-tooltip';
    tooltip.textContent = validationResults.msg;
    
    // Position tooltip near the cell
    const cellRect = cellNode.getBoundingClientRect();
    tooltip.style.top = `${cellRect.bottom + window.scrollY}px`;
    tooltip.style.left = `${cellRect.left + window.scrollX}px`;
    
    document.body.appendChild(tooltip);
    
    // Remove tooltip after a delay
    setTimeout(() => {
      tooltip.remove();
    }, 3000);
  });
  
  // Add validators to columns
  this.#columns.forEach(column => {
    if (this.#validators[column.field]) {
      column.validator = (value, item) => {
        const result = this.#validators[column.field](value, item);
        if (result === true) return { valid: true };
        return { valid: false, msg: result || 'Invalid value' };
      };
    }
  });
  
  // Update grid with modified columns
  this.#grid.setColumns(this.#columns);
}


/**
 * Setup tree grid functionality
 * @private
 */
#setupTreeGrid(options) {
  // Store tree options
  this.#treeOptions = {
    idField: options.idField || 'id',
    parentIdField: options.parentIdField || 'parentId',
    expandedField: options.expandedField || 'expanded',
    indentation: options.indentation || 15,
    ...options
  };
  
  // Add tree column formatter
  const treeColumn = this.#columns.find(col => col.field === options.treeField || col.id === options.treeField);
  if (!treeColumn) return;
  
  // Store original formatter if any
  const originalFormatter = treeColumn.formatter;
  
  // Create tree formatter
  treeColumn.formatter = (row, cell, value, columnDef, dataItem) => {
    // Calculate indent based on level
    const level = dataItem._level || 0;
    const indent = level * this.#treeOptions.indentation;
    
    // Determine if item has children
    const hasChildren = this.#dataView.getItems().some(item => 
      item[this.#treeOptions.parentIdField] === dataItem[this.#treeOptions.idField]
    );
    
    // Create toggle HTML
    let toggleHtml = '';
    if (hasChildren) {
      const expanded = dataItem[this.#treeOptions.expandedField];
      toggleHtml = `<span class="wt-tree-toggle ${expanded ? 'expanded' : 'collapsed'}" 
        style="margin-left:${indent}px" data-id="${dataItem[this.#treeOptions.idField]}"></span>`;
    } else {
      toggleHtml = `<span class="wt-tree-leaf" style="margin-left:${indent}px"></span>`;
    }
    
    // Format value with original formatter if available
    let formattedValue = value;
    if (originalFormatter) {
      formattedValue = originalFormatter(row, cell, value, columnDef, dataItem);
    }
    
    return toggleHtml + formattedValue;
  };
  
  // Handle toggle clicks
  this.#grid.onClick.subscribe((e, args) => {
    const target = e.target;
    if (target.classList.contains('wt-tree-toggle')) {
      const id = target.getAttribute('data-id');
      this.toggleTreeNode(id);
    }
  });
  
  // Update columns
  this.#grid.setColumns(this.#columns);
}



  /**
   * Update visibility of tree items based on expanded state
   * @private
   */
  #updateTreeVisibility() {
    if (!this.#treeOptions) return;
    
    const items = this.#dataView.getItems();
    
    // First pass: Mark all items with level and visible flags
    items.forEach(item => {
      item._level = 0;
      item._visible = true;
      
      // Find parent chain to calculate level
      let currentId = item[this.#treeOptions.parentIdField];
      let ancestors = [];
      
      while (currentId) {
        const parent = items.find(p => p[this.#treeOptions.idField] == currentId);
        if (!parent) break;
        
        ancestors.push(parent);
        currentId = parent[this.#treeOptions.parentIdField];
      }
      
      // Set level based on ancestor count
      item._level = ancestors.length;
      
      // Check if any ancestor is collapsed
      item._visible = !ancestors.some(ancestor => !ancestor[this.#treeOptions.expandedField]);
    });
    
    // Apply filter to show only visible items
    // Use efficient batch update
    this.#dataView.beginUpdate();
    this.#dataView.setFilter(function(item) {
      if (!item) return true;
      return item._visible !== false;
    });
    this.#dataView.endUpdate();  
    this.#dataView.refresh();
  }
  get grid() {
    return this.#grid;
  }
  get dataView() {
    return this.#dataView;
  }

  #handleRowDrop(evt) {
    const { oldIndex, newIndex } = evt;
    if (oldIndex === newIndex) return;
    const [moved] = this.#data.splice(oldIndex, 1);
    this.#data.splice(newIndex, 0, moved);
    this.#dataView.setItems(this.#data);
    this.#grid.invalidate();
    this.#grid.render();

    // BACKEND SIGNAL: Replace with:
    // APP.emit(this.#el, { name: "rowDropEvent", eventObject: evt, event: evt },
    //   newIndex, null, moved.id, 'application/json', null);

    this.#onRowDropEvent?.(newIndex, null, moved.id, 'application/json', null);
  }
  #handleCellDrop(evt, rowIdx, columnId, sourceId, mimeType) {
    // BACKEND SIGNAL:
    // APP.emit(
    //   this.#el,
    //   { name: "dropEvent", eventObject: evt, event: evt },
    //   rowIdx,
    //   columnId,
    //   sourceId,
    //   mimeType
    // );
    this.#onDropEvent?.(rowIdx, columnId, sourceId, mimeType);
  }
  #evaluateFilter(item, filter) {
    // Handle nested filter groups
    if (filter.filters) {
      const results = filter.filters.map(f => this.#evaluateFilter(item, f));
      return filter.condition === 'or' 
        ? results.some(Boolean) 
        : results.every(Boolean);
    }
    
    // Get item value
    const field = filter.column;
    let value = item[field];
    
    // Apply operator
    switch (filter.operator) {
      case 'equals':
        return value == filter.value;
      case 'notEquals':
        return value != filter.value;
      case 'contains':
        return String(value).toLowerCase().includes(String(filter.value).toLowerCase());
      case 'notContains':
        return !String(value).toLowerCase().includes(String(filter.value).toLowerCase());
      case 'startsWith':
        return String(value).toLowerCase().startsWith(String(filter.value).toLowerCase());
      case 'endsWith':
        return String(value).toLowerCase().endsWith(String(filter.value).toLowerCase());
      case 'greaterThan':
        return value > filter.value;
      case 'greaterThanOrEqual':
        return value >= filter.value;
      case 'lessThan':
        return value < filter.value;
      case 'lessThanOrEqual':
        return value <= filter.value;
      case 'inRange':
        return value >= filter.value[0] && value <= filter.value[1];
      case 'empty':
        return value === null || value === undefined || value === '';
      case 'notEmpty':
        return value !== null && value !== undefined && value !== '';
      default:
        return true;
    }
  }
   // ───────── Public API for Advanced Features ─────────
  
  /**
   * Set row grouping
   * @param {Object} grouping - Grouping configuration
   * @param {string|function} grouping.getter - Field to group by or function
   * @param {function} grouping.formatter - Group row formatter
   * @param {Array} grouping.aggregators - Array of aggregators
   * @param {boolean} grouping.collapsed - Whether groups start collapsed
   */
  setGrouping(grouping) {
    if (!grouping) {
      this.#dataView.setGrouping([]);
      return this;
    }
    
    this.#dataView.setGrouping({
      getter: grouping.getter,
      formatter: grouping.formatter || (g => `${g.value} (${g.count} items)`),
      aggregators: grouping.aggregators || this.#aggregators || [],
      collapsed: grouping.collapsed,
      lazyTotalsCalculation: true
    });
    
    return this;
  }
  
  /**
   * Expand all groups
   */
  expandAllGroups() {
    this.#dataView.expandAllGroups();
    return this;
  }
  
  /**
   * Collapse all groups
   */
  collapseAllGroups() {
    this.#dataView.collapseAllGroups();
    return this;
  }
  
  /**
   * Toggle a specific group
   * @param {string} groupValue - The value of the group to toggle
   */
  toggleGroup(groupValue) {
    const groups = this.#dataView.getGroups();
    const group = groups.find(g => g.value === groupValue);
    
    if (group) {
      if (group.collapsed) {
        this.#dataView.expandGroup(group.value);
      } else {
        this.#dataView.collapseGroup(group.value);
      }
    }
    
    return this;
  }
  
  /**
   * Set frozen columns
   * @param {number} columnCount - Number of columns to freeze (-1 to disable)
   */
  setFrozenColumns(columnCount) {
    this.#frozenColumns = columnCount;
    this.#grid.setOptions({ frozenColumn: columnCount });
    return this;
  }
  
  /**
   * Set frozen rows
   * @param {number} rowCount - Number of rows to freeze (0 to disable)
   */
  setFrozenRows(rowCount) {
    this.#frozenRows = rowCount;
    this.#grid.setOptions({ frozenRow: rowCount });
    return this;
  }
  
  /**
   * Show/hide header filter row
   * @param {boolean} show - Whether to show the header row
   */
  showHeaderRow(show) {
    this.#showHeaderRow = show;
    this.#grid.setOptions({ showHeaderRow: show });
    if (show) {
      this.#setupHeaderRowFilters();
    }
    return this;
  }
  
  /**
   * Show/hide footer aggregate row
   * @param {boolean} show - Whether to show the footer row
   */
  showFooterRow(show) {
    this.#showFooterRow = show;
    this.#grid.setOptions({ createFooterRow: show });
    if (show) {
      this.#setupFooterRowAggregates();
      this.#updateFooterRow();
    }
    return this;
  }
  
  /**
   * Update column groups
   * @param {Array} columnGroups - Array of column group definitions
   */
  setColumnGroups(columnGroups) {
    this.#columnGroups = columnGroups;
    
    // Remove existing column groups
    const existingGroupsEl = this.#el.querySelector('.slick-column-groups');
    if (existingGroupsEl) {
      existingGroupsEl.remove();
    }
    
    // Setup new column groups
    if (columnGroups && columnGroups.length) {
      this.#setupColumnGroups();
    }
    
    return this;
  }

  /**
   * Set or update context menu
   * @param {Object} contextMenu - Context menu configuration
   */
  setContextMenu(contextMenu) {
    return this.#setupContextMenu(contextMenu);
  }
  /**
   * Enable or disable clipboard functionality
   */
  enableClipboard(enable = true) {
    if (enable && !this._clipboardEnabled) {
      this.#setupClipboard();
      this._clipboardEnabled = true;
    }
    return this;
  }

  /**
   * Add or update a validator for a column
   * @param {string} field - The column field to validate
   * @param {Function} validator - Validator function returning true or error message
   */
  setValidator(field, validator) {
    if (!field || typeof validator !== 'function') return this;
    
    this.#validators[field] = validator;
    
    // Update column validator
    const column = this.#columns.find(col => col.field === field);
    if (column) {
      column.validator = (value, item) => {
        const result = validator(value, item);
        if (result === true) return { valid: true };
        return { valid: false, msg: result || 'Invalid value' };
      };
    }
    
    // Update grid
    this.#grid.setColumns(this.#columns);
    return this;
  }

  /**
   * Toggle the expanded state of a tree node
   * @param {string|number} id - The ID of the node to toggle
   */
  toggleTreeNode(id) {
    if (!this.#treeOptions) return this;
    
    const items = this.#dataView.getItems();
    const item = items.find(item => item[this.#treeOptions.idField] == id);
    if (!item) return this;
    
    // Toggle expanded state
    item[this.#treeOptions.expandedField] = !item[this.#treeOptions.expandedField];
    
    // Update visibility of child items
    this.#updateTreeVisibility();
    
    return this;
  }
  /**
   * Enable tree grid functionality
   * @param {Object} options - Tree grid options
   */
  enableTreeGrid(options) {
    if (!options) return this;
    this.#setupTreeGrid(options);
    this.#updateTreeVisibility();
    return this;
  }

  // ───────── public API ─────────
  refresh() {
    this.#grid.resizeCanvas();
    this.#grid.invalidate();
    this.#grid.updateRowCount();
    this.#grid.render();
  }

  reset() {
    this.#data = [];
    this.#cursor = null;
    this.#done = false;
    this.#dataView.setItems([]);
    this.#grid.invalidate();
    this.#grid.updateRowCount();
    this.#grid.render();
  }
    
  /**
   * Update a single item and highlight changes
   * @param {Object} item - The item to update (must have an id property)
   * @param {Object} options - Highlighting options
   */
  updateItem(item, options = {}) {
    if (!item || !item.id) {
      console.error('WTableView: updateItem requires an item with id property');
      return;
    }
    
    // Find the existing item
    const existingIndex = this.#data.findIndex(d => d.id === item.id);
    if (existingIndex === -1) {
      // Item doesn't exist, add it
      this.#data.push(item);
      this.#dataView.setItems(this.#data);
      return;
    }
    
    // Get the existing item for comparison
    const existingItem = this.#data[existingIndex];
    
    // Update the item in our data array and dataView
    this.#data[existingIndex] = item;
    this.#dataView.updateItem(item.id, item);
    
    // Highlight changed fields
    if (options.highlight !== false) {
      this.#highlightChanges(existingItem, item, options.highlightDuration || 200);
    }
  }

  /**
   * Update multiple items at once
   * @param {Array} items - Array of items to update
   * @param {Object} options - Highlighting options
   */
  updateItems(items, options = {}) {
    if (!Array.isArray(items)) {
      console.error('WTableView: updateItems requires an array of items');
      return;
    }
    
    // Begin batch updates to improve performance
    this.#dataView.beginUpdate();
    
    items.forEach(item => this.updateItem(item, options));
    
    // End batch updates
    this.#dataView.endUpdate();
  }

  /**
   * Highlight changes between old and new versions of an item
   * @private
   */
  #highlightChanges(oldItem, newItem, duration) {
    if (!oldItem || !newItem) return;
    
    // Get the row index for this item
    const row = this.#dataView.getItems().findIndex(item => item.id === newItem.id);
    if (row === undefined) return;
    
    // Track which fields have changed
    const changes = {};
    let hasChanges = false;
    
    // Compare each field that has a corresponding column
    this.#columns.forEach(col => {
      const field = col.field;
      if (!field) return;
      
      // Skip if values are the same or undefined
      if (oldItem[field] === newItem[field] || 
          oldItem[field] === undefined || 
          newItem[field] === undefined) return;
      
      // Determine if the change is positive or negative (for number fields)
      let cssClass = 'wt-changed';
      if (typeof newItem[field] === 'number' && typeof oldItem[field] === 'number') {
        cssClass = newItem[field] > oldItem[field] ? 'wt-changed-gain' : 'wt-changed-loss';
      }
      
      // Add to changes object
      changes[col.id] = cssClass;
      hasChanges = true;
    });
    
    // Apply highlighting if we have changes
    if (hasChanges) {
      const hash = { [row]: changes };
      const highlightId = `highlight_${newItem.id}_${Date.now()}`;
      this.#grid.setCellCssStyles(highlightId, hash);
      
      // Remove highlight after duration
      setTimeout(() => {
        this.#grid.removeCellCssStyles(highlightId);
      }, duration);
    }
  }
  /**
   * Toggle expand/collapse all groups
   * @param {boolean} expand - Whether to expand (true) or collapse (false)
   */
  toggleGrouping(expand) {
    const groupToggleAllElm = document.querySelector(".slick-group-toggle-all");
    
    if (expand) {
      this.#dataView.expandAllGroups();
      if (groupToggleAllElm) {
        groupToggleAllElm.classList.remove('collapsed');
        groupToggleAllElm.classList.add('expanded'); // Fixed: addClass → add
      }
    } else {
      this.#dataView.collapseAllGroups();
      if (groupToggleAllElm) {
        groupToggleAllElm.classList.remove('expanded');
        groupToggleAllElm.classList.add('collapsed'); // Fixed: addClass → add
      }
    }
    
    return this;
  }
  /**
   * Group by a specific column
   * @param {string} columnName - Field name to group by
   * @param {Object} options - Optional grouping options
   */
  groupByColumn(columnName, options = {}) {
    this.#grid.setSortColumns([]);
    
    this.#dataView.setGrouping({
      getter: columnName,
      formatter: options.formatter || function(g) {
        return `${columnName.replace(/(^\w|\s\w)/g, m => m.toUpperCase())}: ${g.value}  <span style='color:#029eb7'>(${g.count} items)</span>`;
      },
      aggregators: options.aggregators || [
        new Aggregators.Sum("amount")
      ],
      aggregateCollapsed: options.aggregateCollapsed !== false,
      collapsed: options.collapsed !== false,
      lazyTotalsCalculation: true
    });
    
    this.#grid.invalidate();
    return this;
  }
  removeColumnById(array, idVal) {
    return array.filter(function (el, i) {
      return el.id !== idVal;
    });
  }
  /**
   * Set filter for the grid
   * @param {Function|Object} filter - Filter function or filter specification
   * @param {Object} args - Optional filter arguments
   */
  setFilter(filter, args = {}) {
    this.#dataView.beginUpdate();
    
    if (typeof filter === 'function') {
      // Direct filter function
      this.#dataView.setFilter(filter);
    } else if (filter && typeof filter === 'object') {
      // Object-based filter specification
      this.#dataView.setFilter((item) => this.#evaluateFilter(item, filter));
    } else {
      // Clear filter
      this.#dataView.setFilter(null);
    }
    
    this.#dataView.setFilterArgs(args);
    this.#dataView.endUpdate();
    
    return this;
  }
  /**
   * Apply filter to columns directly using built-in functionality
   */
  applyColumnFilters(columnFilters) {
    this.#dataView.beginUpdate();
    this.#dataView.setFilterArgs({ columnFilters });
    
    if (!this.#dataView.getFilter()) {
      this.#dataView.setFilter((item, args) => {
        if (!args || !args.columnFilters) return true;
        
        for (const field in args.columnFilters) {
          const filterVal = args.columnFilters[field].toLowerCase();
          const itemVal = String(item[field] || '').toLowerCase();
          
          if (filterVal && !itemVal.includes(filterVal)) {
            return false;
          }
        }
        return true;
      });
    }
    
    this.#dataView.endUpdate();
    return this;
  }
  /**
   * Set multi-level grouping
   * @param {Array} groupingLevels - Array of grouping configurations
   */
  setMultiLevelGrouping(groupingLevels) {
    if (!Array.isArray(groupingLevels) || !groupingLevels.length) {
      this.#dataView.setGrouping([]);
      return this;
    }
    
    this.#grid.setSortColumns([]);
    this.#dataView.setGrouping(groupingLevels);
    this.#grid.invalidate();
    
    return this;
  }

  /**
   * Group by column with custom sort order
   * @param {string} columnName - Field name to group by
   * @param {Function} comparer - Custom function to sort groups
   * @param {Object} options - Optional grouping options
   */
  groupByColumnCustomSort(columnName, comparer, options = {}) {
    this.#grid.setSortColumns([]);
    
    this.#dataView.setGrouping({
      getter: columnName,
      formatter: options.formatter || function(g) {
        return `${columnName.replace(/(^\w|\s\w)/g, m => m.toUpperCase())}: ${g.value} (${g.count} items)`;
      },
      comparer: comparer,
      aggregators: options.aggregators || [
        new Aggregators.Sum("amount")
      ],
      aggregateCollapsed: options.aggregateCollapsed !== false,
      collapsed: options.collapsed !== false,
      lazyTotalsCalculation: true
    });
    
    this.#grid.invalidate();
    return this;
  }
  /**
   * Add or update items in the DataView using different strategies
   * @public
   * @param {Array} items - Items to add or update
   * @param {string} strategy - Strategy to use: 'addUnique', 'individualUpdate', or 'batchUpdate'
   * @returns {number} - Number of items processed
   */
  processItems(items, strategy = 'addUnique') {
    if (!items || !items.length) return 0;
    
    switch (strategy) {
      case 'addUnique': {
        // Strategy 1: Only add items with unique IDs
        const uniqueItems = items.filter(item => {
          if (this.#existingIds.has(item.id)) {
            return false; // Skip existing items
          }
          this.#existingIds.add(item.id); // Add to set to prevent duplicates
          return true;
        }); 

        if (uniqueItems.length > 0) {
          this.#dataView.addItems(uniqueItems);
        }
        return uniqueItems.length;
      }
      
      case 'individualUpdate': {
        // Strategy 2: Use DataView's built-in update functionality
        let count = 0;
        items.forEach(item => {
          const existingItem = this.#dataView.getItemById(item.id);
          if (existingItem) {
            this.#dataView.updateItem(item.id, item); // Update if exists
          } else {
            this.#dataView.addItem(item); // Add if new
            //this.#existingIds.add(item.id);
          }
          count++;
        });
        return count;
      }
      
      case 'batchUpdate': {
        // Strategy 3: Use map-based approach for bulk operations
        this.#dataView.beginUpdate();
        let count = 0;
        items.forEach(item => {
          // Either add new or update existing
          if (this.#dataView.getItemById(item.id)) {
            this.#dataView.updateItem(item.id, item);
          } else {
            this.#dataView.addItem(item);
            //this.#existingIds.add(item.id);
          }
          count++;
        });
        this.#dataView.endUpdate();
        return count;
      }
      
      default:
        console.warn(`WTableView: Unknown data processing strategy '${strategy}', using 'addUnique'`);
        return this.processItems(items, 'addUnique');
    }
  }
  removeSortColumnById(array, idVal) {
    return array.filter(function (el, i) {
      return el.columnId !== idVal;
    });
  }
  /** Replace the dataUrl (e.g. when Wt sessionId changes) and optionally refresh. */
  setDataUrl(newUrlOrFn, autoRefresh = true) {
    this.#dataUrl = newUrlOrFn;
    if (autoRefresh) this.refresh();
  }

  scrollTo(x, y) {
    this.#grid.getContainerNode().scrollLeft = x;
    this.#grid.getContainerNode().scrollTop  = y;
  }

  resizeColumn(columnId, delta) {
    const col = this.#columns.find(c => c.id === columnId);
    if (!col) return;
    col.width = Math.max(20, (col.width || 80) + delta);
    this.#grid.setColumns(this.#columns);
    this.#onColumnResized?.(columnId, col.width);
  }

  destroy() {
    this.#resizeObserver.disconnect();
    this.#grid.destroy();
  }
}
