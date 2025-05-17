import  openPopupWindow  from './js/PopupWindow.esm.js';
import Tooltip from './js/ToolTip.esm.js';
import { attachTooltip } from './js/ToolTip.esm.js';
import WDialog from './js/WDialog.esm.js';

export function initHelloWorld() {
  console.log('Hello World from module!');

  let root = document.createElement('div');
  root.id = 'helloWorld';
  root.innerHTML = `
    <h1>Hello World</h1>
    <button id="helloButton">Click me!</button>
    <span id="helloResponse"></span>
    <button id="showDlgBtn">Show Dialog</button>
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

      /* helper: one global instance so we don’t build twice */
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
}