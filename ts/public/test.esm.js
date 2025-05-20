import { computePosition, offset, flip, shift } from './vendor/floating-ui/floating-ui.core.browser.min.mjs';
import WDialog from './js/WDialog.esm.js';
import WPopupWidget from './js/WPopupWidget.esm.js';
import { attachTooltip } from './js/ToolTip.esm.js';
import openPopupWindow from './js/PopupWindow.esm.js';

// New widget imports
import WLineEdit from './js/WLineEdit.esm.js';
import WSpinBox from './js/WSpinBox.esm.js';
import WTimeEdit from './js/WTimeEdit.esm.js';
import WSuggestionPopup from './js/WSuggestionPopup.esm.js';
import WDateEdit from './js/WDateEdit.esm.js';

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
      margin-top: 15px;
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
    `<div class="wt-spinbox">
      <button class="spinbox-down">-</button>
      <input type="text" class="spinbox-input" value="0">
      <button class="spinbox-up">+</button>
    </div>`,
    'Numeric spinner with increment/decrement buttons'
  );
  
  const spinInput = spinBoxCard.querySelector('.spinbox-input');
  const upBtn = spinBoxCard.querySelector('.spinbox-up');
  const downBtn = spinBoxCard.querySelector('.spinbox-down');
  
  upBtn.addEventListener('click', () => {
    spinInput.value = parseInt(spinInput.value || 0) + 1;
  });
  
  downBtn.addEventListener('click', () => {
    spinInput.value = Math.max(0, parseInt(spinInput.value || 0) - 1);
  });
  
  container.appendChild(spinBoxCard);
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
      popupEl.style.position = 'absolute';
      
      const updatePosition = () => {
        computePosition(popupBtn, popupEl, {
          platform, 
          placement: 'bottom',
          middleware: [offset(8), flip(), shift()]
        }).then(({x, y}) => {
          Object.assign(popupEl.style, { left: `${x}px`, top: `${y}px` });
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
      
      // Position suggestion popup
      computePosition(fruitInput, suggestionPopup, {
        platform,
        placement: 'bottom-start',
        middleware: [offset(2), flip(), shift()]
      }).then(({x, y}) => {
        Object.assign(suggestionPopup.style, {
          position: 'absolute',
          left: `${x}px`,
          top: `${y}px`,
          width: `${fruitInput.offsetWidth}px`
        });
      });
      
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

// Initialize the gallery when imported
initGallery();
