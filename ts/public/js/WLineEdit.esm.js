/* ------------------------------------------------------------------
 *  w-line-edit.js ⚡️ ES-module rewrite of the legacy **WLineEdit**
 *  (masking line-edit with per-character format string)
 *
 *  Mask symbols (same semantics as original):
 *    A/a — letters      · N/n — alphanum      · X/x — any char
 *    0/9 — digit (0-9)  · D/d — 1-9           · #   — digit,+,-
 *    H/h — hex          · B/b — 0 or 1        · _   — literal / skip
 *
 *  Example
 *    import { WLineEdit } from './w-line-edit.js';
 *    new WLineEdit(inputEl,{
 *      mask      : 'AAA-9999',   // ‘ABC-1234’
 *      spaceChar : ' ',
 *      keepMask  : false         // strip mask on blur
 *    });
 * ------------------------------------------------------------------ */

export default class WLineEdit {
  /**
   * @param {HTMLInputElement} input
   * @param {Object} opt
   * @param {string}  [opt.mask='']             – mask string (see above)
   * @param {string}  [opt.spaceChar=' ']       – placeholder char
   * @param {string}  [opt.caseMap='']          – per-pos '<' lower & '>' upper
   * @param {boolean} [opt.keepMask=false]      – keep mask when blurred
   */
  constructor (input, {
    mask      = '',
    spaceChar = ' ',
    caseMap   = '',
    keepMask  = false
  } = {}) {
    if (!(input instanceof HTMLInputElement))
      throw new TypeError('First arg must be <input> element');

    this.el        = input;
    this.mask      = mask;
    this.spaceChar = spaceChar;
    this.caseMap   = caseMap.padEnd(mask.length,' ');
    this.keepMask  = keepMask;
    this.raw       = mask.replace(/[^_]/g, spaceChar);   // template

    /* -------------------------------------------------------------- */
    if (mask) this.#resetValue();
    input.addEventListener('keydown' , e=>this.#onKeyDown(e));
    input.addEventListener('keypress', e=>this.#onKeyPress(e));
    input.addEventListener('paste'   , e=>this.#onPaste(e));
    input.addEventListener('cut'     , e=>this.#onCut(e));
    input.addEventListener('input'   , e=>this.#onInput(e));
    input.addEventListener('focus'   , ()=>!keepMask&&this.#resetValue());
    input.addEventListener('blur'    , ()=>!keepMask&&this.#stripMask());
  }

  /* ---------------- public helpers ---------------- */
  /** masked value as shown in the control */
  get displayValue () { return this.el.value; }
  /** “clean” value (mask removed) */
  get value () {
    if (!this.mask) return this.el.value;
    return [...this.el.value]
            .filter((ch,i)=>this.mask[i]==='_' ? ch!==this.spaceChar : true)
            .join('')
            .trim();
  }
  set value (v){ this.#setValue(v); }

  /* ---------------- internals ---------------- */
  #resetValue(){
    this.el.value = this.raw;
    this.#moveCursor(0,true);
  }
  #stripMask(){
    const before = this.value;
    this.el.value = before;
  }
  #setValue(str){
    if (!this.mask){ this.el.value = str; return; }

    this.el.value = this.raw;            // reset
    let pos = 0;
    [...str].forEach(ch=>{
      pos = this.#insertChar(ch,pos,true);
    });
    this.#moveCursor(pos,true);
  }

  /* key handling -------------------------------------------------- */
  #onKeyDown(e){
    if (!this.mask || this.el.readOnly) return;
    const {keyCode} = e;
    const sel = this.#sel();

    const isSingle = sel.len<=1;
    const moveFwd  = p=>this.#moveCursor(p, false, 1);
    const moveBack = p=>this.#moveCursor(p, true , 1);

    const valueBefore = this.value;

    switch (keyCode){
      case 39:                         /* → */
        e.preventDefault();
        isSingle ? moveFwd(sel.start+1) : this.#setSel(sel.end);
        break;
      case 37:                         /* ← */
        e.preventDefault();
        isSingle ? moveBack(sel.start-1) : this.#setSel(sel.start);
        break;
      case 36: e.preventDefault(); this.#moveCursor(0,true);
      break; /* Home */
      case 35: e.preventDefault(); this.#setSel(this.mask.length);
      break; /* End  */
      case 46:                         /* Del */
        e.preventDefault();
        if (isSingle){
          const p = sel.start;
          if (p < this.mask.length && !this.#skip(p))
            this.#replaceAt(p,this.spaceChar);
          this.#setSel(p);
        } else this.#clearRange(sel);
        break;

      case 8:                          /* Backspace */
        e.preventDefault();
        if (isSingle){
          let p = sel.start-1;
          if (p>=0){
            p = this.#moveCursor(p,true,0);
            if (!this.#skip(p)) this.#replaceAt(p,this.spaceChar);
          }
        } else this.#clearRange(sel);
        break;
    }
    if (valueBefore!==this.value) this.#fireInput();
  }

  #onKeyPress(e){
    if (!this.mask || this.el.readOnly) return;
    const code = e.charCode || e.keyCode;
    if (!code || code===13) return;           // ignore control chars
    e.preventDefault();

    const sel = this.#sel();
    if (sel.len) this.#clearRange(sel);

    const pos  = this.#insertChar(String.fromCharCode(code), sel.start);
    this.#moveCursor(pos,true);

    this.#fireInput();
  }

  /* clipboard ----------------------------------------------------- */
  #onPaste(e){
    if (!this.mask || this.el.readOnly) return;
    e.preventDefault();
    const txt = (e.clipboardData||window.clipboardData).getData('text');
    const sel = this.#sel();
    if (sel.len) this.#clearRange(sel);

    let pos=sel.start;
    for (const ch of txt) pos=this.#insertChar(ch,pos,true);
    this.#moveCursor(pos,true);
    this.#fireInput();
  }
  #onCut(e){
    if (!this.mask || this.el.readOnly) return;
    const sel = this.#sel();
    if (!sel.len) return;
    e.preventDefault();
    const cut = this.el.value.slice(sel.start, sel.end);
    (e.clipboardData||window.clipboardData).setData('text',cut);
    this.#clearRange(sel);
    this.#fireInput();
  }
  #onInput(e){
    if (this.mask && !this.el.value) this.#resetValue();
  }

  /* util helpers -------------------------------------------------- */
  #skip = pos => this.mask[pos]==='_';
  #sel(){
    const {selectionStart:s,selectionEnd:e} = this.el;
    return {start:s,end:e,len:e-s};
  }
  #setSel(p){ this.el.setSelectionRange(p,p); }
  #moveCursor(pos,forward,step=1){
    while (pos>=0 && pos<this.mask.length && this.#skip(pos))
      pos += forward ? step : -step;
    pos = Math.max(0,Math.min(pos,this.mask.length));
    this.#setSel(pos);
    return pos;
  }
  #replaceAt(i,ch){
    this.el.value = this.el.value.slice(0,i)+ch+this.el.value.slice(i+1);
  }

  /** insert a char at/on/after position (respecting mask). returns next cursor pos */
  #insertChar(ch,pos,paste=false){
    let p   = pos;
    const cp = ch.charCodeAt(0);

    // back-step if user typed preceding literal
    if (!paste && this.raw[p]!==ch && !this.#accept(cp,p) &&
        p>0 && this.raw[p-1]===ch) return p;

    while (p<this.mask.length && !this.#accept(cp,p)) p++;
    if (p===this.mask.length) return pos;           // cannot insert
    const mapped = this.caseMap[p]==='>' ? ch.toUpperCase()
                :  this.caseMap[p]==='<' ? ch.toLowerCase() : ch;
    this.#replaceAt(p,mapped);
    return ++p;
  }

  #clearRange({start,end}){
    for (let i=start;i<end;i++)
      if (!this.#skip(i)) this.#replaceAt(i,this.spaceChar);
    this.#setSel(start);
  }

  #fireInput(){ this.el.dispatchEvent(new InputEvent('input',{bubbles:true})); }

  /* char acceptance ------------------------------------------------ */
  #accept(code,pos){
    if (pos>=this.mask.length) return false;
    switch(this.mask[pos]){
      case 'A':case 'a': return isAlpha(code);
      case 'N':case 'n': return isAlpha(code)||isDigit(code);
      case 'X':case 'x': return true;
      case '0':case '9': return isDigit(code);
      case 'D':case 'd': return code>=49&&code<=57;          // 1-9
      case '#':          return isDigit(code)||code===43||code===45;
      case 'H':case 'h': return isHex(code);
      case 'B':case 'b': return code===48||code===49;
      default:           return false;
    }
  }
}

/* char helpers ---------------------------------------------------- */
const isDigit = c=>c>=48&&c<=57;
const isAlpha = c=>c>=65&&c<=90||c>=97&&c<=122;
const isHex   = c=>isDigit(c)||c>=65&&c<=70||c>=97&&c<=102;
