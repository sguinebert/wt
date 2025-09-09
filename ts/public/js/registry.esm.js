export default class WWidgetRegistry {
  #widgets = new Map();
  #observer = null;
  
  constructor() {
    // Create MutationObserver to track DOM changes
    this.#observer = new MutationObserver(muts => {
      muts.forEach(m => {
        if (m.type === 'attributes' && m.attributeName === 'data-wt') {
          this.registerElement(m.target);
        }
        if (m.type === 'childList') {
          m.addedNodes.forEach(n => n.nodeType === 1 && n.dataset.wt && this.registerElement(n));
          m.removedNodes.forEach(n => n.nodeType === 1 && n.dataset.wt && this.unregisterElement(n));
        }
      });
    })
    
    // Start observing the DOM
    this.#observer.observe(document.body, {
      childList: true,
      subtree: true,
      attributeFilter: ['data-wt'] // Observe changes to data-wt attributes
    });
    
    // Initial scan of existing widgets
    this.scanDOM();
  }
  
  /**
   * Register a DOM element as a widget
   * @param {Element} element - The widget element
   */
  registerElement(element) {
        // Ensure element has an ID
    if (!element.id) {
      throw new Error('Element must have an ID to be registered as a widget');
    }
    const id = element.id;
    
    // Determine widget type
    let type = element.dataset.wt;
    
    // Store widget info
    this.#widgets.set(id, {
      element,
      type,
      parent: null,
      children: [],
      jsObject: null // Will be set later when JS wrapper is created
    });
    
    // Update parent-child relationships
    this.#updateRelationships(element);
    
    return id;
  }
  
  /**
   * Associate a JS object with an element
   * @param {string} id - The element ID
   * @param {Object} jsObject - The JS object
   */
  associateJsObject(id, jsObject) {
    const widgetInfo = this.#widgets.get(id);
    if (widgetInfo) {
      widgetInfo.jsObject = jsObject;
    }
  }
  
  /**
   * Unregister a widget element
   * @param {Element} element - The widget element
   */
  unregisterElement(element) {
    const id = element.id;
    if (!id || !this.#widgets.has(id)) return;
    
    const widgetInfo = this.#widgets.get(id);
    
    // Remove from parent's children list
    if (widgetInfo.parent && this.#widgets.has(widgetInfo.parent)) {
      const parentInfo = this.#widgets.get(widgetInfo.parent);
      parentInfo.children = parentInfo.children.filter(childId => childId !== id);
    }
    
    // Remove from registry
    this.#widgets.delete(id);
  }
  
  /**
   * Update parent-child relationships for an element
   * @param {Element} element - The element to update
   */
  #updateRelationships(element) {
    const id = element.id;
    if (!id || !this.#widgets.has(id)) return;
    
    // Find parent widget
    let parent = element.parentElement;
    while (parent && (!parent.id || !this.#widgets.has(parent.id))) {
      parent = parent.parentElement;
    }
    
    if (parent && parent.id) {
      // Update parent reference
      this.#widgets.get(id).parent = parent.id;
      
      // Add to parent's children list
      const parentInfo = this.#widgets.get(parent.id);
      if (parentInfo && !parentInfo.children.includes(id)) {
        parentInfo.children.push(id);
      }
    }
  }
  
  /**
   * Scan the DOM for existing widgets
   */
  scanDOM() {
    // Clear existing registry
    this.#widgets.clear();
    
    // Find all potential widget elements
    const elements = document.querySelectorAll('[data-wt]');
    elements.forEach(element => {
      //if (this.#isWidgetElement(element)) {
        this.registerElement(element);
      //}
    });
  }
  
  /**
   * Get all registered widgets
   * @returns {Array} Array of widget information
   */
  getAllWidgets() {
    return Array.from(this.#widgets.entries()).map(([id, info]) => ({
      id,
      type: info.type,
      element: info.element,
      jsObject: info.jsObject,
      parent: info.parent,
      children: info.children
    }));
  }
  
  /**
   * Get widget by ID
   * @param {string} id - Widget ID
   * @returns {Object|null} Widget information
   */
  getWidget(id) {
    return this.#widgets.get(id) || null;
  }
  
  /**
   * Get widgets by type
   * @param {string} type - Widget type
   * @returns {Array} Array of widget information
   */
  getWidgetsByType(type) {
    return this.getAllWidgets().filter(widget => widget.type === type);
  }
  
  /**
   * Get widget tree
   * @returns {Object} Widget tree
   */
  getWidgetTree() {
    // Find root widgets (those without parents)
    const roots = this.getAllWidgets().filter(widget => !widget.parent);
    
    // Build tree
    const buildTree = (widget) => {
      return {
        id: widget.id,
        type: widget.type,
        children: widget.children.map(childId => {
          const childWidget = this.getWidget(childId);
          return childWidget ? buildTree(childWidget) : null;
        }).filter(Boolean)
      };
    };
    
    return roots.map(buildTree);
  }
  
  /**
   * Destroy the registry
   */
  destroy() {
    this.#observer.disconnect();
    this.#widgets.clear();
  }
}

// Singleton instance
let instance = null;

/**
 * Get the widget registry instance
 * @returns {WWidgetRegistry} The widget registry
 */
export function getWidgetRegistry() {
  if (!instance) {
    instance = new WWidgetRegistry();
  }
  return instance;
}
