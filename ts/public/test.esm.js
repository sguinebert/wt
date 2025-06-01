import { computePosition, offset, flip, shift } from './vendor/floating-ui/floating-ui.core.browser.min.mjs';
import WDialog from './js/WDialog.esm.js';
import WPopupWidget from './js/WPopupWidget.esm.js';
import { attachTooltip } from './js/ToolTip.esm.js';
import openPopupWindow from './js/PopupWindow.esm.js';

if (typeof window.Wt === 'undefined') {
  window.Wt = {
    emit: function(el, eventName, ...args) {
      // Optional: dispatch a DOM event instead
      if (typeof eventName === 'string') {
        const event = new CustomEvent(eventName, { 
          detail: { args } 
        });
        el.dispatchEvent(event);
      }
      // For debugging
      console.log(`Wt event '${eventName}' would be emitted with args:`, args);
    }
  };
}

// New widget imports
import WLineEdit from './js/WLineEdit.esm.js';
import WSpinBox from './js/WSpinBox.esm.js';
import WTimeEdit from './js/WTimeEdit.esm.js';
import WSuggestionPopup from './js/WSuggestionPopup.esm.js';
import WDateEdit from './js/WDateEdit.esm.js';

import WTableView, { WtFormatters, Editors, Formatters } from './js/WTableView.esm.js';
// import Sortable from './vendor/sortablejs/sortable.core.esm.js';
// window.Sortable = Sortable;
// import {   Editors,
//   Formatters,
//   SlickGlobalEditorLock,
//   SlickRowSelectionModel,
//   SlickColumnPicker,
//   SlickDataView,
//   SlickGridMenu,
//   SlickGridPager,
//   SlickGrid,
//   Utils, } from '../vendor/slickgrid/slick.grid.esm.min.js';
//import './vendor/slickgrid/slick-alpine-theme.min.css';

import { Chart } from './vendor/chartjs/chartjs/auto/auto.js';



function addStylesheet(href) {
  // Skip if the sheet is already present
  if (!document.head.querySelector(`link[rel="stylesheet"][href="${href}"]`)) {
    const link = document.createElement('link');
    link.rel  = 'stylesheet';
    link.href = href;
    document.head.appendChild(link);
  }
}
addStylesheet('./vendor/slickgrid/slick-alpine-theme.min.css');

export function initGallery() {
  // Create main layout structure
  const container = document.createElement('div');
  container.id = 'widgetGallery';
  container.classList.add('Wt-domRoot');
  container.innerHTML = `
    <div class="gallery-container">
      <header class="gallery-header">
        <h1>Wt Frontend Widget Gallery</h1>
        <p>JavaScript implementations of Wt widgets for modern browsers</p>
      </header>
      
      <div class="gallery-layout">
        <nav class="gallery-sidebar">
          <ul>
            <li><a href="#basic" class="active">Basic Widgets</a></li>
            <li><a href="#input">Input Widgets</a></li>
            <li><a href="#popup">Popup Widgets</a></li>
            <li><a href="#date">Date & Time Widgets</a></li>
            <li><a href="#dialog">Dialog Widgets</a></li>
            <li><a href="#grid">Data Grid</a></li>
            <li><a href="#chart">Charts</a></li>
          </ul>
        </nav>
        
        <main class="gallery-content">
          <section id="basic" class="widget-section">
            <h2>Basic Widgets</h2>
            <div class="widget-grid"></div>
          </section>
          
          <section id="input" class="widget-section">
            <h2>Input Widgets</h2>
            <div class="widget-grid"></div>
          </section>
          
          <section id="popup" class="widget-section">
            <h2>Popup Widgets</h2>
            <div class="widget-grid"></div>
          </section>
          
          <section id="date" class="widget-section">
            <h2>Date & Time Widgets</h2>
            <div class="widget-grid"></div>
          </section>
          
          <section id="dialog" class="widget-section">
            <h2>Dialog Widgets</h2>
            <div class="widget-grid"></div>
          </section>

          <section id="stree" class="widget-section">
            <h2>Data tree</h2>
            <div class="widget-grid"></div>
          </section>

          <section id="grid" class="widget-section">
            <h2>Data Grid</h2>
            <div class="widget-grid"></div>
          </section>

          <section id="tree" class="widget-section">
            <h2>Data tree</h2>
            <div class="widget-grid"></div>
          </section>

          <section id="chart" class="widget-section">
            <h2>Chart Widgets</h2>
            <div class="widget-grid"></div>
          </section>
        </main>
      </div>
    </div>
  `;
  
  // Add styles
  const style = document.createElement('style');
  style.textContent = `
    .gallery-container {
      font-family: system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      max-width: 1200px;
      margin: 0 auto;
      padding: 20px;
      color: #333;
    }
    
    .gallery-header {
      text-align: center;
      margin-bottom: 40px;
      padding-bottom: 20px;
      border-bottom: 1px solid #eee;
    }
    
    .gallery-header h1 {
      margin-bottom: 10px;
      color: #2c3e50;
    }
    
    .gallery-layout {
      display: flex;
      gap: 30px;
    }
    
    .gallery-sidebar {
      width: 200px;
      flex-shrink: 0;
    }
    
    .gallery-sidebar ul {
      list-style: none;
      padding: 0;
      margin: 0;
      position: sticky;
      top: 20px;
    }
    
    .gallery-sidebar a {
      display: block;
      padding: 10px 15px;
      border-radius: 4px;
      color: #555;
      text-decoration: none;
      margin-bottom: 5px;
      transition: background-color 0.2s;
    }
    
    .gallery-sidebar a:hover, .gallery-sidebar a.active {
      background-color: #f0f0f0;
      color: #3498db;
    }
    
    .gallery-content {
      flex-grow: 1;
    }
    
    .widget-section {
      margin-bottom: 50px;
    }
    
    .widget-section h2 {
      border-bottom: 2px solid #f0f0f0;
      padding-bottom: 10px;
      margin-bottom: 20px;
      color: #2c3e50;
    }
    
    .widget-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
      gap: 20px;
    }
    
    .widget-card {
      border: 1px solid #eee;
      border-radius: 8px;
      padding: 20px;
      background: white;
      box-shadow: 0 2px 5px rgba(0,0,0,0.05);
    }
    
    .widget-card h3 {
      margin-top: 0;
      font-size: 1.1rem;
      color: #3498db;
      border-bottom: 1px solid #f0f0f0;
      padding-bottom: 10px;
      margin-bottom: 15px;
    }
    
    .widget-demo {
      margin-bottom: 15px;
    }
    
    .widget-description {
      font-size: 0.9rem;
      color: #666;
      margin-top: 45px;
    }
  `;
  style.textContent += `
  .full-width-card {
    grid-column: 1 / -1;
  }
  .slick-header-column,
  .slick-cell {
    box-sizing: border-box;
    padding: 0 4px;                 /* match SlickGrid’s default inner padding */
    border-left: 1px solid transparent;
    border-right: 1px solid transparent;
  }
  
.grid-container {
  width: 100%;         /* full width of parent */
  height: 400px;       /* or whatever fixed height you need */
  position: relative;  /* required by SlickGrid */
}

`;

style.textContent += `
  .chart-container {
    height: 250px;
    position: relative;
  }
  
  /* Make the full-width card taller for better chart display */
  .full-width-chart {
    grid-column: 1 / -1;
    height: 400px;
  }
  
  /* Add some spacing for chart legends */
  canvas {
    margin-bottom: 10px;
  }
`;

style.textContent += `
  .tree-controls {
    margin-bottom: 15px;
    display: flex;
    gap: 10px;
  }
  
  .tree-controls button {
    padding: 5px 10px;
    background: #3498db;
    color: white;
    border: none;
    border-radius: 4px;
    cursor: pointer;
  }
  
  .tree-controls button:hover {
    background: #2980b9;
  }
  
  /* Tree node styling */
  .wt-tree-toggle {
    cursor: pointer;
    margin-right: 5px;
    display: inline-block;
  }
  
  .wt-tree-toggle.expanded:before {
    content: "▼";
    font-size: 10px;
  }
  
  .wt-tree-toggle.collapsed:before {
    content: "►";
    font-size: 10px;
  }
  
  .wt-tree-leaf {
    display: inline-block;
    width: 12px;
    margin-right: 5px;
  }
  
  .wt-tree-icon {
    margin-right: 5px;
  }
`;
  document.head.appendChild(style);
  document.body.appendChild(container);
  
  // Initialize sections
  initBasicWidgets();
  initInputWidgets();
  initPopupWidgets();
  initDateTimeWidgets();
  initDialogWidgets();
  initSimpleTreeExample();
  initGridWidgets();
  initTreeViewExample();
  initChartWidgets();
  initGridLayoutTest();
  
  // Set up navigation
  const navLinks = document.querySelectorAll('.gallery-sidebar a');
  navLinks.forEach(link => {
    link.addEventListener('click', (e) => {
      // Update active state
      navLinks.forEach(l => l.classList.remove('active'));
      link.classList.add('active');
      
      // Scroll to section
      const targetId = link.getAttribute('href').substring(1);
      const targetSection = document.getElementById(targetId);
      if (targetSection) {
        targetSection.scrollIntoView({ behavior: 'smooth' });
      }
      e.preventDefault();
    });
  });
}

function initBasicWidgets() {
  const container = document.querySelector('#basic .widget-grid');
  
  // Basic button example
  const buttonCard = createWidgetCard(
    'Button',
    `<button class="wt-button">Click Me</button>`,
    'Standard button with click handling'
  );
  buttonCard.querySelector('button').addEventListener('click', () => {
    alert('Button clicked!');
  });
  container.appendChild(buttonCard);
  
  // Tooltip example
  const tooltipCard = createWidgetCard(
    'Tooltip',
    `<button id="tooltipButton" class="wt-button">Hover Me</button>`,
    'Tooltip appears on hover'
  );
  container.appendChild(tooltipCard);
  attachTooltip(tooltipCard.querySelector('#tooltipButton'), {
    content: 'This is a tooltip!'
  });
}

function initInputWidgets() {
  const container = document.querySelector('#input .widget-grid');
  
  // WLineEdit with length validator
  const lineEditCard = createWidgetCard(
    'WLineEdit with Length Validator',
    `<div class="wt-field-container">
      <label for="nameInput">Name (3-10 chars):</label>
      <input type="text" id="nameInput" class="wt-line-edit" placeholder="Enter name">
      <div class="validation-message"></div>
    </div>`,
    'Text input with length validation'
  );
  
  const nameInput = lineEditCard.querySelector('#nameInput');
  const validationMsg = lineEditCard.querySelector('.validation-message');
  
  // Create a length validator (3-10 characters)
  const validateLength = (value) => {
    if (value.length < 3) {
      return { valid: false, message: 'Too short (min 3 characters)' };
    } else if (value.length > 10) {
      return { valid: false, message: 'Too long (max 10 characters)' };
    }
    return { valid: true, message: 'Valid input' };
  };
  
  nameInput.addEventListener('input', () => {
    const result = validateLength(nameInput.value);
    validationMsg.textContent = result.message;
    validationMsg.style.color = result.valid ? 'green' : 'red';
  });
  
  container.appendChild(lineEditCard);
  
  // WLineEdit with double validator
  const doubleCard = createWidgetCard(
    'WLineEdit with Double Validator',
    `<div class="wt-field-container">
      <label for="numberInput">Number (0.0-100.0):</label>
      <input type="text" id="numberInput" class="wt-line-edit" placeholder="Enter number">
      <div class="validation-message"></div>
    </div>`,
    'Text input that validates decimal numbers'
  );
  
  const numberInput = doubleCard.querySelector('#numberInput');
  const numValidationMsg = doubleCard.querySelector('.validation-message');
  
  // Create a double validator (0.0-100.0)
  const validateDouble = (value) => {
    const num = parseFloat(value);
    if (isNaN(num)) {
      return { valid: false, message: 'Not a valid number' };
    } else if (num < 0 || num > 100) {
      return { valid: false, message: 'Number must be between 0 and 100' };
    }
    return { valid: true, message: 'Valid number' };
  };
  
  numberInput.addEventListener('input', () => {
    const result = validateDouble(numberInput.value);
    numValidationMsg.textContent = result.message;
    numValidationMsg.style.color = result.valid ? 'green' : 'red';
  });
  
  container.appendChild(doubleCard);
  
  // WSpinBox
  const spinBoxCard = createWidgetCard(
    'WSpinBox',
    `<div class="spinbox-demo">
      <h4>Basic Integer SpinBox</h4>
      <div class="field-container">
        <label>Quantity:</label>
        <input type="number" id="basicSpinBox" value="5" min="0" max="10" step="1">
        <div><small>Value: <span id="basicValue">5</span></small></div>
      </div>

      <h4>Decimal SpinBox with Prefix/Suffix</h4>
      <div class="field-container">
        <label>Price:</label>
        <input type="number" id="priceSpinBox" value="24.99" min="0" max="100" step="0.25">
        <div><small>Value: <span id="priceValue">$24.99</span></small></div>
      </div>

      <h4>SpinBox with Wrapping</h4>
      <div class="field-container">
        <label>Hours (0-23):</label>
        <input type="number" id="hourSpinBox" value="12" min="0" max="23" step="1">
        <div><small>Value: <span id="hourValue">12</span></small></div>
      </div>
    </div>
    <style>
      .spinbox-demo .field-container {
        margin-bottom: 15px;
      }
      .spinbox-demo input {
        width: 120px;
      }
    </style>`,
    'Numeric input with increment/decrement controls, keyboard navigation, and mouse wheel support'
  );
  container.appendChild(spinBoxCard);
  
  // Initialize the SpinBox widgets
  const basicSpin = new WSpinBox({
    input: document.getElementById('basicSpinBox'),
    min: 0,
    max: 10,
    step: 1
  });

  const priceSpin = new WSpinBox({
    input: document.getElementById('priceSpinBox'),
    precision: 2,
    prefix: '$ ',
    min: 0,
    max: 100,
    step: 0.25
  });

  const hourSpin = new WSpinBox({
    input: document.getElementById('hourSpinBox'),
    min: 0,
    max: 23,
    step: 1,
    wrap: true // Enables wrap-around instead of clamping
  });
    
  // Update value displays
  basicSpin.onChange = (value, finished) => {
    document.getElementById('basicValue').textContent = value;
  };

  priceSpin.onChange = (value, finished) => {
    document.getElementById('priceValue').textContent = `$${value.toFixed(2)}`;
  };

  hourSpin.onChange = (value, finished) => {
    document.getElementById('hourValue').textContent = value;
  };
  // Add a feature explanation section
  const spinBoxInfoCard = createWidgetCard(
    'WSpinBox Features',
    `<div class="feature-list">
      <ul>
        <li><strong>Keyboard Navigation</strong>: Press Up/Down arrow keys to increment/decrement values</li>
        <li><strong>Mouse Wheel</strong>: Hover over input and use mouse wheel to change values</li>
        <li><strong>Drag Controls</strong>: Click and drag in the arrow zone (rightmost 16px) to change values</li>
        <li><strong>Number Formatting</strong>: Locale-aware formatting for decimal and group separators</li>
        <li><strong>Prefix/Suffix</strong>: Add text before or after the number (like currency symbols)</li>
        <li><strong>Precision Control</strong>: Set decimal places for floating-point values</li>
        <li><strong>Min/Max Limits</strong>: Constrain input within specified ranges</li>
        <li><strong>Wrap Mode</strong>: Optionally wrap around instead of clamping at limits</li>
      </ul>
    </div>`,
    'Try out these interactions on the examples above'
  );
  container.appendChild(spinBoxInfoCard);
}

function initPopupWidgets() {
  const container = document.querySelector('#popup .widget-grid');
  
  // Popup window
  const popupWindowCard = createWidgetCard(
    'Popup Window',
    `<button id="openPopupBtn" class="wt-button">Open Window</button>`,
    'Opens an external browser window'
  );
  
  popupWindowCard.querySelector('#openPopupBtn').addEventListener('click', () => {
    openPopupWindow('https://google.com', 500, 500);
  });
  
  container.appendChild(popupWindowCard);
  
  // WPopupWidget with Floating UI
  const popupWidgetCard = createWidgetCard(
    'WPopupWidget',
    `<div class="popup-container">
      <button id="showPopupBtn" class="wt-button">Show Popup</button>
      <div id="demoPopup" style="display: none; background: white; padding: 10px; border: 1px solid #ccc; box-shadow: 0 2px 10px rgba(0,0,0,0.1); border-radius: 4px; width: 200px;">
        <h4>Popup Content</h4>
        <p>This is positioned with Floating UI.</p>
      </div>
    </div>`,
    'Popup widget that appears near its trigger'
  );
  
  const popupBtn = popupWidgetCard.querySelector('#showPopupBtn');
  const popupEl = popupWidgetCard.querySelector('#demoPopup');
  let menuVisible = false;
  let cleanup = null;
  
  const platform = {
    isRTL: () => document.documentElement.dir === 'rtl',
    getElementRects: ({ reference, floating }) => ({
      reference: reference.getBoundingClientRect(),
      floating: floating.getBoundingClientRect()
    }),
    getDimensions: (element) => {
      const rect = element.getBoundingClientRect();
      return { width: rect.width, height: rect.height };
    },
    getClippingRect: () => ({
      width: window.innerWidth, height: window.innerHeight, x: 0, y: 0
    })
  };
  
  popupBtn.addEventListener('click', () => {
    menuVisible = !menuVisible;
    
    if (menuVisible) {
      popupEl.style.display = 'block';
      popupEl.style.position = 'fixed';
      
      const updatePosition = () => {
        computePosition(popupBtn, popupEl, {
          platform,
          strategy: 'fixed', 
          placement: 'bottom',
          middleware: [offset(8), flip(), shift()]
        }).then(({x, y}) => {
          Object.assign(popupEl.style, { 
            left: `${x}px`, top: `${y}px`
            // left: '0px', top: '0px',
            // willChange: 'transform',
            // transform: `translate3d(${x}px, ${y}px, 0)`
          });
        });
      };
      
      updatePosition();
      
      const resizeObserver = new ResizeObserver(() => updatePosition());
      resizeObserver.observe(popupBtn);
      resizeObserver.observe(document.body);
      
      const scrollListener = () => updatePosition();
      window.addEventListener('scroll', scrollListener, true);
      
      let previousRect = popupBtn.getBoundingClientRect();
      const layoutShiftObserver = new IntersectionObserver(() => {
        const currentRect = popupBtn.getBoundingClientRect();
        if (previousRect.x !== currentRect.x || previousRect.y !== currentRect.y) {
          updatePosition();
          previousRect = currentRect;
        }
      }, { threshold: 0.5 });
      layoutShiftObserver.observe(popupBtn);
      
      cleanup = () => {
        layoutShiftObserver.disconnect();
        resizeObserver.disconnect();
        window.removeEventListener('scroll', scrollListener, true);
      };
    } else {
      popupEl.style.display = 'none';
      if (cleanup) {
        cleanup();
        cleanup = null;
      }
    }
  });
  
  container.appendChild(popupWidgetCard);
  
  // WSuggestionPopup
  const suggestionCard = createWidgetCard(
    'WSuggestionPopup',
    `<div class="suggestion-container">
      <label for="fruitInput">Fruit:</label>
      <input type="text" id="fruitInput" class="wt-line-edit" placeholder="Type a fruit">
      <div id="suggestionPopup" class="suggestion-popup" style="display: none;"></div>
    </div>`,
    'Dropdown suggestions as you type'
  );
  
  const fruitInput = suggestionCard.querySelector('#fruitInput');
  const suggestionPopup = suggestionCard.querySelector('#suggestionPopup');
  
  const fruits = ['Apple', 'Apricot', 'Avocado', 'Banana', 'Blackberry', 'Blueberry', 
                 'Cherry', 'Coconut', 'Grape', 'Kiwi', 'Lemon', 'Lime', 'Mango', 
                 'Orange', 'Peach', 'Pear', 'Pineapple', 'Plum', 'Raspberry', 'Strawberry'];
  
      
  const updatePos = () => {
      // Position suggestion popup
      computePosition(fruitInput, suggestionPopup, {
        platform,
        strategy: 'fixed', 
        placement: 'bottom-start',
        middleware: [offset(2), flip(), shift()]
      }).then(({x, y}) => {
        Object.assign(suggestionPopup.style, {
          position: 'fixed',
          width: `${fruitInput.offsetWidth}px`,
          left: `${x}px`, top: `${y}px`,
          // left: '0px', top: '0px',
          // willChange: 'transform',
          // transform: `translate3d(${x}px, ${y}px, 0)`
        });
      });
  };
  // const resizeObserver = new ResizeObserver(() => updatePosition());
  // resizeObserver.observe(popupBtn);
  // resizeObserver.observe(document.body);
  window.addEventListener('scroll', () => { updatePos(); }, true);

  fruitInput.addEventListener('input', () => {
    const value = fruitInput.value.toLowerCase();
    if (value.length < 1) {
      suggestionPopup.style.display = 'none';
      return;
    }
    
    const matches = fruits.filter(f => f.toLowerCase().includes(value));
    if (matches.length > 0) {
      suggestionPopup.innerHTML = matches.map(m => 
        `<div class="suggestion-item">${m}</div>`
      ).join('');
      
      suggestionPopup.style.display = 'block';
      
      updatePos();
      
      // Add click handlers to suggestions
      suggestionPopup.querySelectorAll('.suggestion-item').forEach(item => {
        item.addEventListener('click', () => {
          fruitInput.value = item.textContent;
          suggestionPopup.style.display = 'none';
        });
      });
    } else {
      suggestionPopup.style.display = 'none';
    }
  });
  
  // Hide suggestions when clicking outside
  document.addEventListener('click', (e) => {
    if (e.target !== fruitInput && !suggestionPopup.contains(e.target)) {
      suggestionPopup.style.display = 'none';
    }
  });
  
  container.appendChild(suggestionCard);
}

function initDateTimeWidgets() {
  const container = document.querySelector('#date .widget-grid');
  
  // WTimeEdit
  const timeEditCard = createWidgetCard(
    'WTimeEdit',
    `<div class="wt-time-edit">
      <input type="text" class="time-hour" value="12" maxlength="2">
      <span>:</span>
      <input type="text" class="time-minute" value="00" maxlength="2">
      <span class="ampm-container">
        <select class="time-ampm">
          <option value="AM">AM</option>
          <option value="PM">PM</option>
        </select>
      </span>
    </div>`,
    'Time input with hours and minutes'
  );
  
  const hourInput = timeEditCard.querySelector('.time-hour');
  const minuteInput = timeEditCard.querySelector('.time-minute');
  
  hourInput.addEventListener('input', () => {
    let value = parseInt(hourInput.value);
    if (isNaN(value) || value < 1) hourInput.value = '1';
    if (value > 12) hourInput.value = '12';
  });
  
  minuteInput.addEventListener('input', () => {
    let value = parseInt(minuteInput.value);
    if (isNaN(value) || value < 0) minuteInput.value = '00';
    if (value > 59) minuteInput.value = '59';
    if (minuteInput.value.length === 1) minuteInput.value = '0' + minuteInput.value;
  });
  
  container.appendChild(timeEditCard);
  
  // WDateEdit
  const dateEditCard = createWidgetCard(
    'WDateEdit',
    `<div class="wt-date-edit">
      <input type="text" class="date-input" placeholder="YYYY-MM-DD">
      <button class="calendar-button">📅</button>
      <div class="calendar-popup" style="display: none;">
        <div class="calendar-header">
          <button class="prev-month">◀</button>
          <div class="current-month">May 2025</div>
          <button class="next-month">▶</button>
        </div>
        <div class="calendar-grid">
          <div class="weekday">Su</div>
          <div class="weekday">Mo</div>
          <div class="weekday">Tu</div>
          <div class="weekday">We</div>
          <div class="weekday">Th</div>
          <div class="weekday">Fr</div>
          <div class="weekday">Sa</div>
          <!-- Days go here -->
        </div>
      </div>
    </div>`,
    'Date input with popup calendar'
  );
  
  const dateInput = dateEditCard.querySelector('.date-input');
  const calendarBtn = dateEditCard.querySelector('.calendar-button');
  const calendarPopup = dateEditCard.querySelector('.calendar-popup');
  const calendarGrid = dateEditCard.querySelector('.calendar-grid');
  const currentMonthEl = dateEditCard.querySelector('.current-month');
  const prevMonthBtn = dateEditCard.querySelector('.prev-month');
  const nextMonthBtn = dateEditCard.querySelector('.next-month');
  
  // Date validation
  dateInput.addEventListener('input', () => {
    const value = dateInput.value;
    const dateRegex = /^\d{4}-\d{2}-\d{2}$/;
    
    if (value && !dateRegex.test(value)) {
      dateInput.style.borderColor = 'red';
      return;
    }
    
    const date = new Date(value);
    if (value && (isNaN(date) || date.toString() === 'Invalid Date')) {
      dateInput.style.borderColor = 'red';
    } else {
      dateInput.style.borderColor = '';
    }
  });
  
  // Calendar rendering
  let currentDate = new Date();
  
  const renderCalendar = () => {
    const year = currentDate.getFullYear();
    const month = currentDate.getMonth();
    
    // Set header
    currentMonthEl.textContent = `${currentDate.toLocaleString('default', { month: 'long' })} ${year}`;
    
    // Clear previous days
    calendarGrid.querySelectorAll('.day').forEach(day => day.remove());
    
    // Get first day of month and total days
    const firstDay = new Date(year, month, 1).getDay();
    const daysInMonth = new Date(year, month + 1, 0).getDate();
    
    // Add empty cells for days before first of month
    for (let i = 0; i < firstDay; i++) {
      const dayEl = document.createElement('div');
      dayEl.classList.add('day', 'empty');
      calendarGrid.appendChild(dayEl);
    }
    
    // Add days of month
    for (let day = 1; day <= daysInMonth; day++) {
      const dayEl = document.createElement('div');
      dayEl.classList.add('day');
      dayEl.textContent = day;
      
      dayEl.addEventListener('click', () => {
        const date = new Date(year, month, day);
        dateInput.value = date.toISOString().split('T')[0]; // YYYY-MM-DD
        calendarPopup.style.display = 'none';
        dateInput.style.borderColor = '';
      });
      
      calendarGrid.appendChild(dayEl);
    }
  };
  
  prevMonthBtn.addEventListener('click', () => {
    currentDate.setMonth(currentDate.getMonth() - 1);
    renderCalendar();
  });
  
  nextMonthBtn.addEventListener('click', () => {
    currentDate.setMonth(currentDate.getMonth() + 1);
    renderCalendar();
  });
  
  calendarBtn.addEventListener('click', () => {
    if (calendarPopup.style.display === 'none') {
      calendarPopup.style.display = 'block';
      renderCalendar();
      
      // Position calendar popup
      computePosition(dateInput, calendarPopup, {
        placement: 'bottom-start',
        middleware: [offset(4), flip(), shift()]
      }).then(({x, y}) => {
        Object.assign(calendarPopup.style, {
          position: 'absolute',
          left: `${x}px`,
          top: `${y}px`
        });
      });
    } else {
      calendarPopup.style.display = 'none';
    }
  });
  
  // Hide calendar when clicking outside
  document.addEventListener('click', (e) => {
    if (e.target !== calendarBtn && e.target !== dateInput && 
        !calendarPopup.contains(e.target)) {
      calendarPopup.style.display = 'none';
    }
  });
  
  container.appendChild(dateEditCard);
}

function initDialogWidgets() {
  const container = document.querySelector('#dialog .widget-grid');
  
  // WDialog
  const dialogCard = createWidgetCard(
    'WDialog',
    `<button id="showDialogBtn" class="wt-button">Show Dialog</button>
     <div id="demoDialog" class="dialog" style="display: none;">
       <div class="dialog-title">Demo Dialog</div>
       <div class="dialog-content">
         <p>This is a draggable dialog window.</p>
         <p>Try moving it by dragging the title bar.</p>
         <button class="dialog-close">Close</button>
       </div>
     </div>`,
    'Modal dialog with drag functionality'
  );
  
  let dialog = null;
  const dialogBtn = dialogCard.querySelector('#showDialogBtn');
  const dialogEl = dialogCard.querySelector('#demoDialog');
  const closeBtn = dialogCard.querySelector('.dialog-close');
  
  dialogBtn.addEventListener('click', () => {
    if (!dialog) {
      dialog = new WDialog({
        el: dialogEl,
        titlebar: '.dialog-title',
        movable: true,
        centerX: true,
        centerY: true
      });
    }
    dialogEl.style.display = 'block';
    dialog.bringToFront();
  });
  
  closeBtn.addEventListener('click', () => {
    dialogEl.style.display = 'none';
  });
  
  container.appendChild(dialogCard);
  
  // Message Dialog
  const messageDialogCard = createWidgetCard(
    'Message Dialog',
    `<div>
      <button id="showInfoBtn" class="wt-button">Info</button>
      <button id="showWarningBtn" class="wt-button">Warning</button>
      <button id="showErrorBtn" class="wt-button">Error</button>
     </div>`,
    'Predefined message dialogs'
  );
  
  const showMessageDialog = (type, message) => {
    const dialogId = `messageDialog-${Date.now()}`;
    const dialog = document.createElement('div');
    dialog.id = dialogId;
    dialog.className = `message-dialog ${type}`;
    dialog.innerHTML = `
      <div class="dialog-title">${type.charAt(0).toUpperCase() + type.slice(1)}</div>
      <div class="dialog-content">
        <div class="message-icon ${type}"></div>
        <p>${message}</p>
        <button class="dialog-close">OK</button>
      </div>
    `;
    
    document.body.appendChild(dialog);
    
    const dlg = new WDialog({
      el: dialog,
      titlebar: '.dialog-title',
      movable: true,
      centerX: true,
      centerY: true
    });
    
    dialog.querySelector('.dialog-close').addEventListener('click', () => {
      document.body.removeChild(dialog);
    });
  };
  
  messageDialogCard.querySelector('#showInfoBtn').addEventListener('click', () => {
    showMessageDialog('info', 'This is an information message.');
  });
  
  messageDialogCard.querySelector('#showWarningBtn').addEventListener('click', () => {
    showMessageDialog('warning', 'This is a warning message.');
  });
  
  messageDialogCard.querySelector('#showErrorBtn').addEventListener('click', () => {
    showMessageDialog('error', 'This is an error message.');
  });
  
  container.appendChild(messageDialogCard);
}

function createWidgetCard(title, demoHTML, description) {
  const card = document.createElement('div');
  card.className = 'widget-card';
  
  card.innerHTML = `
    <h3>${title}</h3>
    <div class="widget-demo">${demoHTML}</div>
    <div class="widget-description">${description}</div>
  `;
  
  return card;
}
// Create a tree example with WTree
export function initSimpleTreeExample() {
  const container = document.querySelector('#stree .widget-grid');
  
  // Create a simple tree card
  const treeCard = document.createElement('div');
  treeCard.className = 'widget-card full-width-card';
  treeCard.innerHTML = `
    <h3>Simple WTree Example</h3>
    <div class="tree-controls">
      <button id="tree-expand-all">Expand All</button>
      <button id="tree-collapse-all">Collapse All</button>
      <button id="tree-add-node">Add Node</button>
    </div>
    <div class="simple-tree-container" style="height: 300px; overflow: auto; border: 1px solid #ccc; padding: 10px;"></div>
    <div class="tree-selection-info" style="margin-top: 10px;">Selected: None</div>
    <div class="widget-description">
      Lightweight tree with custom icons and event handling
    </div>
  `;
  
  container.appendChild(treeCard);
  
  // Import WTree
  import("./js/WTree.esm.js").then(({ WTreeNode, WTree }) => {
    // Create a tree structure
    const rootNode = new WTreeNode("Root Folder", { 
      expanded: true,
      checkboxes: true,
      icon: '<i class="fas fa-folder-open" style="color: #f8d775;"></i>'
    });
    
    // Add first level children
    const docsNode = new WTreeNode("Documents", { 
      expanded: true,
      icon: '<i class="fas fa-folder-open" style="color: #f8d775;"></i>'
    });
    rootNode.addChild(docsNode);
    
    const picturesNode = new WTreeNode("Pictures", {
      icon: '<i class="fas fa-folder" style="color: #f8d775;"></i>'
    });
    rootNode.addChild(picturesNode);
    
    const musicNode = new WTreeNode("Music", {
      icon: '<i class="fas fa-folder" style="color: #f8d775;"></i>'
    });
    rootNode.addChild(musicNode);
    
    // Add some documents
    docsNode.addChild(new WTreeNode("Resume.pdf", { 
      icon: '<i class="fas fa-file-pdf" style="color: #e74c3c;"></i>' 
    }));
    docsNode.addChild(new WTreeNode("Budget.xlsx", { 
      icon: '<i class="fas fa-file-excel" style="color: #27ae60;"></i>' 
    }));
    
    // Add some pictures
    picturesNode.addChild(new WTreeNode("Vacation.jpg", { 
      icon: '<i class="fas fa-file-image" style="color: #3498db;"></i>' 
    }));
    picturesNode.addChild(new WTreeNode("Family.png", { 
      icon: '<i class="fas fa-file-image" style="color: #3498db;"></i>' 
    }));
    
    // Add some music
    musicNode.addChild(new WTreeNode("Favorite Song.mp3", { 
      icon: '<i class="fas fa-file-audio" style="color: #9b59b6;"></i>' 
    }));
    
    // Create a tree view
    const tree = new WTree(rootNode, '.simple-tree-container', {
      show_root: true
    });
    
    // Add event listeners to nodes
    rootNode.on('toggle_expanded', () => {
      // Update icon based on expanded state
      rootNode.setOption('icon', rootNode.expanded ? 
        '<i class="fas fa-folder-open" style="color: #f8d775;"></i>' : 
        '<i class="fas fa-folder" style="color: #f8d775;"></i>');
      tree.reload();
    });
    
    docsNode.on('toggle_expanded', () => {
      docsNode.setOption('icon', docsNode.expanded ? 
        '<i class="fas fa-folder-open" style="color: #f8d775;"></i>' : 
        '<i class="fas fa-folder" style="color: #f8d775;"></i>');
      tree.reload();
    });
    
    picturesNode.on('toggle_expanded', () => {
      picturesNode.setOption('icon', picturesNode.expanded ? 
        '<i class="fas fa-folder-open" style="color: #f8d775;"></i>' : 
        '<i class="fas fa-folder" style="color: #f8d775;"></i>');
      tree.reload();
    });
    
    musicNode.on('toggle_expanded', () => {
      musicNode.setOption('icon', musicNode.expanded ? 
        '<i class="fas fa-folder-open" style="color: #f8d775;"></i>' : 
        '<i class="fas fa-folder" style="color: #f8d775;"></i>');
      tree.reload();
    });
    
    // Track selections
    const selectionInfo = document.querySelector('.tree-selection-info');
    const updateSelectionInfo = () => {
      const selected = tree.selectedNodes;
      if (selected.length === 0) {
        selectionInfo.textContent = 'Selected: None';
      } else {
        selectionInfo.textContent = `Selected: ${selected.map(n => n.toString()).join(', ')}`;
      }
    };
    
    // Apply selection tracking to all nodes
    const addSelectionTracking = (node) => {
      node.on('toggle_selected', updateSelectionInfo);
      node.children.forEach(addSelectionTracking);
    };
    addSelectionTracking(rootNode);
    
    // Add leaf node click handler for "open" action
    const addOpenHandler = (node) => {
      node.on('open', () => {
        alert(`Opening: ${node.toString()}`);
      });
      node.children.forEach(addOpenHandler);
    };
    addOpenHandler(rootNode);
    
    // Add context menu handler
    const addContextHandler = (node) => {
      node.on('contextmenu', (e) => {
        console.log(`Context menu for: ${node.toString()}`);
        // You could show a custom context menu here
      });
      node.children.forEach(addContextHandler);
    };
    addContextHandler(rootNode);
    
    // Add button handlers
    document.getElementById('tree-expand-all').addEventListener('click', () => {
      tree.expandAll();
    });
    
    document.getElementById('tree-collapse-all').addEventListener('click', () => {
      tree.collapseAll();
    });
    
    document.getElementById('tree-add-node').addEventListener('click', () => {
      // Generate a random node name
      const nodeTypes = ['Document', 'Image', 'Audio', 'Video', 'Archive'];
      const nodeType = nodeTypes[Math.floor(Math.random() * nodeTypes.length)];
      const nodeName = `New ${nodeType} ${Math.floor(Math.random() * 100)}`;
      
      // Add icon based on node type
      let icon = '<i class="fas fa-file"></i>';
      switch (nodeType) {
        case 'Document': 
          icon = '<i class="fas fa-file-alt" style="color: #3498db;"></i>';
          break;
        case 'Image': 
          icon = '<i class="fas fa-file-image" style="color: #2ecc71;"></i>';
          break;
        case 'Audio': 
          icon = '<i class="fas fa-file-audio" style="color: #9b59b6;"></i>';
          break;
        case 'Video': 
          icon = '<i class="fas fa-file-video" style="color: #e74c3c;"></i>';
          break;
        case 'Archive': 
          icon = '<i class="fas fa-file-archive" style="color: #f39c12;"></i>';
          break;
      }
      
      // Create and add the new node
      const newNode = new WTreeNode(nodeName, { icon });
      docsNode.addChild(newNode);
      
      // Make sure docs node is expanded
      docsNode.expanded = true;
      
      // Refresh the tree
      tree.reload();
    });
    
    // Add basic CSS for the tree
    const style = document.createElement('style');
    style.textContent = `
      .tj_container ul {
        list-style-type: none;
        padding-left: 20px;
      }
      
      .tj_container > ul {
        padding-left: 0;
      }
      
      .tj_description {
        cursor: pointer;
        padding: 3px;
        display: inline-block;
        border-radius: 3px;
      }
      
      .tj_description:hover {
        background-color: #f0f0f0;
      }
      
      .tj_description.selected {
        background-color: #e0e0ff;
      }
      
      .tj_mod_icon, .tj_icon {
        margin-right: 5px;
      }
      
      [aria-disabled="true"] .tj_description {
        opacity: 0.5;
        cursor: not-allowed;
      }
    `;
    document.head.appendChild(style);
  });
  
  // Add Font Awesome for icons if not already loaded
  if (!document.querySelector('link[href*="font-awesome"]')) {
    const linkElement = document.createElement('link');
    linkElement.rel = 'stylesheet';
    linkElement.href = 'https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.4/css/all.min.css';
    document.head.appendChild(linkElement);
  }
}
export function initGridWidgets() {
  /*** ---------- build the DOM card ---------- ***/
  const container = document.querySelector('#grid .widget-grid');

  const card = document.createElement('div');
  card.className = 'widget-card full-width-card';
  card.innerHTML = `
    <h3>SlickGrid (ES-module, no jQuery)</h3>

    <!-- filter bar -->
    <div class="filter-bar">
      <input id="filterTitle" placeholder="Filter by title…" />
      <select id="filterPriority">
        <option value="">All priorities</option>
        <option value="1">High</option><option value="2">Medium</option><option value="3">Low</option>
      </select>
    </div>

    <!-- grid -->
    <div class="grid-container" id="slickGridDemo"></div>

    <div class="widget-description">
      Sorting • column filters • inline editing • responsive resize • 100 demo rows
    </div>
  `;
  container.appendChild(card);

  /*** ---------- grid definitions ---------- ***/
  const columns = [
    { id: 'sel', name: '', field: 'sel', width: 28,
      formatter: () => '<input type="checkbox" class="row-select">',
      resizable: false }
      , { id: 'id',     name: 'ID',       field: 'id',     width: 50,  sortable: true }
      , { id: 'title',  name: 'Task',     field: 'title',  width: 180, sortable: true,
          editor: Editors.Text }
      , { id: 'priority', name: 'Priority', field: 'priorityLabel', width: 80,
          sortable: true, formatter: Formatters.Text }                                   // built-in text
      , { id: 'percent', name: 'Progress', field: 'percent', width: 110, sortable: true,
          formatter: Formatters.PercentCompleteBar }                                     // native bar
      , { id: 'start',  name: 'Start',    field: 'start',  width: 110, sortable: true,
          formatter: Formatters.DateIso }                                                // native ISO date
      , { id: 'finish', name: 'Finish',   field: 'finish', width: 110, sortable: true,
          formatter: Formatters.DateIso }
      ];

  const options = {
    // columnPicker: {
    //   columnTitle: "Columns",
    //   hideForceFitButton: false,
    //   hideSyncResizeButton: false,
    //   forceFitTitle: "Force fit columns",
    //   syncResizeTitle: "Synchronous resize",
    // },
    // gridMenu: {
    //   iconCssClass: "sgi sgi-menu sgi-17px",
    //   columnTitle: "Columns",
    //   hideForceFitButton: false,
    //   hideSyncResizeButton: false,
    //   forceFitTitle: "Force fit columns",
    //   syncResizeTitle: "Synchronous resize",
    // },
    editable: true,
    syncColumnCellResize: true,
    forceFitColumns: true,        // <— stretch/shrink to fill
    enableAddRow: true,
    enableCellNavigation: true,
    asyncEditorLoading: true,
    topPanelHeight: 35,
    rowHeight: 28
  };

  /*** ---------- sample data ---------- ***/
  const data = Array.from({ length: 100 }, (_, i) => {
    const p = Math.floor(Math.random() * 3) + 1;
    const pc = Math.round(Math.random() * 100);
    const start = Date.now() - Math.random() * 3.15e10;         // ~year back
    return {
      id: i,
      title: `Task ${i}`,
      priority: p,
      percent: pc,
      start: start,
      finish: start + Math.random() * 1.5e10,
      sel: false
    };
  });


    // Create a mock data URL function that returns our sample data
  const mockDataUrl = ({ page, from, to }) => {
    // Return a Promise to simulate async data loading
    return new Promise(resolve => {
      setTimeout(() => {
        resolve({
          items: data.slice(from, to),
          done: to >= data.length,
          total: data.length
        });
      }, 100); // Simulate network delay
    });
  };

  /*** ---------- Initialize WTableView ---------- ***/
  const tableView = new WTableView({
    el: '#slickGridDemo',
    columns: columns,
    dataUrl: mockDataUrl,
    pageSize: 25,
    options: {
      editable: true,
      syncColumnCellResize: true,
      forceFitColumns: false,
      enableAddRow: true,
      enableCellNavigation: true,
      asyncEditorLoading: true,
      rowHeight: 28
    },
    showHeaderRow: false,
    // Uncomment to enable advanced features
    // columnGroups: [
    //   { name: 'Task Details', columns: ['id', 'title', 'priority'] },
    //   { name: 'Progress', columns: ['percent', 'start', 'finish'] }
    // ],
    // grouping: {
    //   getter: 'priorityLabel',
    //   formatter: g => `${g.value} Priority (${g.count} items)`
    // },
    validators: {
      title: value => value.length >= 3 || 'Title must be at least 3 characters'
    }
  });

  // /*** ---------- DataView + Grid ---------- ***/
  // const dataView = new SlickDataView({ inlineFilters: true });
  // dataView.setItems(data);

  // const gridElem = document.getElementById('slickGridDemo');
  // const grid = new SlickGrid(gridElem, dataView, columns, options);

  // const ro = new ResizeObserver(() => grid.resizeCanvas());
  // ro.observe(gridElem);

  // /*** ---------- sorting ---------- ***/
  // grid.onSort.subscribe((_e, { sortCol, sortAsc }) => {
  //   console.log(`Sorting by ${sortCol.field} (${sortAsc ? 'asc' : 'desc'})`);
  //   // Update the DataView with the new sort order
  //   dataView.sort((a, b) => {
  //       const x = a[sortCol.field];
  //       const y = b[sortCol.field];
        
  //       // Handle different data types appropriately
  //       if (typeof x === 'string' && typeof y === 'string') {
  //         return sortAsc ? x.localeCompare(y) : y.localeCompare(x);
  //       } else {
  //         return sortAsc ? (x === y ? 0 : (x > y ? 1 : -1)) : (x === y ? 0 : (x > y ? -1 : 1));
  //       }
  //   });
  //   // Force grid to refresh after sorting
  //   grid.invalidate();
  //   grid.render();
  // });


  /*** ---------- filtering ---------- ***/
  const filterTitle = document.getElementById('filterTitle');
  const filterPriority = document.getElementById('filterPriority');

 // Setup column filters
  const applyFilters = () => {
    const filters = {};
    
    if (filterTitle.value) {
      filters.title = filterTitle.value;
    }
    
    if (filterPriority.value) {
      filters.priority = filterPriority.value;
    }
    
    tableView.applyColumnFilters(filters);
  };

  filterTitle.addEventListener('input', applyFilters);
  filterPriority.addEventListener('change', applyFilters);

  // Set initial filters
  applyFilters();

  function filterFn(item) {
    const matchTitle = item.title.toLowerCase().includes(filterTitle.value.trim().toLowerCase());
    const matchPrio  = filterPriority.value === '' || String(item.priority) === filterPriority.value;
    return matchTitle && matchPrio;
  }

  // filterTitle.addEventListener('input', () => { dataView.refresh(); });
  // filterPriority.addEventListener('change', () => { dataView.refresh(); });
  // dataView.setFilter(filterFn);

  // /*** ---------- row-selection (checkbox) ---------- ***/
  // grid.onClick.subscribe((_e, args) => {
  //   if (args.cell === 0) {
  //     const item = dataView.getItem(args.row);
  //     item.sel = !item.sel;
  //     dataView.updateItem(item.id, item);
  //   }
  // });

  // /*** ---------- resize handling ---------- ***/
  // function resize() { grid.resizeCanvas(); }
  // window.addEventListener('resize', resize);
  // resize();

  // /*** ---------- initial render ---------- ***/
  // dataView.refresh();  // applies initial filter / sort

  setTimeout(() => {
    tableView.refresh();
    // grid.resizeCanvas();
    // dataView.refresh();
    // grid.invalidate();
    // grid.render();    
  }, 100);

}


export function initTreeViewExample() {
  const container = document.querySelector('#tree .widget-grid');
  
  // Create a tree view card
  const treeViewCard = document.createElement('div');
  treeViewCard.className = 'widget-card full-width-card';
  treeViewCard.innerHTML = `
    <h3>WTreeView Example</h3>
    <div class="tree-controls">
      <button id="expandAll">Expand All</button>
      <button id="collapseAll">Collapse All</button>
      <button id="addNode">Add Node</button>
    </div>
    <div class="grid-container" id="treeViewDemo" style="height: 400px;"></div>
    <div class="widget-description">
      Hierarchical data with expand/collapse functionality, checkboxes, and custom icons
    </div>
  `;
  
  container.appendChild(treeViewCard);
  
  // Sample hierarchical data
  const treeData = [
    { id: 1, name: "Documents", size: "-", type: "folder", expanded: true },
    { id: 2, name: "Projects", parentId: 1, size: "-", type: "folder" },
    { id: 3, name: "Reports", parentId: 1, size: "-", type: "folder", expanded: true },
    { id: 4, name: "Personal", parentId: 1, size: "-", type: "folder" },
    { id: 5, name: "Project A", parentId: 2, size: "-", type: "folder" },
    { id: 6, name: "Project B", parentId: 2, size: "-", type: "folder" },
    { id: 7, name: "Q1 Report.pdf", parentId: 3, size: "2.5 MB", type: "pdf" },
    { id: 8, name: "Q2 Report.pdf", parentId: 3, size: "3.1 MB", type: "pdf" },
    { id: 9, name: "Budget.xlsx", parentId: 3, size: "1.8 MB", type: "excel" },
    { id: 10, name: "Notes.txt", parentId: 4, size: "12 KB", type: "text" },
    { id: 11, name: "Photos", parentId: 4, size: "-", type: "folder" },
    { id: 12, name: "Vacation.jpg", parentId: 11, size: "4.2 MB", type: "image" },
    { id: 13, name: "Family.jpg", parentId: 11, size: "3.8 MB", type: "image" },
    { id: 14, name: "Specifications.docx", parentId: 5, size: "1.2 MB", type: "word" },
    { id: 15, name: "Presentation.pptx", parentId: 5, size: "6.7 MB", type: "powerpoint" }
  ];

  // Add file type icons
  const getFileIcon = (type) => {
    switch(type) {
      case "folder": return `<i class="far fa-folder" style="color: #f8d775;"></i>`;
      case "pdf": return `<i class="far fa-file-pdf" style="color: #e74c3c;"></i>`;
      case "excel": return `<i class="far fa-file-excel" style="color: #27ae60;"></i>`;
      case "word": return `<i class="far fa-file-word" style="color: #3498db;"></i>`;
      case "powerpoint": return `<i class="far fa-file-powerpoint" style="color: #e67e22;"></i>`;
      case "text": return `<i class="far fa-file-alt" style="color: #95a5a6;"></i>`;
      case "image": return `<i class="far fa-file-image" style="color: #9b59b6;"></i>`;
      default: return `<i class="far fa-file"></i>`;
    }
  };

  // Add icons to the data
  treeData.forEach(item => {
    item.icon = getFileIcon(item.type);
  });

  // Define columns for the tree
  const columns = [
    { id: "name", name: "Name", field: "name", width: 280 },
    { id: "size", name: "Size", field: "size", width: 80 },
    { id: "type", name: "Type", field: "type", width: 100 }
  ];

  // Create WTreeView instance
  import("./js/WTreeView.esm.js").then(({ default: WTreeView }) => {
    const treeView = new WTreeView({
      el: '#treeViewDemo',
      columns: columns,
      checkboxes: true, // Enable selection checkboxes
      idField: 'id',
      parentIdField: 'parentId',
      expandedField: 'expanded',
      iconField: 'icon',
      indentation: 16,
      dragDrop: true
    });

    // Set data
    treeView.setData(treeData);

    // Hook up control buttons
    document.getElementById('expandAll').addEventListener('click', () => {
      treeView.expandAll();
    });

    document.getElementById('collapseAll').addEventListener('click', () => {
      treeView.collapseAll();
    });

    // Add a new node when the button is clicked
    let newNodeId = 16; // Start IDs after the existing ones
    document.getElementById('addNode').addEventListener('click', () => {
      const newNode = {
        id: newNodeId++,
        name: `New Item ${newNodeId-16}`,
        parentId: 4, // Add to Personal folder
        size: "1 KB",
        type: "text",
        icon: getFileIcon("text")
      };
      
      // Get current data, add new item, and refresh
      const currentData = treeView.getDataView().getItems();
      const newData = [...currentData, newNode];
      treeView.setData(newData);
      
      // Make sure the parent is expanded to see the new node
      treeView.revealNode(newNode.id);
    });

    // Listen for selection changes
    document.querySelector('#treeViewDemo').addEventListener('selectionchanged', e => {
      console.log('Selected items:', e.detail.selected);
    });
  });
  
  // Add Font Awesome for icons
  const linkElement = document.createElement('link');
  linkElement.rel = 'stylesheet';
  linkElement.href = 'https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.4/css/all.min.css';
  document.head.appendChild(linkElement);
}


export function initChartWidgets() {
  const container = document.querySelector('#chart .widget-grid');
  
  // Line Chart
  const lineChartCard = createWidgetCard(
    'Line Chart',
    `<div class="chart-container">
      <canvas id="lineChart"></canvas>
    </div>`,
    'Time series data visualization with interactive tooltips'
  );
  container.appendChild(lineChartCard);
  
  // Bar Chart
  const barChartCard = createWidgetCard(
    'Bar Chart',
    `<div class="chart-container">
      <canvas id="barChart"></canvas>
    </div>`,
    'Compare values across categories with customizable colors'
  );
  container.appendChild(barChartCard);
  
  // Pie Chart
  const pieChartCard = createWidgetCard(
    'Pie Chart',
    `<div class="chart-container">
      <canvas id="pieChart"></canvas>
    </div>`,
    'Proportional visualization of data categories'
  );
  container.appendChild(pieChartCard);
  
  // Radar Chart
  const radarChartCard = createWidgetCard(
    'Radar Chart',
    `<div class="chart-container">
      <canvas id="radarChart"></canvas>
    </div>`,
    'Multi-variable data visualization on a two-dimensional chart'
  );
  container.appendChild(radarChartCard);
  
  // Initialize charts after DOM has fully updated
  setTimeout(() => {
    // Line chart data and options
    const lineData = {
      labels: ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun'],
      datasets: [{
        label: 'Sales 2025',
        data: [65, 59, 80, 81, 56, 55],
        borderColor: '#3498db',
        backgroundColor: 'rgba(52, 152, 219, 0.1)',
        tension: 0.4,
        fill: true
      }, {
        label: 'Sales 2024',
        data: [28, 48, 40, 19, 86, 27],
        borderColor: '#2ecc71',
        backgroundColor: 'rgba(46, 204, 113, 0.1)',
        tension: 0.4,
        fill: true
      }]
    };
    
    const lineOptions = {
      responsive: true,
      maintainAspectRatio: false,
      interaction: {
        mode: 'index',
        intersect: false
      },
      plugins: {
        title: {
          display: true,
          text: 'Monthly Sales Comparison'
        },
        tooltip: {
          usePointStyle: true
        }
      },
      scales: {
        y: {
          beginAtZero: true,
          title: {
            display: true,
            text: 'Revenue ($1000)'
          }
        }
      }
    };
    
    // Bar chart data and options
    const barData = {
      labels: ['Q1', 'Q2', 'Q3', 'Q4'],
      datasets: [{
        label: 'Revenue',
        data: [120, 190, 170, 220],
        backgroundColor: 'rgba(52, 152, 219, 0.6)'
      }, {
        label: 'Expenses',
        data: [80, 110, 90, 130],
        backgroundColor: 'rgba(231, 76, 60, 0.6)'
      }]
    };
    
    const barOptions = {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        title: {
          display: true,
          text: 'Quarterly Financial Performance'
        },
        legend: {
          position: 'top'
        }
      },
      scales: {
        y: {
          beginAtZero: true,
          title: {
            display: true,
            text: 'Amount ($1000)'
          }
        }
      }
    };
    
    // Pie chart data and options
    const pieData = {
      labels: ['Product A', 'Product B', 'Product C', 'Product D', 'Product E'],
      datasets: [{
        data: [25, 20, 30, 15, 10],
        backgroundColor: [
          'rgba(52, 152, 219, 0.7)',  // Blue
          'rgba(46, 204, 113, 0.7)',  // Green
          'rgba(155, 89, 182, 0.7)',  // Purple
          'rgba(230, 126, 34, 0.7)',  // Orange
          'rgba(241, 196, 15, 0.7)'   // Yellow
        ],
        borderColor: 'white',
        borderWidth: 1
      }]
    };
    
    const pieOptions = {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        title: {
          display: true,
          text: 'Product Sales Distribution'
        },
        legend: {
          position: 'right'
        }
      }
    };
    
    // Radar chart data and options
    const radarData = {
      labels: ['Speed', 'Reliability', 'Comfort', 'Safety', 'Efficiency', 'Design'],
      datasets: [{
        label: 'Model X',
        data: [90, 85, 70, 95, 80, 75],
        backgroundColor: 'rgba(52, 152, 219, 0.2)',
        borderColor: 'rgba(52, 152, 219, 0.8)',
        pointBackgroundColor: 'rgba(52, 152, 219, 1)',
      }, {
        label: 'Model Y',
        data: [75, 90, 85, 80, 95, 90],
        backgroundColor: 'rgba(46, 204, 113, 0.2)',
        borderColor: 'rgba(46, 204, 113, 0.8)',
        pointBackgroundColor: 'rgba(46, 204, 113, 1)',
      }]
    };
    
    const radarOptions = {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        title: {
          display: true,
          text: 'Vehicle Performance Metrics'
        }
      },
      scales: {
        r: {
          min: 0,
          max: 100,
          ticks: {
            stepSize: 20
          }
        }
      }
    };
    
    // Create chart instances
    new Chart(document.getElementById('lineChart'), {
      type: 'line',
      data: lineData,
      options: lineOptions
    });
    
    new Chart(document.getElementById('barChart'), {
      type: 'bar',
      data: barData,
      options: barOptions
    });
    
    new Chart(document.getElementById('pieChart'), {
      type: 'pie',
      data: pieData,
      options: pieOptions
    });
    
    new Chart(document.getElementById('radarChart'), {
      type: 'radar',
      data: radarData,
      options: radarOptions
    });
  }, 100); // Small delay to ensure DOM is ready
}

import StdLayout2 from './js/StdGridLayoutImpl2.esm.js';

export function initGridLayoutTest() {
  const container = document.createElement('div');
  container.id = 'gridLayoutTestContainer';
  container.classList.add('Wt-domRoot');
  
  // Add the CSS styles
  const style = document.createElement('style');
  style.textContent = `
    #gridLayoutTestContainer {
      font-family: system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      max-width: 1200px;
      margin: 0 auto;
      padding: 20px;
      color: #333;
    }
    
    .test-section {
      margin-bottom: 40px;
      border: 1px solid #ddd;
      border-radius: 8px;
      padding: 20px;
    }
    
    .test-section h2 {
      margin-top: 0;
      border-bottom: 2px solid #f0f0f0;
      padding-bottom: 10px;
      margin-bottom: 20px;
      color: #2c3e50;
    }
    
    .test-section .description {
      margin-bottom: 20px;
      font-size: 0.9em;
      color: #666;
    }
    
    .test-layout-root {
      position: relative;
      border: 2px dashed #ccc;
      background: #f9f9f9;
      margin: 20px 0;
      min-height: 300px;
    }
    
    .grid-item {
      background-color: #ecf0f1;
      border: 1px solid #bdc3c7;
      padding: 10px;
      border-radius: 4px;
      display: flex;
      flex-direction: column;
      justify-content: center;
      align-items: center;
      text-align: center;
      box-shadow: 0 2px 5px rgba(0,0,0,0.05);
      overflow: auto;
    }
    
    .grid-item.primary {
      background-color: #3498db;
      color: white;
    }
    
    .grid-item.secondary {
      background-color: #2ecc71;
      color: white;
    }
    
    .grid-item.tertiary {
      background-color: #e74c3c;
      color: white;
    }
    
    .grid-item.highlight {
      background-color: #f39c12;
      color: white;
    }
    
    .control-panel {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      margin-bottom: 15px;
    }
    
    .control-panel button {
      padding: 8px 12px;
      background: #3498db;
      color: white;
      border: none;
      border-radius: 4px;
      cursor: pointer;
    }
    
    .control-panel button:hover {
      background: #2980b9;
    }
    
    .metrics {
      font-family: monospace;
      font-size: 0.9em;
      margin-top: 10px;
      display: block;
    }
    
    .resize-handle {
      position: absolute;
      bottom: -10px;
      right: -10px;
      width: 20px;
      height: 20px;
      background: #3498db;
      border-radius: 50%;
      cursor: nwse-resize;
    }
          /* Styles for StdLayout2 resize handles */
    .layout-col-handle {
      position: absolute; /* Managed by StdLayout2's #positionHandle */
      width: 7px; /* Make it a bit wider for easier grabbing */
      top: 0;
      bottom: 0;
      background-color: rgba(0, 123, 255, 0.3); /* Default color */
      cursor: col-resize;
      z-index: 100;
      transition: background-color 0.2s ease;
    }
    .layout-col-handle:hover {
      background-color: rgba(0, 123, 255, 0.6); /* Darker on hover */
    }

    .layout-row-handle {
      position: absolute; /* Managed by StdLayout2's #positionHandle */
      height: 7px; /* Make it a bit wider for easier grabbing */
      left: 0;
      right: 0;
      background-color: rgba(0, 123, 255, 0.3); /* Default color */
      cursor: row-resize;
      z-index: 100;
      transition: background-color 0.2s ease;
    }
    .layout-row-handle:hover {
      background-color: rgba(0, 123, 255, 0.6); /* Darker on hover */
    }

    /* Style for the resizable container in test 6 */
    #handleLayoutRoot {
      resize: both; /* Allows user to resize the whole container */
      overflow: auto; /* Important for the 'resize' property to work well */
      min-width: 200px; /* Prevent it from becoming too small */
      min-height: 200px;
    }
  `;
  document.head.appendChild(style);
  
  // Create the test sections
  container.innerHTML = `
    <h1>StdGridLayout2 Test Suite</h1>
    <p>This test suite demonstrates the capabilities of the StdGridLayout2 class for creating flexible grid layouts.</p>
    
    <div id="basicTest" class="test-section">
      <h2>1. Basic Grid Layout</h2>
      <div class="description">
        A simple 2x2 grid demonstrating the basic layout capabilities with equal sizing.
      </div>
      <div class="control-panel">
        <button id="basic-measure">Refresh</button>
      </div>
      <div id="basicLayoutRoot" class="test-layout-root" style="height: 300px;">
        <div id="basic-cell-1" class="grid-item">Cell 1</div>
        <div id="basic-cell-2" class="grid-item primary">Cell 2</div>
        <div id="basic-cell-3" class="grid-item secondary">Cell 3</div>
        <div id="basic-cell-4" class="grid-item tertiary">Cell 4</div>
      </div>
    </div>
    
    <div id="stretchTest" class="test-section">
      <h2>2. Stretch Factors</h2>
      <div class="description">
        Demonstrates how stretch factors control proportional sizing of rows and columns.
      </div>
      <div class="control-panel">
        <button id="stretch-measure">Refresh</button>
      </div>
      <div id="stretchLayoutRoot" class="test-layout-root" style="height: 400px;">
        <div id="stretch-cell-1" class="grid-item">Stretch 1</div>
        <div id="stretch-cell-2" class="grid-item primary">Stretch 2</div>
        <div id="stretch-cell-3" class="grid-item secondary">Stretch 1</div>
        <div id="stretch-cell-4" class="grid-item tertiary">Fixed Size</div>
        <div id="stretch-cell-5" class="grid-item highlight">Stretch 3</div>
      </div>
    </div>
    
    <div id="dynamicTest" class="test-section">
      <h2>3. Dynamic Content</h2>
      <div class="description">
        Tests how the layout responds to dynamic content changes.
      </div>
      <div class="control-panel">
        <button id="dynamic-add">Add Content</button>
        <button id="dynamic-remove">Remove Content</button>
        <button id="dynamic-resize">Resize Content</button>
      </div>
      <div id="dynamicLayoutRoot" class="test-layout-root" style="height: 300px;">
        <div id="dynamic-cell-1" class="grid-item">Dynamic Content</div>
        <div id="dynamic-cell-2" class="grid-item primary">Resize Me</div>
      </div>
    </div>
    
    <div id="rtlTest" class="test-section">
      <h2>4. RTL Support</h2>
      <div class="description">
        Tests right-to-left layout direction support.
      </div>
      <div class="control-panel">
        <button id="rtl-toggle">Toggle RTL</button>
      </div>
      <div id="rtlLayoutRoot" class="test-layout-root" style="height: 300px;" dir="ltr">
        <div id="rtl-cell-1" class="grid-item">Left</div>
        <div id="rtl-cell-2" class="grid-item primary">Center</div>
        <div id="rtl-cell-3" class="grid-item secondary">Right</div>
      </div>
    </div>
    
    <div id="sizeConstraintTest" class="test-section">
      <h2>5. Size Constraints</h2>
      <div class="description">
        Demonstrates minimum and preferred size handling.
      </div>
      <div class="control-panel">
        <button id="constraint-toggle">Toggle Size Constraints</button>
      </div>
      <div id="constraintLayoutRoot" class="test-layout-root" style="height: 300px;">
        <div id="constraint-cell-1" class="grid-item" style="min-width: 100px; min-height: 50px;">
          Min size: 100×50px
        </div>
        <div id="constraint-cell-2" class="grid-item primary" style="width: 200px; height: 150px;">
          Preferred: 200×150px
        </div>
        <div id="constraint-cell-3" class="grid-item secondary">
          No constraints
        </div>
      </div>
    </div>
    
    <div id="resizableTest" class="test-section">
      <h2>6. Interactive Resizing</h2>
      <div class="description">
        Tests how the layout adapts when the container is resized by the user.
      </div>
      <div id="resizableLayoutRoot" class="test-layout-root" style="height: 300px; width: 100%; resize: both; overflow: auto;">
        <div id="resize-cell-1" class="grid-item">Top Left</div>
        <div id="resize-cell-2" class="grid-item primary">Top Right</div>
        <div id="resize-cell-3" class="grid-item secondary">Bottom Left</div>
        <div id="resize-cell-4" class="grid-item tertiary">Bottom Right</div>
      </div>
      <p>Resize the container using the bottom-right corner</p>
    </div>

    <div id="resizableTest" class="test-section">
      <h2>6. Interactive Resizing (Rows & Columns)</h2>
      <div class="description">
        Tests interactive resizing of grid rows and columns using drag handles.
        The handles should appear between rows and columns.
      </div>
      <div id="handleLayoutRoot" class="test-layout-root" style="height: 300px; width: 100%;">
        <div id="resize-cell-5" class="grid-item">Top Left</div>
        <div id="resize-cell-6" class="grid-item primary">Top Right</div>
        <div id="resize-cell-7" class="grid-item secondary">Bottom Left</div>
        <div id="resize-cell-8" class="grid-item tertiary">Bottom Right</div>
      </div>
      <p>Try dragging the faint blue lines between rows/columns.</p>
    </div>
    
    <div id="complexTest" class="test-section">
      <h2>7. Complex Layout Example</h2>
      <div class="description">
        A more complex layout demonstrating a realistic application interface.
      </div>
      <div id="complexLayoutRoot" class="test-layout-root" style="height: 500px;">
        <div id="complex-header" class="grid-item" style="min-height: 50px;">Header</div>
        <div id="complex-sidebar" class="grid-item primary" style="min-width: 150px;">Sidebar</div>
        <div id="complex-content" class="grid-item secondary">Main Content Area</div>
        <div id="complex-panel" class="grid-item tertiary" style="min-width: 180px;">Right Panel</div>
        <div id="complex-footer" class="grid-item" style="min-height: 40px;">Footer</div>
      </div>
    </div>
  `;
  
  // Append to the body
  document.body.appendChild(container);
  
  // Initialize the test layouts
  initBasicTest();
  initStretchTest();
  initDynamicTest();
  initRtlTest();
  initSizeConstraintTest();
  initResizableTest();
  initComplexTest();
  initResizableHandle();
}

function initBasicTest() {
  const layout = new StdLayout2({
    root: document.getElementById('basicLayoutRoot'),
    rows: [
      { stretch: 1 },
      { stretch: 1 }
    ],
    columns: [
      { stretch: 1 },
      { stretch: 1 }
    ],
    items: [
      { el: 'basic-cell-1', row: 0, col: 0 },
      { el: 'basic-cell-2', row: 0, col: 1 },
      { el: 'basic-cell-3', row: 1, col: 0 },
      { el: 'basic-cell-4', row: 1, col: 1 }
    ]
  });
  
  document.getElementById('basic-measure').addEventListener('click', () => {
    layout.refresh();
  });
  
  // Initial layout
  layout.refresh();
}

function initStretchTest() {
  const layout = new StdLayout2({
    root: document.getElementById('stretchLayoutRoot'),
    rows: [
      { stretch: 1 },
      { stretch: 2 },
      { stretch: 0, min: 80, preferred: 80, max: 80 } // Fixed height row
    ],
    columns: [
      { stretch: 1 },
      { stretch: 2 },
      { stretch: 3 }
    ],
    items: [
      { el: 'stretch-cell-1', row: 0, col: 0 }, // Stretch 1×1
      { el: 'stretch-cell-2', row: 0, col: 1 }, // Stretch 1×2
      { el: 'stretch-cell-3', row: 1, col: 0 }, // Stretch 2×1
      { el: 'stretch-cell-4', row: 2, col: 0 }, // Fixed height
      { el: 'stretch-cell-5', row: 1, col: 2 }  // Stretch 2×3
    ]
  });
  
  document.getElementById('stretch-measure').addEventListener('click', () => {
    layout.refresh();
  });
    
  // Initial layout
  layout.refresh();
}

function initDynamicTest() {
  const layout = new StdLayout2({
    root: document.getElementById('dynamicLayoutRoot'),
    rows: [
      { stretch: 1 },
      { stretch: 1 }
    ],
    columns: [
      { stretch: 1 },
      { stretch: 1 }
    ],
    items: [
      { el: 'dynamic-cell-1', row: 0, col: 0 },
      { el: 'dynamic-cell-2', row: 1, col: 1 }
    ]
  });
  
  let contentAdded = false;
  let expanded = false;
  
  document.getElementById('dynamic-add').addEventListener('click', () => {
    if (!contentAdded) {
      const newCell = document.createElement('div');
      newCell.id = 'dynamic-cell-3';
      newCell.className = 'grid-item tertiary';
      newCell.textContent = 'New Content';
      document.getElementById('dynamicLayoutRoot').appendChild(newCell);

            // Add to layout and refresh
      layout.addItem({
        el: 'dynamic-cell-3', 
        row: 0, 
        col: 1
      }).refresh();
      contentAdded = true;
    }
  });
  
  document.getElementById('dynamic-remove').addEventListener('click', () => {
    if (contentAdded) {
      const cell = document.getElementById('dynamic-cell-3');
      if (cell) {
        cell.remove();
        layout.refresh();
        contentAdded = false;
      }
    }
  });
  
  document.getElementById('dynamic-resize').addEventListener('click', () => {
    const cell = document.getElementById('dynamic-cell-2');
    if (expanded) {
      cell.style.minHeight = '';
      cell.style.minWidth = '';
      cell.textContent = 'Resize Me';
    } else {
      cell.style.minHeight = '800px';
      cell.style.minWidth = '250px';
      cell.textContent = 'I am bigger now!';
    }
    expanded = !expanded;
    layout.refresh();
  });
  
  // Initial layout
  layout.refresh();
}
function initRtlTest() {
  let isRtl = false;
  const rootEl = document.getElementById('rtlLayoutRoot');

  const layout = new StdLayout2({
    root: rootEl,
    rows:    [ { stretch: 1 } ],
    columns: [ { stretch: 1 }, { stretch: 1 }, { stretch: 1 } ],
    items:   [
      { el: 'rtl-cell-1', row: 0, col: 0 },
      { el: 'rtl-cell-2', row: 0, col: 1 },
      { el: 'rtl-cell-3', row: 0, col: 2 }
    ]
  });

  document.getElementById('rtl-toggle').addEventListener('click', () => {
    isRtl = !isRtl;
    rootEl.dir = isRtl ? 'rtl' : 'ltr';   // optional: update DOM attribute
    layout.setRTL(isRtl);                 // 🔄 single call does it all
  });
  // Initial layout
  layout.refresh();
}
function initSizeConstraintTest() {
  const layout = new StdLayout2({
    root: document.getElementById('constraintLayoutRoot'),
    rows: [
      { stretch: 1 }
    ],
    columns: [
      { stretch: 1 },
      { stretch: 1, preferred: 200 },
      { stretch: 1 }
    ],
    items: [
      { el: 'constraint-cell-1', row: 0, col: 0 },
      { el: 'constraint-cell-2', row: 0, col: 1 },
      { el: 'constraint-cell-3', row: 0, col: 2 }
    ]
  });
  
  let constraintsOn = true;
  
  document.getElementById('constraint-toggle').addEventListener('click', () => {
    const cell1 = document.getElementById('constraint-cell-1');
    const cell2 = document.getElementById('constraint-cell-2');
    
    if (constraintsOn) {
      // Remove constraints
      cell1.style.minWidth = '';
      cell1.style.minHeight = '';
      cell1.textContent = 'No constraints';
      
      // cell2.style.maxWidth = '';
      // cell2.style.maxHeight = '';
      layout.setColSize(1, 0); // Set preferred size
      layout.setRowSize(0, 0); 
      cell2.textContent = 'No preferred size';
    } else {
      // Add constraints back
      cell1.style.minWidth = '100px';
      cell1.style.minHeight = '50px';
      cell1.textContent = 'Min size: 100×50px';
      
      // cell2.style.maxWidth = '200px';
      // cell2.style.maxHeight = '150px';
      layout.setColSize(1, 200); // Set preferred size
      layout.setRowSize(0, 150); // Set preferred size
      cell2.textContent = 'Preferred: 200×150px';
    }
    
    constraintsOn = !constraintsOn;
    layout.refresh();
  });
  
  // Initial layout
  layout.refresh();
}

function initResizableTest() {
  const layout = new StdLayout2({
    root: document.getElementById('resizableLayoutRoot'),
    rows: [
      { stretch: 1 },
      { stretch: 1 }
    ],
    columns: [
      { stretch: 1 },
      { stretch: 1 }
    ],
    items: [
      { el: 'resize-cell-1', row: 0, col: 0 },
      { el: 'resize-cell-2', row: 0, col: 1 },
      { el: 'resize-cell-3', row: 1, col: 0 },
      { el: 'resize-cell-4', row: 1, col: 1 }
    ]
  });
  
  // ResizeObserver should handle this automatically
  
  // Initial layout
  layout.refresh();
}

function initComplexTest() {
  const layout = new StdLayout2({
    root: document.getElementById('complexLayoutRoot'),
    rows: [
      { stretch: 0, min: 50 },  // Header (fixed height)
      { stretch: 1 },           // Content area (stretchy)
      { stretch: 0, min: 40 }   // Footer (fixed height)
    ],
    columns: [
      { stretch: 0, min: 150 }, // Sidebar (fixed width)
      { stretch: 1 },           // Main content (stretchy)
      { stretch: 0, min: 180 }  // Right panel (fixed width)
    ],
    items: [
      // Header spans all columns
      { el: 'complex-header', row: 0, col: 0, colSpan: 3 },
      
      // Sidebar in the middle row, first column
      { el: 'complex-sidebar', row: 1, col: 0 },
      
      // Main content area
      { el: 'complex-content', row: 1, col: 1 },
      
      // Right panel
      { el: 'complex-panel', row: 1, col: 2 },
      
      // Footer spans all columns
      { el: 'complex-footer', row: 2, col: 0, colSpan: 3 }
    ]
  });
  
  // Initial layout
  layout.refresh();
}

function initResizableHandle() {
  const layout = new StdLayout2({
    root: document.getElementById('handleLayoutRoot'),
    rows: [
      { stretch: 1, min: 50 }, // Add min sizes to make resizing more robust
      { stretch: 1, min: 50 }
    ],
    columns: [
      { stretch: 1, min: 50, resizable: true }, // Add min sizes
      { stretch: 1, min: 50 }
    ],
    items: [
      { el: 'resize-cell-5', row: 0, col: 0 },
      { el: 'resize-cell-6', row: 0, col: 1 },
      { el: 'resize-cell-7', row: 1, col: 0 },
      { el: 'resize-cell-8', row: 1, col: 1 }
    ]
  });
  
  // The StdLayout2 constructor calls refresh(), which should create and position handles.
  // No extra calls needed here if StdLayout2 is implemented as expected.
  // layout.refresh(); // Already called by constructor typically, or ensure it is.
}


// Initialize the gallery when imported
initGallery();
document.addEventListener('DOMContentLoaded', initGridLayoutTest);
