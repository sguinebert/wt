import  openPopupWindow  from './js/PopupWindow.esm.js';
import Tooltip from './js/ToolTip.esm.js';
import { attachTooltip } from './js/ToolTip.esm.js';
import WDialog from './js/WDialog.esm.js';

import { computePosition, offset, flip, shift } from './vendor/floating-ui/floating-ui.core.browser.min.mjs';
import WPopupWidget from './js/WPopupWidget.esm.js';

// Create a test function that sets up everything
export function initPopupTest() {
  // Create button and popup elements
  const container = document.createElement('div');
  container.id = 'popupTest';
  container.classList.add('Wt-domRoot');
  container.innerHTML = `
    <button id="popupButton">Show Popup</button>
    
    <div id="myPopup" style="display: none; background: white; padding: 10px; border: 1px solid #ccc; box-shadow: 0 2px 10px rgba(0,0,0,0.1); border-radius: 4px; width: 200px;">
      <h4>Popup Content</h4>
      <p>This is a popup positioned with Floating UI.</p>
    </div>
  `;
  document.body.appendChild(container);
  
  // Get references
  const button = document.getElementById('popupButton');
  const popupEl = document.getElementById('myPopup');
  
  // Initialize the popup widget
  const popup = new WPopupWidget(myPopup, {
    transient: true,
    autoHideDelay: 300
  });
  
  // Use Floating UI to position when showing
  button.addEventListener('click', () => {
    // First make visible so we can measure it
    //popupEl.hidden = false;
    console.log('popupEl', popupEl);

      // Let popup widget handle the rest
      if(popupEl.hidden)
        popup.show('popupButton'); // No anchor parameter so it doesn't reposition
      else
        popup.hide()
  });
  
  // Return the references in case we need them later
  return { button, popup };
}

export function initHelloWorld() {
  console.log('Hello World from module!');

  const platform = {
    isRTL: () => document.documentElement.dir === 'rtl',
    getElementRects: ({ reference, floating }) => ({
      reference: reference.getBoundingClientRect(),
      floating: floating.getBoundingClientRect()
    }),
    getDimensions: (element) => {
      const rect = element.getBoundingClientRect();
      return {
        width: rect.width,
        height: rect.height
      };
    },
    getClippingRect: () => {
      return {
        width: window.innerWidth,
        height: window.innerHeight,
        x: 0,
        y: 0
      };
    }
  };

  let root = document.createElement('div');
  root.id = 'helloWorld';
  root.innerHTML = `
    <h1>Hello World</h1>
    <button id="helloButton">Click me!</button>
    <span id="helloResponse"></span>
    <button id="showDlgBtn">Show Dialog</button>
    
    <!-- New button for Floating UI test -->
    <button id="floatingBtn">Show Floating UI Menu</button>
    
    <!-- Floating element that will be positioned -->
    <div id="floatingMenu" style="display: none; background-color: white; border: 1px solid #ccc; padding: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); border-radius: 4px; width: 200px;">
      <div>Floating UI Menu</div>
      <ul style="margin: 0; padding: 0; list-style: none;">
        <li style="padding: 8px 0;">Menu Item 1</li>
        <li style="padding: 8px 0;">Menu Item 2</li>
        <li style="padding: 8px 0;">Menu Item 3</li>
      </ul>
    </div>
    
    <div id="myDialog" class="dialog">
      <div class="dialog-title">Hello World Dialog</div>
      <div class="dialog-content">
        <p>This is a draggable dialog.</p>
        <p>Click and drag the title bar to move it around.</p>
      </div>
  `;

  document.body.appendChild(root);

  const btn   = root.querySelector('#helloButton');
  const reply = root.querySelector('#helloResponse');

  // tooltip (hover)
  attachTooltip(btn, { content: 'Opens a pop-up window' });

  // pop-up (click)
  btn.addEventListener('click', () => {
    // show a modal / pop-up – assumes your PopupWindow.esm.js exports that function
    openPopupWindow('https://google.com', 500, 500, (popup) => {
      // this is the callback function that gets called when the pop-up is opened
      // you can use the popup object to manipulate the pop-up window if needed
      console.log('Pop-up closed:', popup);
    });

    // also write something under the button for demo purposes
    reply.textContent = 'Pop-up was opened at ' + new Date().toLocaleTimeString();
  });

  /* helper: one global instance so we don't build twice */
  let dlg = null;

  document.getElementById('showDlgBtn').addEventListener('click', () => {
    if (!dlg) {
      dlg = new WDialog({
        el       : document.getElementById('myDialog'),
        titlebar : '.dialog-title',
        movable  : true,
        centerX  : true,
        centerY  : true,
        onMove   : (x,y)=>console.log('moved to',x,y),
        onZIndex : z   =>console.log('z-index =>',z)
      });
    }
    dlg.bringToFront();
  });

  /* nice touch: tooltip on the launcher button */
  attachTooltip(document.getElementById('showDlgBtn'), {
    content: 'Click to open a draggable dialog'
  });
  
  // Floating UI test
  const floatingBtn = document.getElementById('floatingBtn');
  const floatingMenu = document.getElementById('floatingMenu');
  let menuVisible = false;
  let cleanup = null;
  
  floatingBtn.addEventListener('click', () => {
    menuVisible = !menuVisible;
    
    if (menuVisible) {
      // Show the menu before positioning it
      floatingMenu.style.display = 'block';
      floatingMenu.style.position = 'absolute';
      
      // Function to update position
      const updatePosition = () => {
        computePosition(floatingBtn, floatingMenu, {
          platform, 
          placement: 'bottom',
          middleware: [
            offset(8),
            flip(),
            shift()
          ]
        }).then(({x, y}) => {
          Object.assign(floatingMenu.style, {
            left: `${x}px`,
            top: `${y}px`
          });
        });
      };
      
      // Initial positioning
      updatePosition();
      
      // Setup continuous updates
      const resizeObserver = new ResizeObserver(() => updatePosition());
      resizeObserver.observe(floatingBtn);
      resizeObserver.observe(document.body);
      
      // Still need scroll events (no perfect ScrollObserver yet)
      const scrollListener = () => updatePosition();
      window.addEventListener('scroll', scrollListener, true);

      let previousRect = floatingBtn.getBoundingClientRect();
      const layoutShiftObserver = new IntersectionObserver(() => {
        const currentRect = floatingBtn.getBoundingClientRect();
        if (
          previousRect.x !== currentRect.x ||
          previousRect.y !== currentRect.y
        ) {
          updatePosition();
          previousRect = currentRect;
        }
      }, { threshold: 0.5 });
      layoutShiftObserver.observe(floatingBtn);
      
      // Clean up function
      cleanup = () => {
        layoutShiftObserver.disconnect();
        resizeObserver.disconnect();
        window.removeEventListener('scroll', scrollListener, true);
      };
    } else {
      // Hide the menu and clean up
      floatingMenu.style.display = 'none';
      if (cleanup) {
        cleanup();
        cleanup = null;
      }
    }
  });
  
  // Add tooltip to floating button
  initPopupTest();
}
