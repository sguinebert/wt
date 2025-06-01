# Font Awesome Icons Documentation

This documentation provides a comprehensive guide to using our Font Awesome integration module. The module provides a streamlined way to use Font Awesome icons in your application with additional utility functions for common use cases.

## Table of Contents

- [Installation](#installation)
- [API Reference](#api-reference)
- [Usage Examples](#usage-examples)
- [Icon Gallery](#icon-gallery)

## Installation

Import the module in your JavaScript file:

```javascript
import { getIcon, getIconHTML, autoReplace, getTreeIcons } from '/js/icons.esm.js';
```

## API Reference

### getIcon(name, color = null)

Returns the SVG icon as HTML.

**Parameters:**
- `name` (string): The name of the icon to retrieve
- `color` (string, optional): Color for the icon. Can be a hex value or CSS color name

**Returns:** SVG HTML string for the requested icon

**Example:**
```javascript
// Get icon with default color
const searchIcon = getIcon('search');

// Get icon with custom color
const redTrashIcon = getIcon('trash', '#ff0000');
// OR
const blueUserIcon = getIcon('user', 'blue');
```

### getIconHTML(name, color = null)

Returns a legacy Font Awesome HTML string (for backward compatibility).

**Parameters:**
- `name` (string): The name of the icon to retrieve
- `color` (string, optional): Color for the icon as CSS color value

**Returns:** HTML string with Font Awesome classes

**Example:**
```javascript
const folderIconHTML = getIconHTML('folder');
const greenCheckIconHTML = getIconHTML('check', 'green');
```

### autoReplace()

Automatically replaces any Font Awesome classes in the DOM with SVG icons.

**Example:**
```javascript
// Call this after your DOM is loaded
autoReplace();
```

### getTreeIcons()

Returns a set of icons commonly used in tree view components.

**Returns:** Object containing `open`, `close`, `leaf`, and `parent` icon HTML

**Example:**
```javascript
const treeIcons = getTreeIcons();
// Use in a tree component
treeNode.innerHTML = isExpanded ? treeIcons.open : treeIcons.close;
```

## Usage Examples

### Basic Icon Usage

```javascript
// Add an icon to a button
const button = document.createElement('button');
button.innerHTML = `${getIcon('save')} Save Changes`;
document.body.appendChild(button);
```

### Colored Icons

```javascript
// Create a success message with a green check icon
const message = document.createElement('div');
message.className = 'alert alert-success';
message.innerHTML = `${getIcon('check-circle', 'green')} Operation completed successfully!`;
document.body.appendChild(message);
```

### Tree Component Integration

```javascript
import { getTreeIcons } from '/js/icons.esm.js';

function renderTreeNode(node) {
  const icons = getTreeIcons();
  const nodeElement = document.createElement('div');
  
  if (node.children.length > 0) {
    nodeElement.innerHTML = `
      <span class="toggle">${node.expanded ? icons.open : icons.close}</span>
      <span class="node-icon">${icons.parent}</span>
      <span class="node-label">${node.label}</span>
    `;
  } else {
    nodeElement.innerHTML = `
      <span class="toggle"></span>
      <span class="node-icon">${icons.leaf}</span>
      <span class="node-label">${node.label}</span>
    `;
  }
  
  return nodeElement;
}
```

## Icon Gallery

### Files and Folders

| Icon | Name | Usage |
|------|------|-------|
| <i class="fa-solid fa-folder"></i> | folder | `getIcon('folder')` |
| <i class="fa-solid fa-folder-open"></i> | folder-open | `getIcon('folder-open')` |
| <i class="fa-solid fa-folder-plus"></i> | folder-plus | `getIcon('folder-plus')` |
| <i class="fa-solid fa-file"></i> | file | `getIcon('file')` |
| <i class="fa-solid fa-file-alt"></i> | file-text | `getIcon('file-text')` |
| <i class="fa-solid fa-file-image"></i> | file-image | `getIcon('file-image')` |
| <i class="fa-solid fa-file-audio"></i> | file-audio | `getIcon('file-audio')` |
| <i class="fa-solid fa-file-video"></i> | file-video | `getIcon('file-video')` |
| <i class="fa-solid fa-file-archive"></i> | file-archive | `getIcon('file-archive')` |
| <i class="fa-solid fa-file-pdf"></i> | file-pdf | `getIcon('file-pdf')` |
| <i class="fa-solid fa-file-excel"></i> | file-excel | `getIcon('file-excel')` |
| <i class="fa-solid fa-file-word"></i> | file-word | `getIcon('file-word')` |
| <i class="fa-solid fa-file-powerpoint"></i> | file-powerpoint | `getIcon('file-powerpoint')` |
| <i class="fa-solid fa-file-code"></i> | file-code | `getIcon('file-code')` |
| <i class="fa-solid fa-file-csv"></i> | file-csv | `getIcon('file-csv')` |
| <i class="fa-solid fa-file-contract"></i> | file-contract | `getIcon('file-contract')` |
| <i class="fa-solid fa-file-download"></i> | file-download | `getIcon('file-download')` |
| <i class="fa-solid fa-file-export"></i> | file-export | `getIcon('file-export')` |
| <i class="fa-solid fa-file-import"></i> | file-import | `getIcon('file-import')` |

### Navigation Icons

| Icon | Name | Usage |
|------|------|-------|
| <i class="fa-solid fa-chevron-down"></i> | chevron-down | `getIcon('chevron-down')` |
| <i class="fa-solid fa-chevron-up"></i> | chevron-up | `getIcon('chevron-up')` |
| <i class="fa-solid fa-chevron-left"></i> | chevron-left | `getIcon('chevron-left')` |
| <i class="fa-solid fa-chevron-right"></i> | chevron-right | `getIcon('chevron-right')` |
| <i class="fa-solid fa-caret-down"></i> | caret-down | `getIcon('caret-down')` |
| <i class="fa-solid fa-caret-up"></i> | caret-up | `getIcon('caret-up')` |
| <i class="fa-solid fa-caret-left"></i> | caret-left | `getIcon('caret-left')` |
| <i class="fa-solid fa-caret-right"></i> | caret-right | `getIcon('caret-right')` |
| <i class="fa-solid fa-arrow-up"></i> | arrow-up | `getIcon('arrow-up')` |
| <i class="fa-solid fa-arrow-down"></i> | arrow-down | `getIcon('arrow-down')` |
| <i class="fa-solid fa-arrow-left"></i> | arrow-left | `getIcon('arrow-left')` |
| <i class="fa-solid fa-arrow-right"></i> | arrow-right | `getIcon('arrow-right')` |
| <i class="fa-solid fa-angle-double-left"></i> | angle-double-left | `getIcon('angle-double-left')` |
| <i class="fa-solid fa-angle-double-right"></i> | angle-double-right | `getIcon('angle-double-right')` |
| <i class="fa-solid fa-angle-left"></i> | angle-left | `getIcon('angle-left')` |
| <i class="fa-solid fa-angle-right"></i> | angle-right | `getIcon('angle-right')` |

### Action Icons

| Icon | Name | Usage |
|------|------|-------|
| <i class="fa-solid fa-plus"></i> | plus | `getIcon('plus')` |
| <i class="fa-solid fa-minus"></i> | minus | `getIcon('minus')` |
| <i class="fa-solid fa-edit"></i> | edit | `getIcon('edit')` |
| <i class="fa-solid fa-trash"></i> | trash | `getIcon('trash')` |
| <i class="fa-solid fa-save"></i> | save | `getIcon('save')` |
| <i class="fa-solid fa-copy"></i> | copy | `getIcon('copy')` |
| <i class="fa-solid fa-cut"></i> | cut | `getIcon('cut')` |
| <i class="fa-solid fa-paste"></i> | paste | `getIcon('paste')` |
| <i class="fa-solid fa-undo"></i> | undo | `getIcon('undo')` |
| <i class="fa-solid fa-redo"></i> | redo | `getIcon('redo')` |
| <i class="fa-solid fa-upload"></i> | upload | `getIcon('upload')` |
| <i class="fa-solid fa-download"></i> | download | `getIcon('download')` |
| <i class="fa-solid fa-link"></i> | link | `getIcon('link')` |
| <i class="fa-solid fa-unlink"></i> | unlink | `getIcon('unlink')` |
| <i class="fa-solid fa-eye"></i> | eye | `getIcon('eye')` |
| <i class="fa-solid fa-eye-slash"></i> | eye-slash | `getIcon('eye-slash')` |

### UI Element Icons

| Icon | Name | Usage |
|------|------|-------|
| <i class="fa-solid fa-check"></i> | check | `getIcon('check')` |
| <i class="fa-solid fa-check-circle"></i> | check-circle | `getIcon('check-circle')` |
| <i class="fa-solid fa-check-square"></i> | check-square | `getIcon('check-square')` |
| <i class="fa-solid fa-square"></i> | square | `getIcon('square')` |
| <i class="fa-solid fa-toggle-on"></i> | toggle-on | `getIcon('toggle-on')` |
| <i class="fa-solid fa-toggle-off"></i> | toggle-off | `getIcon('toggle-off')` |
| <i class="fa-solid fa-sliders"></i> | sliders | `getIcon('sliders')` |
| <i class="fa-solid fa-sort"></i> | sort | `getIcon('sort')` |
| <i class="fa-solid fa-sort-up"></i> | sort-up | `getIcon('sort-up')` |
| <i class="fa-solid fa-sort-down"></i> | sort-down | `getIcon('sort-down')` |
| <i class="fa-solid fa-filter"></i> | filter | `getIcon('filter')` |
| <i class="fa-solid fa-list"></i> | list | `getIcon('list')` |
| <i class="fa-solid fa-table"></i> | table | `getIcon('table')` |

### Functional Icons

| Icon | Name | Usage |
|------|------|-------|
| <i class="fa-solid fa-search"></i> | search | `getIcon('search')` |
| <i class="fa-solid fa-user"></i> | user | `getIcon('user')` |
| <i class="fa-solid fa-users"></i> | users | `getIcon('users')` |
| <i class="fa-solid fa-home"></i> | home | `getIcon('home')` |
| <i class="fa-solid fa-cog"></i> | cog | `getIcon('cog')` |
| <i class="fa-solid fa-cogs"></i> | cogs | `getIcon('cogs')` |
| <i class="fa-solid fa-bell"></i> | bell | `getIcon('bell')` |
| <i class="fa-solid fa-calendar"></i> | calendar | `getIcon('calendar')` |
| <i class="fa-solid fa-clock"></i> | clock | `getIcon('clock')` |
| <i class="fa-solid fa-map-marker"></i> | map-marker | `getIcon('map-marker')` |
| <i class="fa-solid fa-info"></i> | info | `getIcon('info')` |
| <i class="fa-solid fa-info-circle"></i> | info-circle | `getIcon('info-circle')` |
| <i class="fa-solid fa-question"></i> | question | `getIcon('question')` |
| <i class="fa-solid fa-question-circle"></i> | question-circle | `getIcon('question-circle')` |
| <i class="fa-solid fa-exclamation"></i> | exclamation | `getIcon('exclamation')` |
| <i class="fa-solid fa-exclamation-triangle"></i> | exclamation-triangle | `getIcon('exclamation-triangle')` |
| <i class="fa-solid fa-exclamation-circle"></i> | exclamation-circle | `getIcon('exclamation-circle')` |

## Notes

- When viewing this documentation in a browser with Font Awesome properly loaded, you should see the actual icons in the Icon Gallery.
- The icon tables use Font Awesome classes for visualization, but your application should use the `getIcon()` function to generate SVG icons.
- For the best performance, use SVG icons through the `getIcon()` function rather than Font Awesome classes.
