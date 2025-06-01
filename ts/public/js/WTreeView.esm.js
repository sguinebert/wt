/* ------------------------------------------------------------------
 * WTreeView - High-performance tree component with virtualization
 * Extends WTableView to provide specialized tree functionality.
 *
 * Tree Components
    ├── WTree (simple, basic tree)
    │   ├── Basic hierarchy display
    │   ├── Expand/collapse
    │   ├── Icons & checkboxes
    │   └── Simple selection
    │
    └── WTreeView (complex, SlickGrid-based)
        ├── All WTree features
        ├── Column management
        ├── Virtualization
        ├── Advanced sorting/filtering
        └── Complex drag & drop
 * 
 *  ▸ no globals, no WT macros – a self-contained class
 *  ▸ pointer / wheel friendly, ResizeObserver aware
 *  ▸ event forwarding through `CustomEvent`s
 *  ▸ optional drag-&-drop helpers (item + between-rows)
 * Features:
 * - Efficient virtualization for large trees
 * - Expand/collapse functionality
 * - Custom icons and styling
 * - Selection with checkboxes (optional)
 * - Drag and drop support (optional)
 * 
 * const treeView = new WTreeView({
    el: '#tree-container',
    checkboxes: true,
    columns: [
      { id: 'name', name: 'Name', field: 'name', width: 220 },
      { id: 'size', name: 'Size', field: 'size', width: 80 }
    ],
    indentation: 16,
    dragDrop: true
  });

  // Load data with parent-child relationships
  treeView.setData([
    { id: 1, name: "Root 1", size: "20KB" },
    { id: 2, name: "Child 1.1", parentId: 1, size: "10KB" },
    { id: 3, name: "Child 1.2", parentId: 1, size: "5KB" },
    { id: 4, name: "Root 2", size: "15KB" },
    { id: 5, name: "Child 2.1", parentId: 4, size: "7KB" }
  ]);

  // Expand all nodes
  treeView.expandAll();

  // Listen for selection changes
  document.querySelector('#tree-container').addEventListener('selectionchanged', e => {
    console.log('Selected items:', e.detail.selected);
  });
 * ------------------------------------------------------------------ */
import WTableView from './WTableView.esm.js';

export default class WTreeView extends WTableView {
  // Private fields using proper # syntax
  #cascadeSelection = false;
  #defaultIcons = {
    leaf: "<span>&#128441;</span>",
    parent: "<span>&#128449;</span>",
    expanded: "<span>&#9698;</span>",
    collapsed: "<span>&#9654;</span>"
  };

  /**
   * @param {Object} config - Configuration object
   * @param {string|HTMLElement} config.el - Container element or selector
   * @param {Array<Object>} config.data - Initial tree data (optional)
   * @param {string|Function} config.dataUrl - URL or function for loading data
   * @param {Array<Object>} config.columns - Column definitions
   * @param {boolean} config.checkboxes - Whether to show checkboxes (default: false)
   * @param {string} config.idField - ID field name (default: 'id')
   * @param {string} config.parentIdField - Parent ID field name (default: 'parentId')
   * @param {string} config.expandedField - Expanded state field name (default: 'expanded')
   * @param {string} config.iconField - Field containing custom icon (default: 'icon')
   * @param {number} config.indentation - Indentation per level (default: 16)
   * @param {boolean} config.dragDrop - Enable drag and drop (default: false)
   */
  constructor(config) {
    // Set default tree column if not provided
    if (!config.columns || !config.columns.length) {
      config.columns = [{
        id: 'tree',
        name: 'Tree',
        field: 'name',
        width: 220
      }];
    }
    
    // Ensure first column has tree formatting
    const treeField = config.columns[0].field;
    
    // Setup tree configuration
    const treeOptions = {
      idField: config.idField || 'id',
      parentIdField: config.parentIdField || 'parentId',
      expandedField: config.expandedField || 'expanded',
      treeField: treeField,
      indentation: config.indentation || 16,
      iconField: config.iconField || 'icon'
    };
    
    // Add checkbox column if requested
    if (config.checkboxes) {
      config.columns.unshift({
        id: 'checkbox',
        name: '',
        field: '_checkbox',
        width: 40,
        formatter: (_row, _cell, _value, _columnDef, dataItem) => {
          const checked = dataItem._selected ? 'checked' : '';
          return `<input type="checkbox" ${checked}>`;
        }
      });
    }
    
    // Configure custom tree formatter for the main column
    config.columns = config.columns.map(col => {
      if (col.field === treeField) {
        const originalFormatter = col.formatter;
        col.formatter = (row, cell, value, columnDef, dataItem) => {
          // Get the tree formatting from the grid
          let treeHtml = '';
          
          // Calculate indent based on level
          const level = dataItem._level || 0;
          const indent = level * treeOptions.indentation;
          
          // Determine if item has children
          const hasChildren = dataItem._hasChildren || false;
          
          // Create toggle HTML
          if (hasChildren) {
            const expanded = dataItem[treeOptions.expandedField];
            treeHtml = `<span class="wt-tree-toggle ${expanded ? 'expanded' : 'collapsed'}" 
              style="margin-left:${indent}px" data-id="${dataItem[treeOptions.idField]}"></span>`;
          } else {
            treeHtml = `<span class="wt-tree-leaf" style="margin-left:${indent}px"></span>`;
          }
          
          // Add custom icon if available
          if (dataItem[treeOptions.iconField]) {
            treeHtml += `<span class="wt-tree-icon">${dataItem[treeOptions.iconField]}</span>`;
          }
          
          // Format value with original formatter if available
          let formattedValue = value;
          if (originalFormatter) {
            formattedValue = originalFormatter(row, cell, value, columnDef, dataItem);
          }
          
          return treeHtml + formattedValue;
        };
      }
      return col;
    });
    
    // Call parent constructor with merged configuration
    super({
      ...config,
      treeOptions
    });
    
    // Set up checkbox handling if enabled
    if (config.checkboxes) {
      this.#setupCheckboxes();
    }
    
    // Set up drag and drop if enabled
    if (config.dragDrop) {
      this.#setupDragDrop();
    }
  this.#setupTreeFormatting();
  this.#setupTreeFiltering();
  this.setupTreeSorting();
    this.#setupToggleHandlers();
  }
  // Add to your WTreeView constructor or add as a method
#setupTreeFormatting() {
  // Add necessary CSS styles
  const styleEl = document.createElement('style');
  styleEl.textContent = `
    .wt-tree-toggle {
      cursor: pointer;
      display: inline-block;
      width: 16px;
      height: 16px;
      text-align: center;
      line-height: 16px;
    }
    .wt-tree-toggle.expanded:before {
      content: "▼";
      font-size: 10px;
    }
    .wt-tree-toggle.collapsed:before {
      content: "►";
      font-size: 10px;
    }
  `;
  document.head.appendChild(styleEl);
}
  // Add this method to WTreeView
#setupTreeFiltering() {
  const treeOptions = this.getTreeOptions();
  const dataView = this.dataView;
  
  // Set up the tree filter function
  dataView.setFilter(item => {
    if (!item) return true;
    
    // If this item has no parent, always show it
    if (!item[treeOptions.parentIdField]) return true;
    
    // Find parent chain and check if any are collapsed
    let currentParentId = item[treeOptions.parentIdField];
    let parentVisible = true;
    
    while (currentParentId && parentVisible) {
      const parent = dataView.getItemById(currentParentId);
      if (!parent) break;
      
      // If any parent is collapsed, hide this item
      if (!parent[treeOptions.expandedField]) {
        parentVisible = false;
      }
      
      // Move up to next parent
      currentParentId = parent[treeOptions.parentIdField];
    }
    
    return parentVisible;
  });
}
  /**
   * Initialize the tree with data
   * @param {Array} data - Tree data
   */
  setData(data) {
    // Pre-process data to add tree-specific properties
    const processedData = this.#preprocessData(data);
    
    // Update the data view (accessing parent's protected methods via public API)
    const dataView = this.dataView;
    dataView.beginUpdate();
    dataView.setItems(processedData);
    this.refreshTreeView(); // Updates visibility based on expanded state
    dataView.endUpdate();
    
    return this;
  }
  

  
  /**
   * Pre-process data to add necessary tree properties
   * @private
   */
  #preprocessData(data) {
    if (!data || !data.length) return [];
    
    // Clone the data to avoid modifying originals
    const processedData = JSON.parse(JSON.stringify(data));
    
    // Add _hasChildren flag to each item
    const idField = this.getTreeOptions().idField;
    const parentIdField = this.getTreeOptions().parentIdField;
    
    // First pass - create a map of all IDs
    const idMap = new Map();
    processedData.forEach(item => {
      idMap.set(item[idField], true);
    });
    
    // Second pass - mark items with children
    processedData.forEach(item => {
      // Count how many items have this as parent
      let hasChildren = false;
      for (const potentialChild of processedData) {
        if (potentialChild[parentIdField] === item[idField]) {
          hasChildren = true;
          break;
        }
      }
      item._hasChildren = hasChildren;
      
      // Set default expanded state if not specified
      if (typeof item[this.getTreeOptions().expandedField] === 'undefined') {
        item[this.getTreeOptions().expandedField] = false;
      }
    });
    
    return processedData;
  }
  
  /**
   * Get tree options
   * @private
   */
  getTreeOptions() {
    // Another helper to access private fields from parent
    // In a real implementation, you might need to store a local copy
    return {
      idField: 'id',
      parentIdField: 'parentId',
      expandedField: 'expanded',
      indentation: 16,
      iconField: 'icon'
    };
  }
  
  /**
   * Setup checkbox handling
   * @private
   */
  #setupCheckboxes() {
    this.grid.onClick.subscribe((e, args) => {
      if (e.target.type === 'checkbox') {
        const dataView = this.dataView;
        const item = dataView.getItem(args.row);
        if (!item) return;
        
        // Toggle selection state
        item._selected = !item._selected;
        
        // Update children recursively if needed
        if (item._hasChildren && this.#cascadeSelection) {
          this.#applySelectionToChildren(item, item._selected);
        }
        
        // Update the UI
        dataView.updateItem(item[this.getTreeOptions().idField], item);
        
        // Trigger selection changed event
        this.#triggerSelectionChanged();
      }
    });
  }
  
  /**
   * Recursively apply selection to children
   * @private
   */
  #applySelectionToChildren(parent, selected) {
    const dataView = this.dataView;
    const items = dataView.getItems();
    const parentId = parent[this.getTreeOptions().idField];
    
    items.forEach(item => {
      if (item[this.getTreeOptions().parentIdField] === parentId) {
        item._selected = selected;
        
        // Recurse to children
        if (item._hasChildren) {
          this.#applySelectionToChildren(item, selected);
        }
        
        dataView.updateItem(item[this.getTreeOptions().idField], item);
      }
    });
  }
  /**
   * Setup tree node toggle functionality
   * @private
   */
  #setupToggleHandlers() {
    this.grid.onClick.subscribe((e, args) => {
      console.log('Click event:', e, args);
      // Check if the click was on a toggle element
      if (e.target.classList.contains('wt-tree-toggle') || 
          e.target.parentElement.classList.contains('wt-tree-toggle')) {
        
        // Get the node ID from the data attribute
        const toggleEl = e.target.classList.contains('wt-tree-toggle') ? 
                        e.target : e.target.parentElement;
        const nodeId = toggleEl.getAttribute('data-id');
        
        if (nodeId) {
          // Toggle the node expanded state
          this.toggleNode(nodeId);
          e.stopPropagation(); // Prevent other click handlers
        }
      }
    });
  }
  /**
   * Trigger selection changed event
   * @private
   */
  #triggerSelectionChanged() {
    const selectedItems = this.getSelectedItems();
    console.log('Selection changed:', selectedItems);
    // document.querySelector(this.el).dispatchEvent(new CustomEvent('selectionchanged', {
    //   detail: { selected: selectedItems }
    // }));
  }
  
  /**
   * Setup drag and drop functionality
   * @private
   */
  #setupDragDrop() {
    // Implementation would leverage existing drag/drop in WTableView
    // with tree-specific behaviors for parent/child relationships
  }
  
  /**
   * Public method to refresh tree view after changes
   */
  refreshTreeView() {
    // Call the parent's private method via the public API
    // This would normally be #updateTreeVisibility() in the parent
    this.enableTreeGrid(this.getTreeOptions());
    return this;
  }
  
  /**
   * Expand a specific node
   * @param {string|number} id - ID of the node to expand
   * @returns {WTreeView} - For chaining
   */
  expandNode(id) {
    const dataView = this.dataView;
    const items = dataView.getItems();
    const item = items.find(item => item[this.getTreeOptions().idField] == id);
    
    if (item && !item[this.getTreeOptions().expandedField]) {
      item[this.getTreeOptions().expandedField] = true;
      this.refreshTreeView();
    }
    
    return this;
  }
  
  /**
   * Collapse a specific node
   * @param {string|number} id - ID of the node to collapse
   * @returns {WTreeView} - For chaining
   */
  collapseNode(id) {
    const dataView = this.dataView;
    const items = dataView.getItems();
    const item = items.find(item => item[this.getTreeOptions().idField] == id);
    
    if (item && item[this.getTreeOptions().expandedField]) {
      item[this.getTreeOptions().expandedField] = false;
      this.refreshTreeView();
    }
    
    return this;
  }
  
  /**
   * Toggle a node's expanded state
   * @param {string|number} id - ID of the node to toggle
   * @returns {WTreeView} - For chaining
   */
  toggleNode(id) {
    const dataView = this.dataView;
    const item = dataView.getItemById(id);
    
    if (item && item._hasChildren) {
      // Toggle expanded state
      item[this.getTreeOptions().expandedField] = !item[this.getTreeOptions().expandedField];
      
      // Update the data view to reflect changes
      dataView.updateItem(id, item);
      
      // Refresh the tree to update visibility of children
      this.refreshTreeView();
    }
    
    return this;
  }

  setupTreeSorting() {
  this.grid.onSort.subscribe((e, args) => {
    const dataView = this.dataView;
    const treeOptions = this.getTreeOptions();
    const items = dataView.getItems();
    
    // Group items by parentId
    const itemsByParent = this.#groupByParent(items, treeOptions.parentIdField);
    
    // Sort each group of siblings
    for (const parentId in itemsByParent) {
      const siblingItems = itemsByParent[parentId];
      siblingItems.sort((a, b) => {
        for (const sortCol of args.sortCols) {
          const field = sortCol.sortCol.field;
          const sign = sortCol.sortAsc ? 1 : -1;
          const valueA = a[field];
          const valueB = b[field];
          
          // Handle undefined values
          if (valueA === undefined && valueB === undefined) continue;
          if (valueA === undefined) return 1 * sign;
          if (valueB === undefined) return -1 * sign;
          
          // Compare values
          const result = (valueA === valueB ? 0 : (valueA > valueB ? 1 : -1)) * sign;
          if (result !== 0) return result;
        }
        return 0;
      });
    }
    
    // Rebuild the flat array while preserving hierarchy
    const sortedItems = this.#flattenTree(itemsByParent["null"] || [], itemsByParent);
    
    // Update the data view
    dataView.beginUpdate();
    dataView.setItems(sortedItems);
    dataView.endUpdate();
  });
}

// Helper method to group items by parentId
#groupByParent(items, parentIdField) {
  const groups = {};
  
  items.forEach(item => {
    const parentId = item[parentIdField] || "null";
    if (!groups[parentId]) {
      groups[parentId] = [];
    }
    groups[parentId].push(item);
  });
  
  return groups;
}

// Helper method to flatten grouped items back into an array
#flattenTree(rootItems, itemsByParent) {
  const result = [];
  
  function addItemsRecursively(items) {
    if (!items) return;
    
    for (const item of items) {
      result.push(item);
      const childItems = itemsByParent[item.id];
      if (childItems) {
        addItemsRecursively(childItems);
      }
    }
  }
  
  addItemsRecursively(rootItems);
  return result;
}
  
  /**
   * Expand all nodes
   * @returns {WTreeView} - For chaining
   */
  expandAll() {
    const dataView = this.dataView;
    const items = dataView.getItems();
    
    dataView.beginUpdate();
    items.forEach(item => {
      if (item._hasChildren) {
        item[this.getTreeOptions().expandedField] = true;
      }
    });
    this.refreshTreeView();
    dataView.endUpdate();
    
    return this;
  }
  
  /**
   * Collapse all nodes
   * @returns {WTreeView} - For chaining
   */
  collapseAll() {
    const dataView = this.dataView;
    const items = dataView.getItems();
    
    dataView.beginUpdate();
    items.forEach(item => {
      if (item._hasChildren) {
        item[this.getTreeOptions().expandedField] = false;
      }
    });
    this.refreshTreeView();
    dataView.endUpdate();
    
    return this;
  }
  
  /**
   * Expand node and all its parents (reveal node)
   * @param {string|number} id - ID of the node to reveal
   * @returns {WTreeView} - For chaining
   */
  revealNode(id) {
    const dataView = this.dataView;
    const items = dataView.getItems();
    const item = items.find(item => item[this.getTreeOptions().idField] == id);
    
    if (!item) return this;
    
    // Expand all parents
    let currentId = item[this.getTreeOptions().parentIdField];
    
    dataView.beginUpdate();
    while (currentId) {
      const parent = items.find(p => p[this.getTreeOptions().idField] == currentId);
      if (!parent) break;
      
      parent[this.getTreeOptions().expandedField] = true;
      currentId = parent[this.getTreeOptions().parentIdField];
    }
    
    this.refreshTreeView();
    dataView.endUpdate();
    
    // Scroll to the node
    const row = dataView.getRowById(id);
    if (row !== undefined) {
      this.grid.scrollRowIntoView(row);
    }
    
    return this;
  }
  
  /**
   * Get selected items
   * @returns {Array} - Array of selected items
   */
  getSelectedItems() {
    return this.dataView.getItems().filter(item => item._selected);
  }
  
  /**
   * Select a specific node
   * @param {string|number} id - ID of the node to select
   * @param {boolean} clearOthers - Whether to clear other selections
   * @returns {WTreeView} - For chaining
   */
  selectNode(id, clearOthers = true) {
    const dataView = this.dataView;
    const items = dataView.getItems();
    
    dataView.beginUpdate();
    
    if (clearOthers) {
      items.forEach(item => {
        item._selected = false;
      });
    }
    
    const item = items.find(item => item[this.getTreeOptions().idField] == id);
    if (item) {
      item._selected = true;
      this.revealNode(id);
    }
    
    dataView.endUpdate();
    this.#triggerSelectionChanged();
    
    return this;
  }
  
  /**
   * Set cascade selection (when a parent is selected, select all children)
   * @param {boolean} enabled - Whether cascade selection is enabled
   * @returns {WTreeView} - For chaining
   */
  setCascadeSelection(enabled = true) {
    this.#cascadeSelection = enabled;
    return this;
  }
}
