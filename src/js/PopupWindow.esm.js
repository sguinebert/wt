/* -------------------------------------------------------------------------
 * PopupWindow – modern rewrite (ES 2022+)
 * -------------------------------------------------------------------------
 * import { openPopupWindow } from './PopupWindow.js';
 * openPopupWindow(url, 600, 400, (win) => console.log('Popup closed', win));
 * ------------------------------------------------------------------------- */

 export function openPopupWindow(url, width, height, onClose) {
  const computePopupPos = (width, height) => {
    const parentSize = { x: window.innerWidth, y: window.innerHeight };

    const xPos = window.screenX + Math.max(0, Math.floor((parentSize.x - width) / 2));
    const yPos = window.screenY + Math.max(0, Math.floor((parentSize.y - height) / 2));

    return { x: xPos, y: yPos };
  };

  const { x, y } = computePopupPos(width, height);

  const features = `width=${width},height=${height},status=yes,location=yes,resizable=yes,scrollbars=yes,left=${x},top=${y}`;

  const popup = window.open(url, '_blank', features);

  if (popup) {
    popup.opener = window;

    if (onClose) {
      const timer = setInterval(() => {
        if (popup.closed) {
          clearInterval(timer);
          onClose(popup);
        }
      }, 500);
    }
  }

  return popup;
}
