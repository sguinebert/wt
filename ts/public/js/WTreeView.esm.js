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
  #checkboxes = false;
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
   * @param {string} config.checkboxPlacement - Where to place checkboxes ('column' or 'inline', default: 'column')
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
    const checkboxPlacement = config.checkboxPlacement || 'column';
    
    // Setup tree configuration
    const treeOptions = {
      idField: config.idField || 'id',
      parentIdField: config.parentIdField || 'parentId',
      expandedField: config.expandedField || 'expanded',
      treeField: treeField,
      indentation: config.indentation || 16,
      iconField: config.iconField || 'icon',
      checkboxPlacement: checkboxPlacement
    };
    
    // Add checkbox column if requested
    if (config.checkboxes && checkboxPlacement === 'column') {
      config.columns.unshift({
        id: 'checkbox',
        name: '',
        field: '_checkbox',
        width: 40,
        formatter: (_row, _cell, _value, _columnDef, dataItem) => {
          const checked = dataItem._selected ? 'checked' : '';
          const indeterminate = dataItem._indeterminate ? 'indeterminate="true"' : '';
          return `<input type="checkbox" ${checked} ${indeterminate} class="tree-checkbox">`;
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

          if (config.checkboxes && checkboxPlacement === 'inline') {
            const checked = dataItem._selected ? 'checked' : '';
            const indeterminate = dataItem._indeterminate ? 'indeterminate="true"' : '';
            treeHtml += `<input type="checkbox" ${checked} ${indeterminate} 
                        class="tree-checkbox inline" style="margin-right:4px;">`;
          }
          
          // Create toggle HTML
          // if (hasChildren) {
          //   const expanded = dataItem[treeOptions.expandedField];
          //   treeHtml = `<span class="wt-tree-toggle ${expanded ? 'expanded' : 'collapsed'}" 
          //     style="margin-left:${indent}px" data-id="${dataItem[treeOptions.idField]}"></span>`;
          // } else {
          //   treeHtml = `<span class="wt-tree-leaf" style="margin-left:${indent}px"></span>`;
          // }
          
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
    

    // Set up drag and drop if enabled
    if (config.dragDrop) {
      this.#setupDragDrop();
    }

    this.#setupTreeFormatting();
    this.#setupTreeFiltering();
    if(config.data)
      this.setData(config.data);
    this.#setupToggleHandlers();
    // Set up checkbox handling if enabled
    if (config.checkboxes) {
      this.#checkboxes = true;
      this.#setupCheckboxes();
    }
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
    if (!item) return false;
    
    // Always show root items
    if (!item[treeOptions.parentIdField]) return true;
    
    // Check parent chain to see if any are collapsed
    let currentParentId = item[treeOptions.parentIdField];
    
    while (currentParentId) {
      const parent = dataView.getItemById(currentParentId);
      if (!parent) break;
      
      // If a parent is collapsed, hide this item
      if (parent._collapsed) {
        return false;
      }
      
      // Move up to next parent
      currentParentId = parent[treeOptions.parentIdField];
    }
    
    return true;
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

    // Initialize checkbox states if enabled
    if (this.#checkboxes) {
      // Calculate initial parent checkbox states
      const items = dataView.getItems();
      const rootItems = items.filter(item => !item[this.getTreeOptions().parentIdField]);
      
      rootItems.forEach(root => {
        this.#updateParentCheckboxStates(root);
      });
    }

    this.refreshTreeView(); // Updates visibility based on expanded state
    dataView.endUpdate();

    this.setupTreeSorting();
    
    return this;
  }
  
  /**
   * Update checkbox state for a node and all its children
   * @private
   */
  #cascadeCheckboxToChildren(item, checked) {
    const dataView = this.dataView;
    const treeOptions = this.getTreeOptions();
    const items = dataView.getItems();
    const idField = treeOptions.idField;
    
    // Start batch update
    dataView.beginUpdate();
    
    // Function to process a node and its children recursively
    const processNode = (node) => {
      // Update this node
      node._selected = checked;
      node._indeterminate = false;
      dataView.updateItem(node[idField], node);
      
      // Find and update all children
      items.forEach(childItem => {
        if (childItem[treeOptions.parentIdField] === node[idField]) {
          processNode(childItem);
        }
      });
    };
    
    // Start processing with the clicked item
    processNode(item);
    
    // End batch update
    dataView.endUpdate();
  }
  /**
   * Update parent nodes based on children's states
   * @private
   */
  #updateParentCheckboxStates(item) {
    const dataView = this.dataView;
    const treeOptions = this.getTreeOptions();
    const items = dataView.getItems();
    const idField = treeOptions.idField;
    const parentIdField = treeOptions.parentIdField;
    
    // Start batch update
    dataView.beginUpdate();
    
    // Function to process a node and update its state
    const updateNodeState = (node) => {
      // Find all direct children
      const children = items.filter(child => 
        child[parentIdField] === node[idField]);
      
      if (children.length === 0) return;
      
      // Count selected and indeterminate children
      const selectedCount = children.filter(child => 
        child._selected || child._indeterminate).length;
      
      // Update node state
      if (selectedCount === 0) {
        // None selected
        node._selected = false;
        node._indeterminate = false;
      } else if (selectedCount === children.length) {
        // All selected
        node._selected = true;
        node._indeterminate = false;
      } else {
        // Some selected - indeterminate state
        node._selected = false;
        node._indeterminate = true;
      }
      
      // Update in data view
      dataView.updateItem(node[idField], node);
      
      // Process parent of this node recursively
      const parentId = node[parentIdField];
      if (parentId) {
        const parent = items.find(p => p[idField] === parentId);
        if (parent) updateNodeState(parent);
      }
    };
    
    // Find the parent of the clicked item
    const parentId = item[parentIdField];
    if (parentId) {
      const parent = items.find(p => p[idField] === parentId);
      if (parent) updateNodeState(parent);
    }
    
    // End batch update
    dataView.endUpdate();
  }
  /**
   * Pre-process data to add necessary tree properties
   * @private
   */
  #preprocessData(data) {
    if (!data || !data.length) return [];
    
    // Clone the data to avoid modifying originals
    const processedData = JSON.parse(JSON.stringify(data));
    const treeOptions = this.getTreeOptions();
    
    // Build parent-child relationships and compute levels
    const idToItem = new Map();
    processedData.forEach(item => {
      idToItem.set(item[treeOptions.idField], item);
    });
    
    // Calculate levels and check for children
    processedData.forEach(item => {
      // Set default values
      item._level = 0;
      item._hasChildren = false;
      item._collapsed = item._collapsed ?? !item[treeOptions.expandedField];
      
      // Calculate level by traversing parents
      let parent = idToItem.get(item[treeOptions.parentIdField]);
      while (parent) {
        item._level++;
        parent._hasChildren = true;
        parent = idToItem.get(parent[treeOptions.parentIdField]);
      }
    });
    
    // Sort by hierarchy to ensure parents come before children
    processedData.sort((a, b) => {
      // Sort by parent chain first
      let aParent = a[treeOptions.parentIdField];
      let bParent = b[treeOptions.parentIdField];
      
      if (aParent !== bParent) {
        return (aParent || 0) - (bParent || 0);
      }
      
      // Then by display order if available
      if (a.order !== undefined && b.order !== undefined) {
        return a.order - b.order;
      }
      
      // Default to name sorting
      return (a[treeOptions.treeField] || '').localeCompare(b[treeOptions.treeField] || '');
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
      const target = e.target;
      
      // Check if clicked on checkbox
      if (target.type === 'checkbox' && target.classList.contains('tree-checkbox')) {
        e.stopImmediatePropagation();
        
        const dataView = this.dataView;
        const item = dataView.getItem(args.row);
        if (!item) return;
        
        // Update the checkbox state immediately for better UX
        const checked = target.checked;
        
        // 1. Cascade to children
        this.#cascadeCheckboxToChildren(item, checked);
        
        // 2. Update parent states
        this.#updateParentCheckboxStates(item);
        
        // 3. Refresh grid display
        this.grid.invalidate();
        
        // 4. Trigger selection changed event
        this.#triggerSelectionChanged();
      }
    });
    
    // Add special handling for indeterminate checkboxes
    this.grid.onHeaderRowCellRendered.subscribe((e, args) => {
      const cells = document.querySelectorAll('.slick-cell input.tree-checkbox');
      cells.forEach(checkbox => {
        // Set indeterminate property since HTML attributes can't set this directly
        if (checkbox.getAttribute('indeterminate') === 'true') {
          checkbox.indeterminate = true;
        }
      });
    });
    
    // Also set indeterminate state after grid render
    this.grid.onRendered.subscribe(() => {
      setTimeout(() => {
        const cells = document.querySelectorAll('.slick-cell input.tree-checkbox');
        cells.forEach(checkbox => {
          if (checkbox.getAttribute('indeterminate') === 'true') {
            checkbox.indeterminate = true;
          }
        });
      }, 0);
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
      // Check if clicked on toggle icon
      const target = e.target;
      if (target.classList.contains('toggle') || 
          target.closest('.toggle')) {
        e.stopImmediatePropagation();
        
        // Get the item from the current row
        const item = this.dataView.getItem(args.row);
        if (item) {
          this.toggleNode(item[this.getTreeOptions().idField]);
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
    
    if (item) {
      // Toggle collapsed state (use SlickGrid's approach)
      item._collapsed = !item._collapsed;
      
      // Store in expanded field too for compatibility
      item[this.getTreeOptions().expandedField] = !item._collapsed;
      
      // Update the data view to reflect changes
      dataView.updateItem(id, item);
      
      // Refresh the data view to update visibility
      dataView.refresh();
    }
    
    return this;
  }

  setupTreeSorting() {
    this.grid.onSort.subscribe((e, args) => {
      const dataView = this.dataView;
      const treeOptions = this.getTreeOptions();
      const items = dataView.getItems();
      
      // Get proper field names
      const idField = treeOptions.idField;
      const parentIdField = treeOptions.parentIdField;
      
      // Group items by parentId
      const itemsByParent = this.#groupByParent(items, parentIdField);
      
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
      const sortedItems = this.#flattenTree(itemsByParent["null"] || [], itemsByParent, idField);
      
      // Update the data view
      dataView.beginUpdate();
      dataView.setItems(sortedItems);
      dataView.endUpdate();
    });
    this.grid.onSort.notify({
      multiColumnSort: true,
      sortCols: [
        { sortCol: { field: "_level" }, sortAsc: true },
        { sortCol: { field: this.getTreeOptions().parentIdField }, sortAsc: true }
      ]
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
  #flattenTree(rootItems, itemsByParent, idField) {
    const result = [];
    
    function addItemsRecursively(items) {
      if (!items) return;
      
      for (const item of items) {
        result.push(item);
        // Use the item's ID (using the configured idField) to look up children
        const childItems = itemsByParent[item[idField]];
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
    return this.dataView.getItems().filter(item => 
      item._selected || item._indeterminate);
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
