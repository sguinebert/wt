/*  flex-layout.js  ▸  Modern ES module with clean structure  */
export default class FlexLayout {
  // Private fields
  #el;
  #ro;
  #mo;
  #onRemoved;
  #pending = false;

  /**
   * @param {HTMLElement|string} container  DOM element or selector
   * @param {(HTMLElement)=>void=} onRemovedCallback  Optional C++ bridge
   */
  constructor(container, onRemovedCallback) {
    this.#el = typeof container === 'string' ? document.getElementById(container) : container;
    if (!this.#el) throw new Error('FlexLayout: container not found');

    this.#onRemoved = onRemovedCallback;
    
    // Initialize observers and first layout
    this.#setupObservers();
    this.allocate();
  }

  /**
   * Configure both observers at once
   */
  #setupObservers() {
    // ResizeObserver for child size changes
    this.#ro = new ResizeObserver(() => this.allocate());
    this.#refreshResizeObservers();
    
    // MutationObserver for DOM/attribute changes
    this.#mo = new MutationObserver(mutations => {
      // Track removed nodes
      const removed = mutations.flatMap(m => [...m.removedNodes]);
      
      // Schedule removal notifications
      if (removed.length) {
        queueMicrotask(() => this.#notifyRemoved(removed));
      }
      
      // Debounced refresh
      if (!this.#pending) {
        this.#pending = true;
        queueMicrotask(() => {
          this.#pending = false;
          this.refresh();
        });
      }
    });
    
    this.#mo.observe(this.#el, {
      childList: true,
      subtree: false,
      attributes: true,
      attributeFilter: ['class', 'style', 'flg']
    });
  }

  /**
   * Notify about removed elements
   */
  #notifyRemoved(nodes) {
    for (const node of nodes) {
      if (node.nodeType !== Node.ELEMENT_NODE) continue;
      
      this.#el.dispatchEvent(new CustomEvent('flex-layout:removed', { detail: node }));
      this.#onRemoved?.(node); // Optional chaining for callback
    }
  }

  /**
   * Get visible children
   */
  #visibleChildren() {
    return [...this.#el.children].filter(node => 
      node.nodeType === Node.ELEMENT_NODE && 
      node.style.display !== 'none' && 
      !node.classList.contains('out')
    );
  }

  /**
   * Update resize observer targets
   */
  #refreshResizeObservers() {
    this.#ro.disconnect();
    this.#visibleChildren().forEach(child => this.#ro.observe(child));
  }

  /**
   * Calculate and apply flex-grow values
   */
  allocate() {
    const children = this.#visibleChildren();
    const growable = children.filter(child => child.getAttribute('flg') !== '0');
    
    // Calculate total grow factor (or use equal distribution)
    const totalGrow = growable.reduce(
      (sum, child) => sum + (parseFloat(getComputedStyle(child).flexGrow) || 1), 
      0
    ) || growable.length;
    
    // Apply calculated flex-grow values
    children.forEach(child => {
      if (child.getAttribute('flg') === '0') {
        child.style.flexGrow = '0';
      } else {
        const growFactor = parseFloat(getComputedStyle(child).flexGrow) || 1;
        child.style.flexGrow = (growFactor / totalGrow).toString();
      }
    });
  }

  /**
   * Public API: Refresh layout
   */
  refresh() {
    this.#refreshResizeObservers();
    this.allocate();
  }

  /**
   * Public API: Clean up
   */
  destroy() {
    this.#ro?.disconnect();
    this.#mo?.disconnect();
  }
  
  /**
   * Getter for the container element
   */
  get el() {
    return this.#el;
  }
}