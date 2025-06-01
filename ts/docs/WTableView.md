# WTableView Documentation

WTableView is a powerful ES6+ wrapper around SlickGrid for the Wt frontend, providing advanced data grid functionality with minimal setup.

## Table of Contents

- [Basic Setup](#basic-setup)
- [Data Loading](#data-loading)
- [Column Configuration](#column-configuration)
- [Sorting](#sorting)
- [Filtering](#filtering)
- [Grouping](#grouping)
- [Selection](#selection)
- [Editing](#editing)
- [Formatting](#formatting)
- [Advanced Features](#advanced-features)
  - [Frozen Columns/Rows](#frozen-columnsrows)
  - [Header and Footer Rows](#header-and-footer-rows)
  - [Tree Grid](#tree-grid)
  - [Clipboard Support](#clipboard-support)
  - [Validation](#validation)
  - [Context Menu](#context-menu)
- [Events and Callbacks](#events-and-callbacks)
- [API Reference](#api-reference)

## Basic Setup

Initialize WTableView with a container element and column definitions:

```javascript
const columns = [
  { id: "id", name: "ID", field: "id", width: 50 },
  { id: "name", name: "Name", field: "name", width: 150, editor: Editors.Text },
  { id: "price", name: "Price", field: "price", width: 80, formatter: WtFormatters.currency() }
];

const tableView = new WTableView({
  el: '#grid-container',
  columns: columns,
  dataUrl: '/api/data',
  pageSize: 100,
  pagerElement: '#pager'
});
```

## Data Loading

### Remote Data Loading

Load data from a REST endpoint:

```javascript
const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  dataUrl: '/api/data',  // URL that returns JSON { items: [...], done: boolean, nextKey: any }
  pageSize: 50
});
```

### Function-based Data Loading

Use a function for dynamic URL generation:

```javascript
const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  dataUrl: ({cursor, limit}) => `/api/data?sid=${Wt.sessionId}&cursor=${cursor??''}&limit=${limit}`,
  pageSize: 100
});
```

### Updating Data URL

Change the data source at runtime:

```javascript
// Update when session ID changes
tableView.setDataUrl(`/api/data?sid=${newSessionId}`);

// Update with function and don't auto-refresh
tableView.setDataUrl(({cursor, limit}) => `/api/newdata?cursor=${cursor??''}&limit=${limit}`, false);
```

## Column Configuration

### Basic Column Definition

```javascript
const columns = [
  { id: "id", name: "ID", field: "id", width: 50 },
  { id: "name", name: "Name", field: "name", width: 150 },
  { id: "email", name: "Email", field: "email", width: 200 }
];
```

### Column with Editor

```javascript
{ 
  id: "date", 
  name: "Date", 
  field: "date", 
  width: 120, 
  editor: Editors.Date 
}
```

### Column with Custom Formatter

```javascript
{ 
  id: "price", 
  name: "Price", 
  field: "price", 
  width: 80, 
  formatter: WtFormatters.currency('€', 2)
}
```

### Column with Both Formatter and Editor

```javascript
{ 
  id: "progress", 
  name: "Progress", 
  field: "progress", 
  width: 100, 
  editor: Editors.Integer,
  formatter: WtFormatters.percentage(true)
}
```

## Sorting

### Enable Multi-Column Sort

```javascript
const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  options: {
    multiColumnSort: true
  }
});
```

## Filtering

### Apply Column Filters

```javascript
// Filter rows where name contains "John"
tableView.applyColumnFilters({
  name: "John"
});

// Multiple filters (AND condition)
tableView.applyColumnFilters({
  name: "John",
  status: "active"
});
```

### Complex Filter

```javascript
// Custom filter function
tableView.setFilter(item => {
  return item.price > 100 && item.category === "electronics";
});

// Object-based filter specification
tableView.setFilter({
  column: "price",
  operator: "greaterThan",
  value: 100,
  condition: "and",
  filters: [
    {
      column: "category",
      operator: "equals",
      value: "electronics"
    }
  ]
});
```

## Grouping

### Basic Column Grouping

```javascript
// Group by a single column
tableView.groupByColumn("category");

// Group with custom options
tableView.groupByColumn("category", {
  formatter: g => `Category: ${g.value} (${g.count} items)`,
  aggregators: [new Aggregators.Sum("price")],
  collapsed: true,
  aggregateCollapsed: true
});
```

### Custom Group Sorting

```javascript
// Sort groups by count (smallest to largest)
tableView.groupByColumnCustomSort("category", (a, b) => a.count - b.count);

// Sort groups by count (largest to smallest)
tableView.groupByColumnCustomSort("category", (a, b) => b.count - a.count);
```

### Multi-Level Grouping

```javascript
tableView.setMultiLevelGrouping([
  {
    getter: "category",
    formatter: g => `Category: ${g.value} (${g.count} items)`,
    aggregators: [new Aggregators.Sum("price")],
    aggregateCollapsed: true
  },
  {
    getter: "status",
    formatter: g => `Status: ${g.value} (${g.count} items)`,
    aggregators: [new Aggregators.Avg("rating")],
    collapsed: true
  }
]);
```

### Expanding and Collapsing Groups

```javascript
// Expand all groups
tableView.expandAllGroups();

// Collapse all groups
tableView.collapseAllGroups();

// Toggle specific group
tableView.toggleGroup("electronics");

// Toggle all groups
tableView.toggleGrouping(true);  // expand all
tableView.toggleGrouping(false); // collapse all
```

## Selection

### Configure Selection

```javascript
const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  multiSelect: true  // Enable multi-row selection
});
```

## Editing

### Enable Cell Editing

```javascript
const columns = [
  { 
    id: "name", 
    name: "Name", 
    field: "name", 
    editor: Editors.Text 
  },
  { 
    id: "complete", 
    name: "Complete", 
    field: "complete", 
    editor: Editors.Checkbox 
  },
  { 
    id: "start", 
    name: "Start", 
    field: "start", 
    editor: Editors.Date 
  }
];

const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  options: {
    editable: true,
    enableCellNavigation: true,
    autoEdit: false  // Double-click to edit
  }
});
```

## Formatting

### Using Built-in Formatters

```javascript
const columns = [
  { 
    id: "price", 
    name: "Price", 
    field: "price", 
    formatter: WtFormatters.currency('$', 2)
  },
  { 
    id: "progress", 
    name: "Progress", 
    field: "progress", 
    formatter: WtFormatters.percentage(true)
  },
  { 
    id: "trend", 
    name: "Trend", 
    field: "history", 
    formatter: WtFormatters.sparkline('history', {
      width: 100,
      height: 20,
      color: '#0066cc'
    })
  }
];
```

## Advanced Features

### Frozen Columns/Rows

```javascript
// Freeze first two columns
const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  frozenColumns: 2
});

// Freeze first row
const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  frozenRows: 1
});

// Update at runtime
tableView.setFrozenColumns(3);
tableView.setFrozenRows(2);
```

### Header and Footer Rows

```javascript
// Enable header filter row and footer aggregate row
const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  showHeaderRow: true,
  headerRowHeight: 30,
  showFooterRow: true,
  footerRowHeight: 30
});

// Update at runtime
tableView.showHeaderRow(true);
tableView.showFooterRow(true);
```

### Column Groups

```javascript
const columnGroups = [
  { 
    name: "Customer Information", 
    columns: ["firstName", "lastName", "email"] 
  },
  { 
    name: "Order Details", 
    columns: ["product", "quantity", "price", "total"] 
  }
];

const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  columnGroups: columnGroups
});

// Update at runtime
tableView.setColumnGroups([
  { 
    name: "Personal", 
    columns: ["firstName", "lastName"] 
  },
  { 
    name: "Contact", 
    columns: ["email", "phone"] 
  }
]);
```

### Tree Grid

```javascript
// Initialize with tree grid options
const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  treeOptions: {
    idField: 'id',
    parentIdField: 'parentId',
    expandedField: 'expanded',
    treeField: 'name',  // column to display tree controls in
    indentation: 15
  }
});

// Enable at runtime
tableView.enableTreeGrid({
  idField: 'id',
  parentIdField: 'parentId',
  expandedField: 'expanded',
  treeField: 'name'
});

// Toggle node expanded state
tableView.toggleTreeNode(42);  // Toggle item with id=42
```

### Clipboard Support

```javascript
// Enable on initialization
const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  enableClipboard: true
});

// Enable at runtime
tableView.enableClipboard();
```

### Validation

```javascript
// Setup with validators
const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  validators: {
    email: (value) => {
      return /\S+@\S+\.\S+/.test(value) || 'Please enter a valid email';
    },
    age: (value) => {
      return (value >= 18 && value <= 100) || 'Age must be between 18 and 100';
    }
  }
});

// Add validator at runtime
tableView.setValidator('price', (value) => {
  return value > 0 || 'Price must be greater than zero';
});
```

### Context Menu

```javascript
// Initialize with context menu
const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  contextMenu: {
    items: [
      { 
        title: "Edit Row", 
        icon: "fa fa-edit", 
        action: (item) => console.log('Edit', item) 
      },
      { 
        title: "Delete Row", 
        icon: "fa fa-trash", 
        action: (item) => console.log('Delete', item) 
      },
      { divider: true },
      { 
        title: "Export", 
        icon: "fa fa-download", 
        action: () => console.log('Export') 
      }
    ]
  }
});

// Update context menu at runtime
tableView.setContextMenu({
  items: (item, column) => {
    // Dynamic menu based on item/column
    return [
      { 
        title: `Edit ${item.name}`, 
        action: () => console.log('Edit', item),
        disabled: item.locked
      }
    ];
  }
});
```

## Events and Callbacks

```javascript
const tableView = new WTableView({
  el: '#grid',
  columns: columns,
  
  // Scrolling callback
  onScrolled: (left, top, width, height) => {
    console.log('Grid scrolled', { left, top, width, height });
  },
  
  // Column resize callback
  onColumnResized: (columnId, width) => {
    console.log('Column resized', columnId, width);
  },
  
  // Drop event callbacks
  onDropEvent: (rowIdx, columnId, sourceId, mimeType) => {
    console.log('Cell drop', rowIdx, columnId, sourceId, mimeType);
  },
  
  onRowDropEvent: (rowIdx, columnId, sourceId, mimeType, side) => {
    console.log('Row drop', rowIdx, columnId, sourceId, mimeType, side);
  }
});
```

## API Reference

### Core Methods

```javascript
// Refresh the grid
tableView.refresh();

// Reset the grid (clear data)
tableView.reset();

// Update a single item with highlighting
tableView.updateItem(item, { highlight: true, highlightDuration: 500 });

// Update multiple items
tableView.updateItems([item1, item2], { highlight: true });

// Scroll to a specific position
tableView.scrollTo(x, y);

// Resize a column
tableView.resizeColumn('name', 20);  // Add 20px to 'name' column

// Destroy grid (cleanup)
tableView.destroy();
```

### Other Useful Methods

```javascript
// Set or update data URL
tableView.setDataUrl('/api/newdata');

// Set filter
tableView.setFilter(item => item.active === true);

// Remove filter
tableView.setFilter(null);

// Set validators
tableView.setValidator('email', value => /\S+@\S+\.\S+/.test(value) || 'Invalid email');
```

## Complete Usage Example

Here's a complete example showing how to set up a WTableView with many features enabled:

```javascript
// Define columns
const columns = [
  { id: "id", name: "ID", field: "id", width: 50 },
  { id: "name", name: "Name", field: "name", width: 150, editor: Editors.Text },
  { 
    id: "category", 
    name: "Category", 
    field: "category", 
    width: 120,
    editor: Editors.Select,
    options: ["Electronics", "Books", "Clothing", "Food"]
  },
  { 
    id: "price", 
    name: "Price", 
    field: "price", 
    width: 80, 
    formatter: WtFormatters.currency('$'),
    editor: Editors.Float,
    validator: value => value > 0 || "Price must be positive"
  },
  { 
    id: "stock", 
    name: "In Stock", 
    field: "stock", 
    width: 70,
    formatter: (row, cell, value) => value ? "✓" : "✗",
    editor: Editors.Checkbox
  },
  { 
    id: "rating", 
    name: "Rating", 
    field: "rating", 
    width: 100,
    formatter: WtFormatters.percentage(),
    editor: Editors.Integer
  }
];

// Initialize grid
const tableView = new WTableView({
  el: '#products-grid',
  columns: columns,
  dataUrl: '/api/products',
  pageSize: 50,
  pagerElement: '#products-pager',
  
  // Advanced options
  showHeaderRow: true,
  showFooterRow: true,
  multiSelect: true,
  enableClipboard: true,
  
  // Context menu
  contextMenu: {
    items: [
      { title: "Edit", action: item => openEditDialog(item) },
      { title: "Delete", action: item => confirmDelete(item) },
      { divider: true },
      { title: "View Details", action: item => showDetails(item) }
    ]
  },
  
  // Validation
  validators: {
    price: value => value > 0 || "Price must be positive",
    rating: value => (value >= 0 && value <= 100) || "Rating must be 0-100%"
  },
  
  // Event handlers
  onColumnResized: (columnId, width) => saveColumnSettings(columnId, width)
});

// Group by category
tableView.groupByColumn("category", {
  formatter: g => `${g.value} (${g.count} products)`,
  aggregators: [new Aggregators.Avg("price"), new Aggregators.Sum("rating")],
  collapsed: false
});

// Apply initial filter
tableView.setFilter(item => item.price > 10);
```

## C++ Server-Side Configuration with fmt::format_to()

When generating JavaScript for WTableView from C++ server-side code, you can use fmt::format_to() for efficient string building:

```cpp
// Complete WTableView configuration with fmt::format_to()
std::string generateWTableViewConfig(const std::string& gridId, 
                                    const std::string& dataUrl,
                                    const std::vector<Product>& sampleData) {
    std::string result;
    fmt::memory_buffer buf;
    
    // Column definitions with formatters
    fmt::format_to(std::back_inserter(buf),
        R"(
        const columns = [
            {{ id: "id", name: "ID", field: "id", width: 50 }},
            {{ id: "name", name: "Name", field: "name", width: 150, editor: Editors.Text }},
            {{ 
                id: "category", 
                name: "Category", 
                field: "category", 
                width: 120,
                editor: Editors.Select,
                options: {0}
            }},
            {{ 
                id: "price", 
                name: "Price", 
                field: "price", 
                width: 80, 
                formatter: WtFormatters.currency('{1}'),
                editor: Editors.Float,
                validator: value => value > {2} || "Price must be positive"
            }},
            {{ 
                id: "stock", 
                name: "In Stock", 
                field: "stock", 
                width: 70,
                formatter: (row, cell, value) => value ? "{3}" : "{4}",
                editor: Editors.Checkbox
            }},
            {{ 
                id: "rating", 
                name: "Rating", 
                field: "rating", 
                width: 100,
                formatter: WtFormatters.percentage(),
                editor: Editors.Integer,
                validator: value => (value >= {5} && value <= {6}) || "Rating must be {5}-{6}%"
            }}
        ];
        )",
        // {0} - Category options as JSON array
        generateCategoryOptions(),
        // {1} - Currency symbol
        getCurrencySymbol(),
        // {2} - Minimum price
        0,
        // {3} - In stock symbol
        "✓",
        // {4} - Out of stock symbol
        "✗",
        // {5} - Min rating
        0,
        // {6} - Max rating
        100
    );
    
    // Main WTableView configuration
    fmt::format_to(std::back_inserter(buf),
        R"(
        const tableView = new WTableView({{
            el: '#{0}',
            columns: columns,
            dataUrl: '{1}',
            pageSize: {2},
            pagerElement: '#{0}-pager',
            
            // Advanced options
            showHeaderRow: {3},
            showFooterRow: {4},
            multiSelect: {5},
            enableClipboard: {6},
            
            // Context menu
            contextMenu: {7},
            
            // Validation
            validators: {8},
            
            // Tree options (if enabled)
            {9}
            
            // Event handlers
            onScrolled: {10},
            onColumnResized: {11},
            onDropEvent: {12},
            onRowDropEvent: {13}
        }});
        )",
        // {0} - Grid element ID
        gridId,
        // {1} - Data URL
        dataUrl,
        // {2} - Page size
        50,
        // {3} - Show header row
        true,
        // {4} - Show footer row
        true,
        // {5} - Multi select
        true,
        // {6} - Enable clipboard
        true,
        // {7} - Context menu configuration
        generateContextMenu(gridId),
        // {8} - Validators
        generateValidators(),
        // {9} - Tree options (if enabled)
        generateTreeOptions(),
        // {10} - Scrolled callback
        generateScrolledCallback(gridId),
        // {11} - Column resized callback
        generateColumnResizedCallback(gridId),
        // {12} - Drop event callback
        generateDropEventCallback(gridId),
        // {13} - Row drop event callback
        generateRowDropEventCallback(gridId)
    );
    
    // Initial grouping and filtering
    fmt::format_to(std::back_inserter(buf),
        R"(
        // Group by category
        tableView.groupByColumn("{0}", {{
            formatter: g => `${{g.value}} (${{g.count}} {1})`,
            aggregators: [new Aggregators.Avg("{2}"), new Aggregators.Sum("{3}")],
            collapsed: {4}
        }});

        // Apply initial filter
        tableView.setFilter(item => item.{5} > {6});
        )",
        // {0} - Group by field
        "category",
        // {1} - Items label
        "products",
        // {2} - Avg aggregator field
        "price",
        // {3} - Sum aggregator field
        "rating",
        // {4} - Collapsed state
        "false",
        // {5} - Filter field
        "price",
        // {6} - Filter value
        10
    );
    
    return fmt::to_string(buf);
}

// Helper functions for generating complex parts
std::string generateCategoryOptions() {
    std::vector<std::string> categories = {"Electronics", "Books", "Clothing", "Food"};
    fmt::memory_buffer buf;
    
    fmt::format_to(std::back_inserter(buf), "[");
    for (size_t i = 0; i < categories.size(); ++i) {
        if (i > 0) fmt::format_to(std::back_inserter(buf), ", ");
        fmt::format_to(std::back_inserter(buf), "\"{}\"", categories[i]);
    }
    fmt::format_to(std::back_inserter(buf), "]");
    
    return fmt::to_string(buf);
}

std::string generateContextMenu(const std::string& gridId) {
    fmt::memory_buffer buf;
    
    fmt::format_to(std::back_inserter(buf),
        R"({{
            items: [
                {{ 
                    title: "{0}", 
                    icon: "fa fa-edit", 
                    action: item => Wt.emit('{1}', 'edit', item.id)
                }},
                {{ 
                    title: "{2}", 
                    icon: "fa fa-trash", 
                    action: item => Wt.emit('{1}', 'delete', item.id)
                }},
                {{ divider: true }},
                {{ 
                    title: "{3}", 
                    icon: "fa fa-eye", 
                    action: item => Wt.emit('{1}', 'view', item.id)
                }}
            ]
        }})",
        // {0} - Edit label
        "Edit",
        // {1} - Grid ID for Wt.emit
        gridId,
        // {2} - Delete label
        "Delete",
        // {3} - View details label
        "View Details"
    );
    
    return fmt::to_string(buf);
}

std::string generateValidators() {
    fmt::memory_buffer buf;
    
    fmt::format_to(std::back_inserter(buf),
        R"({{
            price: value => value > {0} || "{1}",
            rating: value => (value >= {2} && value <= {3}) || "{4}",
            email: value => /{5}/.test(value) || "{6}"
        }})",
        // {0} - Min price
        0,
        // {1} - Price error message
        "Price must be positive",
        // {2} - Min rating
        0,
        // {3} - Max rating
        100,
        // {4} - Rating error message
        "Rating must be between 0 and 100%",
        // {5} - Email regex
        "\\S+@\\S+\\.\\S+",
        // {6} - Email error message
        "Please enter a valid email address"
    );
    
    return fmt::to_string(buf);
}

std::string generateTreeOptions() {
    // If tree view is not enabled, return empty string
    if (!isTreeViewEnabled()) {
        return "";
    }
    
    fmt::memory_buffer buf;
    
    fmt::format_to(std::back_inserter(buf),
        R"(treeOptions: {{
            idField: '{0}',
            parentIdField: '{1}',
            expandedField: '{2}',
            treeField: '{3}',
            indentation: {4}
        }},)",
        // {0} - ID field
        "id",
        // {1} - Parent ID field
        "parentId",
        // {2} - Expanded field
        "expanded",
        // {3} - Tree field (column to show controls)
        "name",
        // {4} - Indentation
        15
    );
    
    return fmt::to_string(buf);
}

std::string generateScrolledCallback(const std::string& gridId) {
    fmt::memory_buffer buf;
    
    fmt::format_to(std::back_inserter(buf),
        R"((left, top, width, height) => {{
            Wt.emit('{0}', 'scrolled', Math.round(left), Math.round(top), 
                    Math.round(width), Math.round(height));
            {1}
        }})",
        // {0} - Grid ID
        gridId,
        // {1} - Additional client-side code (if any)
        "console.log('Grid scrolled', { left, top, width, height });"
    );
    
    return fmt::to_string(buf);
}

std::string generateColumnResizedCallback(const std::string& gridId) {
    fmt::memory_buffer buf;
    
    fmt::format_to(std::back_inserter(buf),
        R"((columnId, width) => {{
            Wt.emit('{0}', 'columnResized', columnId, parseInt(width));
            {1}
        }})",
        // {0} - Grid ID
        gridId,
        // {1} - Additional client-side code
        "localStorage.setItem(`${columnId}_width`, width);"
    );
    
    return fmt::to_string(buf);
}

std::string generateDropEventCallback(const std::string& gridId) {
    fmt::memory_buffer buf;
    
    fmt::format_to(std::back_inserter(buf),
        R"((rowIdx, columnId, sourceId, mimeType) => {{
            Wt.emit('{0}', {{
                name: "dropEvent", 
                eventObject: {{}}, 
                event: {{}}
            }}, rowIdx, columnId, sourceId, mimeType);
        }})",
        // {0} - Grid ID
        gridId
    );
    
    return fmt::to_string(buf);
}

std::string generateRowDropEventCallback(const std::string& gridId) {
    fmt::memory_buffer buf;
    
    fmt::format_to(std::back_inserter(buf),
        R"((rowIdx, columnId, sourceId, mimeType, side) => {{
            Wt.emit('{0}', {{
                name: "rowDropEvent", 
                eventObject: {{}}, 
                event: {{}}
            }}, rowIdx, columnId, sourceId, mimeType, side || "bottom");
        }})",
        // {0} - Grid ID
        gridId
    );
    
    return fmt::to_string(buf);
}

// Usage example:
// std::string jsCode = generateWTableViewConfig("products-grid", "/api/products", products);
// wApp->doJavaScript(jsCode);
```
