/* ------------------------------------------------------------------
 * Modern WTree implementation (ES2022+) *
 * Tree Components
    ├── WTree (simple, TreeJS-based)
    │   ├── Basic hierarchy display
    │   ├── Expand/collapse
    │   ├── Icons & checkboxes
    │   └── Simple selection
    │
    └── WTreeView (complex, SlickGrid-based)
        ├── All Tree features
        ├── Column management
        ├── Virtualization
        ├── Advanced sorting/filtering
        └── Complex drag & drop
 * 
 * Highlights
 * ==========
 * • Custom events dispatched on nodes & the view for easy integration
 * • Accessibility: ARIA roles + keyboard navigation helpers
 * • Async data‑loader hook for lazy children
 *
 * Missing features still TODO (next iterations):
 *   – Shift‑click range selection ?
 *   – basic Drag & drop re‑ordering
 *   – simple Search / filter API
 * ------------------------------------------------------------------ */

import { getIcon, getTreeIcons } from './icons.esm.js';


// ------------------------------------------------------------
// Utility helpers
// ------------------------------------------------------------
const isDOM = obj => obj instanceof HTMLElement;

const defaultIcons = {
  leaf: "<span>&#128441;</span>",
  parent: "<span>&#128449;</span>",
  open: "<span>&#9698;</span>",
  close: "<span>&#9654;</span>"
};

/**
 * Walk the subtree depth‑first and execute callback.
 * @param {WTreeNode} node
 * @param {(n:WTreeNode)=>void} cb
 */
function walk(node, cb) {
  cb(node);
  node.children.forEach(child => walk(child, cb));
}

// ------------------------------------------------------------
// TreeNode – single node in the tree
// ------------------------------------------------------------
export class WTreeNode {
  /** @type {WTreeNode[]} */ #children = [];
  #userObject;
  #expanded;
  #enabled;
  #selected;
  #options;
  /** @type {Map<string, Function>} */ #events = new Map();
  /** @type {WTreeNode|undefined} */ parent;

  /**
   * @param {string|Object} userObject
   * @param {Object} [options]
   */
  constructor(userObject = "", options = {}) {
    if (typeof userObject !== "string" && typeof userObject.toString !== "function") {
      throw new TypeError("userObject must be string or implement toString()");
    }

    this.#userObject = userObject;
    this.#options = options;
    this.#expanded = options.expanded ?? true;
    this.#enabled = options.enabled ?? true;
    this.#selected = options.selected ?? false;
  }

  // ----------------- Children -----------------
  get children() {
    return this.#children;
  }

  addChild(node) {
    if (!(node instanceof WTreeNode)) throw new TypeError("child must be TreeNode");
    if (!this.allowsChildren) {
      console.warn("allowsChildren is false → child not added");
      return;
    }
    node.parent = this;
    this.#children.push(node);
  }

  removeChild(node) {
    const idx = this.#children.indexOf(node);
    if (idx !== -1) this.#children.splice(idx, 1);
  }

  /**
   * Remove child at index.
   * @param {number} idx
   */
  removeChildAt(idx) {
    this.#children.splice(idx, 1);
  }

  get allowsChildren() {
    return this.#options.allowsChildren ?? true;
  }

  // ----------------- State flags -----------------
  get expanded() {
    return this.isLeaf ? true : this.#expanded;
  }
  set expanded(v) {
    if (this.isLeaf) return;
    if (this.#expanded === v) return;
    this.#expanded = v;
    this.dispatch(v ? "expand" : "collapse");
    this.dispatch("toggle_expanded");
  }

  toggleExpanded() {
    this.expanded = !this.expanded;
  }

  get enabled() {
    return this.#enabled;
  }
  set enabled(v) {
    if (this.#enabled === v) return;
    this.#enabled = v;
    this.dispatch(v ? "enable" : "disable");
    this.dispatch("toggle_enabled");
  }

  toggleEnabled() {
    this.enabled = !this.enabled;
  }

  get selected() {
    return this.#selected;
  }
  set selected(v) {
    if (this.#selected === v) return;
    this.#selected = v;
    this.dispatch(v ? "select" : "deselect");
    this.dispatch("toggle_selected");
  }

  toggleSelected() {
    this.selected = !this.selected;
  }

  get isLeaf() {
    return this.#children.length === 0;
  }

  // ----------------- Data -----------------
  get userObject() {
    return this.#userObject;
  }
  set userObject(v) {
    if (typeof v !== "string" && typeof v.toString !== "function") {
      throw new TypeError("userObject must be string or implement toString()");
    }
    this.#userObject = v;
  }

  get options() {
    return this.#options;
  }

  set options(obj) {
    if (typeof obj !== "object") throw new TypeError("options must be object");
    this.#options = obj;
  }

  /**
   * @param {string} key
   * @param {*} value
   */
  setOption(key, value) {
    this.#options[key] = value;
  }

  /**
   * String representation (what gets rendered).
   */
  toString() {
    return typeof this.#userObject === "string" ? this.#userObject : this.#userObject.toString();
  }

  /**
   * Event API (simple)
   */
  on(event, cb) {
    if (typeof cb !== "function") throw new TypeError("callback must be function");
    this.#events.set(event, cb);
    return this; // chainable
  }

  dispatch(event, ...args) {
    const fn = this.#events.get(event);
    if (fn) fn(...args, this);
  }

  /**
   * Deep equals by user object reference.
   * Extend if you need stricter check.
   */
  equals(other) {
    return other instanceof WTreeNode && other.userObject === this.userObject;
  }
}

// ------------------------------------------------------------
// TreeView – visual component
// ------------------------------------------------------------
export class WTree {
  #root;
  /** @type {HTMLElement|null} */ #container;
  #options;

  /**
   * @param {WTreeNode} root
   * @param {HTMLElement|string} [container]
   * @param {Object} [options]
   */
  constructor(root, container = null, options = { 
    show_root: true,
    open_icon: getIcon('chevron-down', '#666'),
    close_icon: getIcon('chevron-right', '#666'),
    leaf_icon: getIcon('file-text', '#999'),
    parent_icon: getIcon('folder', '#f8d775')
  }) 
  {
    if (!(root instanceof WTreeNode)) throw new TypeError("root must be TreeNode");
    this.#root = root;

    this.#options = options;
    // Resolve container early so reload() won’t crash.
    if (container) this.container = container;


    // Initial render (if container resolved)
    if (this.#container) this.reload();

    if (this.#options.checkboxes) {
      this.#updateParentCheckboxStates(this.#root);
    }
  }

  // ----------------- Properties -----------------
  get root() {
    return this.#root;
  }
  set root(node) {
    if (!(node instanceof WTreeNode)) throw new TypeError("root must be TreeNode");
    this.#root = node;
    this.reload();
  }

  get container() {
    return this.#container;
  }
  set container(el) {
    if (typeof el === "string") el = document.querySelector(el);
    if (!isDOM(el)) throw new TypeError("container must be HTMLElement or selector");
    this.#container = el;
    this.reload();
  }

  get options() {
    return this.#options;
  }

  setOption(key, value) {
    this.#options[key] = value;
    this.reload();
  }

  // ----------------- Public API -----------------
  expandAll() {
    walk(this.#root, n => (n.expanded = true));
    this.reload();
  }

  collapseAll() {
    walk(this.#root, n => (n.expanded = false));
    this.reload();
  }

  /**
   * Expand nodes along a TreePath or array of nodes.
   * @param {WTreeNode[]} path
   */
  expandPath(path) {
    path.forEach(n => (n.expanded = true));
    this.reload();
  }

  /**
   * Retrieve all currently selected TreeNodes.
   * @returns {WTreeNode[]}
   */
  get selectedNodes() {
    const out = [];
    walk(this.#root, n => n.selected && out.push(n));
    return out;
  }

  // ----------------- Rendering -----------------
  /**
   * Full re‑render of the tree into the container.
   */
  reload() {
    if (!this.#container) {
      console.warn("TreeView: no container set");
      return;
    }

    this.#container.classList.add("tj_container");
    this.#container.replaceChildren(this.#renderTree());
  }

  /**
   * @returns {HTMLUListElement}
   */
  #renderTree() {
    const ul = document.createElement("ul");
    const showRoot = this.#options.show_root ?? true;

    if (showRoot) {
      ul.appendChild(this.#renderNode(this.#root));
    } else {
      this.#root.children.forEach(child => ul.appendChild(this.#renderNode(child)));
    }

    return ul;
  }
  /**
   * Updates checkbox state for a node and all its children
   * @private
   */
  #updateCheckboxStates(node, checked) {
    // Update this node's selection state
    node.selected = checked;
    
    // Cascade to children
    if (!node.isLeaf) {
      node.children.forEach(child => {
        if (child.enabled) {
          this.#updateCheckboxStates(child, checked);
        }
      });
    }
  }

  /**
   * Updates parent checkbox states based on children's states
   * @private
   */
  #updateParentCheckboxStates(node) {
    // Process from leaves up to root
    if (!node.isLeaf) {
      // First update all descendants
      node.children.forEach(child => {
        this.#updateParentCheckboxStates(child);
      });
      
      // Then determine this node's state based on its children
      const enabledChildren = node.children.filter(c => c.enabled);
      const selectedCount = enabledChildren.filter(c => c.selected || c._indeterminate).length;
      const totalCount = node.children.length;

      // console.log("Updating checkbox state for", node, "→", {
      //   enabledChildren: enabledChildren.length,
      //   selectedCount,
      //   totalCount,
      // });
      
      if (selectedCount === 0) {
        // None selected
        node.selected = false;
        node._indeterminate = false;
      } else if (selectedCount === totalCount) {
        // All selected
        node.selected = true;
        node._indeterminate = false;
      } else {
        // Some selected - tri-state
        node.selected = false;
        node._indeterminate = true;
      }
    }
  }


  /**
   * @param {WTreeNode} node
   * @returns {HTMLLIElement}
   */
  #renderNode(node) {
    const li = document.createElement("li");
    const span = document.createElement("span");
    span.className = "tj_description";
    span._node = node; // non‑standard prop, fine in modern JS

    // Disabled
    if (!node.enabled) {
      li.setAttribute("aria-disabled", "true");
      node.expanded = false;
      node.selected = false;
    }

    // Selected styling
    if (node.selected) span.classList.add("selected");

    // Add checkbox if enabled
    let checkbox = null;
    if (this.#options.checkboxes || node.options.checkboxes ||true) {
      checkbox = document.createElement("input");
      checkbox.type = "checkbox";
      checkbox.className = "tj_checkbox";
      checkbox.checked = node.selected;
      checkbox.disabled = !node.enabled;
      
      if (node._indeterminate) {
        checkbox.indeterminate = true;
      }

      // Handle checkbox click to toggle selection
      checkbox.addEventListener("click", (e) => {
        e.stopPropagation(); // Prevent node click handler
        if (node.enabled) {
          // Update this node and all children
          this.#updateCheckboxStates(node, checkbox.checked);
          
          // Update all parent nodes
          let parent = node.parent;
          while (parent) {
            this.#updateParentCheckboxStates(parent);
            parent = parent.parent;
          }
          
          this.reload(); // Refresh the entire tree
        }
      });
      
      // Append checkbox before other content
      span.appendChild(checkbox);
    }

    // Click + selection
    span.addEventListener("click", e => {
      if (!node.enabled) return;
      const isMulti = e.ctrlKey || e.metaKey;

      if (!node.isLeaf) {
        node.toggleExpanded();
      } else {
        node.dispatch("open", e);
      }

      if (this.#options.checkboxes) {
        // Toggle checkbox
        //node.selected = !node.selected;
        //if (checkbox) checkbox.checked = node.selected;
      } else if (isMulti) {
        node.toggleSelected();
      } else {
        // Deselect others
        // this.selectedNodes.forEach(n => (n.selected = false));
        // node.selected = true;
      }

      this.reload();
    });

    // Context menu
    span.addEventListener("contextmenu", e => {
      e.preventDefault();
      node.dispatch("contextmenu", e);
    });

    // Build innerHTML (icons + label)
    const icons = [];
    if (!node.isLeaf || node.options.forceParent) {
      icons.push(`<span class="tj_mod_icon">${node.expanded ? (this.#options.open_icon ?? defaultIcons.open) : (this.#options.close_icon ?? defaultIcons.close)}</span>`);
    }

    const customIcon = node.options.icon ?? this.#options[(node.isLeaf ? "leaf_icon" : "parent_icon")] ?? defaultIcons[node.isLeaf ? "leaf" : "parent"];
    icons.push(`<span class="tj_icon">${customIcon}</span>`);

    //span.innerHTML = `${icons.join("")}${node}`;
    const fragment = document.createRange().createContextualFragment(`${icons.join("")}${node}`);
    span.appendChild(fragment);
    if (node.isLeaf && !node.options.forceParent) span.classList.add("tj_leaf");

    li.appendChild(span);

    // Children
    if (!node.isLeaf && node.expanded) {
      const ul = document.createElement("ul");
      node.children.forEach(child => ul.appendChild(this.#renderNode(child)));
      li.appendChild(ul);
    }

    return li;
  }
}
