// WTimeEdit.js  –  ES module
export default class WTimeEdit {
  static BTN_W = 40;                 // width (px) of the ▶ icon zone
  #input; #popup; #popupApi;

  /**
   * @param {HTMLInputElement} input     – the `<input type="time">`
   * @param {HTMLElement}      popupUl   – the <ul> / container that acts as the
   *                                       dropdown. Must have .wtPopup API.
   */
  constructor(input, popupUl){
    if(!(input instanceof HTMLInputElement))
      throw new TypeError('WTimeEdit: first arg must be <input>');
    this.#input   = input;
    this.#popup   = popupUl;
    this.#popupApi= popupUl.wtPopup;               // already created elsewhere

    // Pointer modelling for all devices
    input.addEventListener('pointermove',  this.#onMove);
    input.addEventListener('pointerleave', this.#onLeave);
    input.addEventListener('pointerdown',  this.#onDown);
    input.addEventListener('pointerup',    this.#onUp);
  }

  /* ---------- pointer handlers ------------------------------------- */
  #onMove = e=>{
    if(this.#input.readOnly) return;
    const hover = e.offsetX > this.#input.clientWidth - WTimeEdit.BTN_W;
    this.#input.classList.toggle('hover', hover);
  };
  #onLeave = ()=> this.#input.classList.remove('hover');

  #onDown = e=>{
    if(this.#input.readOnly) return;
    if(e.offsetX > this.#input.clientWidth - WTimeEdit.BTN_W){
      this.#input.classList.add('active','unselectable');
    }
  };
  #onUp = e=>{
    this.#input.classList.remove('unselectable');
    if(e.offsetX > this.#input.clientWidth - WTimeEdit.BTN_W){
      this.#showPopup();
    }else{
      this.#input.classList.remove('active');
    }
  };

  /* ---------- helpers ---------------------------------------------- */
  #showPopup(){
    this.#input.classList.add('active');

    // ensure we reset the button when popup closes
    this.#popupApi.onHide = ()=> this.#input.classList.remove('active');

    // position & show
    this.#popupApi.show(this.#input,'vertical');   // your helper adapts
  }
}
