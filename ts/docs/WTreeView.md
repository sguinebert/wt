# WTreeView Documentation

## Overview

WTreeView is a high-performance tree component that extends WTableView to provide specialized tree functionality with advanced features like virtualization, multi-column support, and complex interactions.

![WTreeView Example](../assets/images/wtreeview-example.png)

## Tree Component Comparison

There are two main tree components available:

### WTree (Simple Tree)
- Basic hierarchy display
- Expand/collapse functionality
- Icons & checkboxes
- Simple selection
- Lightweight and fast for small to medium trees

### WTreeView (Complex Tree with Grid)
- All features from WTree
- Column management and multi-column display
- Virtualization for handling large datasets
- Advanced sorting and filtering
- Complex drag and drop capabilities
- Better suited for data-intensive applications

## Installation

```javascript
import WTreeView from './WTreeView.esm.js';
```

## Basic Setup

Creating a basic tree view is simple:

```javascript
const treeView = new WTreeView({
  el: '#tree-container',
  columns: [
    { id: 'name', name: 'Name', field: 'name', width: 220 }
  ]
});

// Load data with parent-child relationships
treeView.setData([
  { id: 1, name: "Root 1" },
  { id: 2, name: "Child 1.1", parentId: 1 },
  { id: 3, name: "Child 1.2", parentId: 1 }
]);
```

## Key Features with Examples

### 1. Multi-Column Tree

WTreeView can display multiple columns of data, unlike a simple tree:

```javascript
const treeView = new WTreeView({
  el: '#tree-container',
  columns: [
    { id: 'name', name: 'Name', field: 'name', width: 220 },
    { id: 'size', name: 'Size', field: 'size', width: 80 },
    { id: 'modified', name: 'Last Modified', field: 'modified', width: 150 }
  ]
});

treeView.setData([
  { id: 1, name: "Documents", size: "120KB", modified: "2023-05-10" },
  { id: 2, name: "Report.pdf", parentId: 1, size: "85KB", modified: "2023-05-09" },
  { id: 3, name: "Notes.txt", parentId: 1, size: "35KB", modified: "2023-05-10" }
]);
```

### 2. Checkbox Selection

Enable checkbox selection for items:

```javascript
const treeView = new WTreeView({
  el: '#tree-container',
  checkboxes: true,
  columns: [
    { id: 'name', name: 'Name', field: 'name', width: 220 }
  ]
});

// Listen for selection changes
document.querySelector('#tree-container').addEventListener('selectionchanged', e => {
  console.log('Selected items:', e.detail.selected);
});

// Programmatically select a node
treeView.selectNode(2);

// Enable cascade selection (selecting a parent selects all children)
treeView.setCascadeSelection(true);
```

### 3. Custom Icons

You can customize the icons for different node types:

```javascript
const treeView = new WTreeView({
  el: '#tree-container',
  columns: [
    { id: 'name', name: 'Name', field: 'name', width: 220 }
  ]
});

// Set data with custom icons
treeView.setData([
  { 
    id: 1, 
    name: "Project", 
    icon: "<span class='material-icons'>folder</span>" 
  },
  { 
    id: 2, 
    name: "README.md", 
    parentId: 1, 
    icon: "<span class='material-icons'>description</span>" 
  },
  { 
    id: 3, 
    name: "Images", 
    parentId: 1, 
    icon: "<span class='material-icons'>photo_library</span>" 
  }
]);
```

### 4. Expand and Collapse Operations

Control the expanded state of the tree:

```javascript
// Expand all nodes
treeView.expandAll();

// Collapse all nodes
treeView.collapseAll();

// Expand a specific node
treeView.expandNode(1);

// Collapse a specific node
treeView.collapseNode(1);

// Toggle a node's expanded state
treeView.toggleNode(1);

// Expand a node and all its parents to make it visible
treeView.revealNode(5);
```

### 5. Handling Large Data Sets (Virtualization)

WTreeView efficiently handles large data sets through virtualization, rendering only visible rows:

```javascript
// Generate a large dataset
const largeData = [];
for (let i = 0; i < 1000; i++) {
  largeData.push({ id: i, name: `Item ${i}` });
  
  // Add children
  if (i < 50) {
    for (let j = 0; j < 20; j++) {
      largeData.push({ 
        id: `${i}-${j}`, 
        name: `Child ${i}-${j}`, 
        parentId: i 
      });
    }
  }
}

const treeView = new WTreeView({
  el: '#large-tree-container',
  columns: [
    { id: 'name', name: 'Name', field: 'name', width: 220 }
  ]
});

treeView.setData(largeData);
```

### 6. Drag and Drop

Enable drag and drop for tree nodes:

```javascript
const treeView = new WTreeView({
  el: '#tree-container',
  dragDrop: true,
  columns: [
    { id: 'name', name: 'Name', field: 'name', width: 220 }
  ]
});

// Listen for drag and drop events
document.querySelector('#tree-container').addEventListener('itemdropped', e => {
  console.log('Item dropped:', e.detail);
  console.log('Source:', e.detail.source);
  console.log('Target:', e.detail.target);
  console.log('Position:', e.detail.position); // 'before', 'after', or 'child'
});
```

## API Reference

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| el | string\|HTMLElement | - | Container element or selector |
| data | Array<Object> | [] | Initial tree data |
| dataUrl | string\|Function | null | URL or function for loading data |
| columns | Array<Object> | [{id:'tree',name:'Tree',field:'name',width:220}] | Column definitions |
| checkboxes | boolean | false | Whether to show checkboxes |
| idField | string | 'id' | ID field name |
| parentIdField | string | 'parentId' | Parent ID field name |
| expandedField | string | 'expanded' | Expanded state field name |
| iconField | string | 'icon' | Field containing custom icon |
| indentation | number | 16 | Indentation per level |
| dragDrop | boolean | false | Enable drag and drop |

### Methods

#### Data Operations
- `setData(data)` - Initialize the tree with data
- `getSelectedItems()` - Get selected items

#### Node Operations
- `expandNode(id)` - Expand a specific node
- `collapseNode(id)` - Collapse a specific node
- `toggleNode(id)` - Toggle a node's expanded state
- `expandAll()` - Expand all nodes
- `collapseAll()` - Collapse all nodes
- `revealNode(id)` - Expand node and all its parents
- `selectNode(id, clearOthers = true)` - Select a specific node
- `setCascadeSelection(enabled = true)` - Set cascade selection

#### UI Operations
- `refreshTreeView()` - Refresh tree view after changes

### Events

| Event | Detail | Description |
|-------|--------|-------------|
| selectionchanged | {selected: Array} | Fired when selection changes |
| itemdropped | {source: Object, target: Object, position: string} | Fired when an item is dropped |

## Common Use Cases

### File Explorer

```javascript
const fileExplorer = new WTreeView({
  el: '#file-explorer',
  checkboxes: true,
  columns: [
    { id: 'name', name: 'Name', field: 'name', width: 220 },
    { id: 'size', name: 'Size', field: 'size', width: 80 },
    { id: 'modified', name: 'Modified', field: 'modified', width: 150 }
  ]
});

// Load file system data
fileExplorer.setData([
  { id: 1, name: "Documents", size: "-", modified: "2023-05-01" },
  { id: 2, name: "report.pdf", parentId: 1, size: "2.1MB", modified: "2023-05-01" },
  { id: 3, name: "Images", size: "-", modified: "2023-04-28" },
  { id: 4, name: "photo1.jpg", parentId: 3, size: "3.2MB", modified: "2023-04-27" },
  { id: 5, name: "photo2.jpg", parentId: 3, size: "2.8MB", modified: "2023-04-28" }
]);
```

### Organization Chart

```javascript
const orgChart = new WTreeView({
  el: '#org-chart',
  columns: [
    { id: 'name', name: 'Employee', field: 'name', width: 220 },
    { id: 'title', name: 'Title', field: 'title', width: 200 },
    { id: 'department', name: 'Department', field: 'department', width: 150 }
  ]
});

// Load organization data
orgChart.setData([
  { id: 1, name: "John Smith", title: "CEO", department: "Executive" },
  { id: 2, name: "Jane Doe", title: "CTO", department: "Technology", parentId: 1 },
  { id: 3, name: "Bob Johnson", title: "CFO", department: "Finance", parentId: 1 },
  { id: 4, name: "Alice Brown", title: "Lead Developer", department: "Technology", parentId: 2 },
  { id: 5, name: "Charlie Wilson", title: "UX Designer", department: "Technology", parentId: 2 }
]);
```

## Performance Tips

1. **Avoid Expanding All Nodes** with very large datasets, as this can impact performance.
2. **Use Virtualization** (built-in) for large trees to maintain smooth scrolling.
3. **Limit Tree Depth** when possible - extremely deep hierarchies can be difficult to navigate.
4. **Consider Async Loading** for subtrees when dealing with extremely large datasets.
