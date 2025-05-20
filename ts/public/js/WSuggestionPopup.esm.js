// SuggestionPopup.js --------------------------------------------------------
/* ------------------------------------------------------------------
 *  StdMatcher –        replacement for WSuggestionPopupStdMatcher
 * ------------------------------------------------------------------ */
export class StdMatcher {
  #highlightStart; #highlightEnd;
  #listSep; #white; #wordSep; #wordRE; #append;

  /**
   * @param {object} opts
   * @param {string} opts.highlightBeginTag     – e.g. "<b>"
   * @param {string} opts.highlightEndTag       – e.g. "</b>"
   * @param {string} opts.listSeparator         – e.g. ","
   * @param {string} opts.whiteSpace            – e.g. " \t"
   * @param {string} opts.wordSeparators        – additional word separators
   * @param {string} opts.wordRegexp            – overrides automatic regexp
   * @param {string} opts.appendReplacedText    – what to append after replace
   */
  constructor({
    highlightBeginTag     = '<b>',
    highlightEndTag       = '</b>',
    listSeparator         = '',
    whiteSpace            = ' \t',
    wordSeparators        = '',
    wordRegexp            = '',
    appendReplacedText    = ''
  } = {}) {
      this.#highlightStart = highlightBeginTag;
      this.#highlightEnd   = highlightEndTag;
      this.#listSep        = listSeparator;
      this.#white          = whiteSpace;
      this.#wordSep        = wordSeparators;
      this.#wordRE         = wordRegexp;
      this.#append         = appendReplacedText;

  }

  /* --- internal helpers ------------------------------------------------- */
  #parseEdit(el){
    const { value } = el;
    const pos   = el.selectionStart ?? value.length;
    let start   = this.#listSep ? value.lastIndexOf(this.#listSep, pos-1)+1 : 0;

    while (start < pos && this.#white.includes(value[start])) ++start;
    return {start,end:pos};
  }

  /* ----------------------------------------------------------------------
   * Public API expected by SuggestionPopup
   * -------------------------------------------------------------------- */
  createMatcher (editElement){
    const {start,end}=this.#parseEdit(editElement);
    const typed       = editElement.value.slice(start,end);

    // Build the regexp that will later be applied to every suggestion line
    let leading;
    if (this.#wordRE){
      leading = `(${this.#wordRE})`;
    }else if (this.#wordSep){
      const sepEsc = [...this.#wordSep].map(ch => `\\u${ch.charCodeAt(0).toString(16).padStart(4,'0')}`).join('');
      leading = `(^|[${sepEsc}])`;
    }else{
      leading = '(^)';
    }
    const esc   = s=>s.replace(/[\\^$.*+?()[\]{}|]/g,'\\$&');
    const re    = new RegExp(`${leading}(${esc(typed)})`,'gi');

    /** @param {string} suggestion */
    return suggestion=>{
      if(!typed.length) return {match:true , suggestion};
      const highlighted =  suggestion.replace(re,
                         `$1${this.#highlightStart}$2${this.#highlightEnd}`);
      return { match : highlighted!==suggestion , suggestion:highlighted };
    };
  }

  /** Replaces text in edit element with chosen suggestion */
  replace(editElement , suggestionText , suggestionValue = suggestionText){
    const {start,end}=this.#parseEdit(editElement);
    const before = editElement.value.slice(0,start);
    const after  = editElement.value.slice(end);
    editElement.value = `${before}${suggestionValue}${this.#append}${after}`;

    // caret right behind inserted text
    const pos = before.length + suggestionValue.length + this.#append.length;
    editElement.setSelectionRange(pos,pos);
  }
}

/* ------------------------------------------------------------------
 *  SuggestionPopup – modernised WSuggestionPopup
 * ------------------------------------------------------------------ */
export default class SuggestionPopup {
  /** @type {HTMLUListElement} */           #ul;
  /** @type {HTMLInputElement|HTMLTextAreaElement|null} */ #edit = null;
  /** @type {StdMatcher} */                 #matcher;
  /** @type {function} */                   #replacer;
  /** @type {number}  */                    #minLen;
  /** @type {boolean} */                    #partial;
  /** @type {boolean} */                    #iconUnfiltered;
  /** @type {boolean} */                    #autoSelect;
  /** @type {string|null} */                #lastValue = null;
  /** @type {HTMLElement|null} */           #selected = null;
  #droppedDown = false;
  #hideTimer   = null;

  /* ---- external callbacks -------------------------------------------- */
  onSelect = (_text,_value)=>{};
  onFilter = (_filter)=>{};
  onShow   = ()=>{};
  onHide   = ()=>{};

  /**
   * @param {HTMLUListElement} listElement – the <ul> that contains <li> suggestions
   * @param {object} opts
   * @param {StdMatcher}  opts.matcher
   * @param {function}    opts.replacer          – called when a suggestion chosen
   * @param {number}      [opts.filterMinLength] – minimum chars before list shown
   * @param {boolean}     [opts.filterMore]      – partial match or full?
   * @param {number}      [opts.defaultIndex]    – index pre-selected on open
   * @param {boolean}     [opts.dropDownIconUnfiltered=false]
   * @param {boolean}     [opts.autoSelect=true] – auto-select first match
   */
  constructor(listElement,{
      matcher,
      replacer ,
      filterMinLength = 0,
      filterMore      = false,
      defaultIndex    = null,
      dropDownIconUnfiltered = false,
      autoSelect      = true
    }){
    if(!(listElement instanceof HTMLElement))
      throw new TypeError('SuggestionPopup: listElement must be an element');

    this.#ul             = listElement;
    this.#matcher        = matcher;
    this.#replacer       = replacer;
    this.#minLen         = filterMinLength;
    this.#partial        = filterMore;
    this.#iconUnfiltered = dropDownIconUnfiltered;
    this.#autoSelect     = autoSelect;

    /* pointer / click on suggestion lines */
    this.#ul.addEventListener('pointerdown', e=>{
      const li = e.target.closest('li');
      if(li) this.#choose(li);
    });

    /* keep focus while scroll (Safari quirk in old code) */
    this.#ul.addEventListener('scroll',()=>{
      if(this.#edit) this.#edit.focus({preventScroll:true});
    });

    /* hide after small delay when focus moves away */
    this.#ul.addEventListener('pointerleave',()=> this.#scheduleHide());
    this.#ul.addEventListener('pointerenter', ()=> this.#clearHide());

    /* default selection index (if given) */
    if(defaultIndex!=null){
      const li = this.#ul.querySelectorAll('li')[defaultIndex] || null;
      if(li) this.#select(li);
    }

    this.#ul.hidden = true;               // start hidden
  }

  /* ------------------------------------------------------------------ */
  /*  Public API                                                        */
  /* ------------------------------------------------------------------ */

  /** Binds the popup to an <input> / <textarea> element. */
  attachInput(editElement){
    if(!(editElement instanceof HTMLInputElement ||
         editElement instanceof HTMLTextAreaElement))
      throw new TypeError('attachInput expects <input> or <textarea>');

    /* detach previous */
    this.detach();
    this.#edit = editElement;

    /* hover for dropdown icon */
    editElement.addEventListener('pointermove', this.#onPointerMove);
    editElement.addEventListener('pointerleave', this.#onPointerLeave);

    /* click / tap – maybe opens dropdown */
    editElement.addEventListener('pointerdown', this.#onEditPointerDown);

    /* text input */
    editElement.addEventListener('input',   ()=> this.#refilter());
    editElement.addEventListener('keydown', this.#onKeyDown);
    editElement.addEventListener('keyup',   this.#onKeyUp);

    /* hide when element loses focus (but let clicks in popup through) */
    editElement.addEventListener('blur',    ()=> this.#scheduleHide());
  }
  /** Remove all listeners from current edit field */
  detach(){
    if(!this.#edit) return;
    this.#edit.replaceWith(this.#edit); // cheap cloneNode hack to drop listeners
    this.#edit = null;
  }

  /** Forces popup open with the given filter (or empty string for “show all”). */
  show(filterText = ''){
    if(!this.#edit) return;
    this.#droppedDown = true;
    this.#clearHide();
    this.#refilter(filterText);
  }

  /** Programmatically hides the popup. */
  hide(){
    if(this.#ul.hidden) return;
    this.#clearHide();
    this.#ul.hidden = true;
    this.onHide();
  }

  /* ------------------------------------------------------------------ */
  /*  Internal helpers                                                  */
  /* ------------------------------------------------------------------ */
  #onPointerMove = e=>{
    if(!this.#edit) return;
    const btnWidth = 20;  // minimal – style your icon via CSS
    const inside   = e.offsetX > this.#edit.clientWidth - btnWidth;
    this.#edit.style.cursor = inside ? 'pointer' : '';
  };
  #onPointerLeave = ()=>{ if(this.#edit) this.#edit.style.cursor=''; };

  #onEditPointerDown = e=>{
    if(!this.#edit) return;
    const btnWidth = 20;
    if(e.offsetX > this.#edit.clientWidth - btnWidth){
      /* click on dropdown icon area */
      if(this.#ul.hidden) this.show('');
      else                this.hide();
    }
  };

  #onKeyDown = e=>{
    if(this.#ul.hidden) return;

    switch(e.key){
      case 'ArrowDown': case 'PageDown':
      case 'ArrowUp':   case 'PageUp':
        e.preventDefault();                             // stop caret move
        this.#moveSelection(e.key==='ArrowDown'||e.key==='PageDown',
                            e.key.startsWith('Page') ? 'page':'step');
        break;

      case 'Enter': case 'Tab':
        if(this.#selected){
          e.preventDefault();
          this.#choose(this.#selected);
        }
        break;

      case 'Escape':
        this.hide();
        break;
    }
  };
  #onKeyUp = ()=>{
    /* after navigation keys we don’t want to re-filter */
    this.#lastValue = this.#edit?.value ?? null;
  };

  #scheduleHide(){
    this.#clearHide();
    this.#hideTimer = setTimeout(()=> this.hide(), 250);
  }
  #clearHide(){ if(this.#hideTimer){ clearTimeout(this.#hideTimer); this.#hideTimer=null;} }

  #refilter(explicitText = null){
    if(!this.#edit) return;

    const text = explicitText ?? this.#edit.value;
    if(!this.#droppedDown && text.length < this.#minLen){
      this.hide();
      return;
    }
    if(text === this.#lastValue && !explicitText) return;   // nothing new
    this.#lastValue = text;

    const matcherFn = this.#matcher.createMatcher(this.#edit);
    const showAll   = this.#droppedDown && text.length===0;

    let first=null;
    this.#ul.querySelectorAll('li').forEach(li=>{
      const raw  = li.dataset.raw ?? (li.dataset.raw = li.textContent);
      const {match,suggestion} = matcherFn(raw);
      const visible = showAll || match;
      li.innerHTML  = suggestion;       // keep HTML highlighting
      li.hidden     = !visible;
      if(visible && !first) first = li;
    });

    if(!first){
      this.hide();
      return;
    }

    /* show & position */
    if(this.#ul.hidden){
      this.#ul.hidden = false;
      WT.positionAtWidget(this.#ul, this.#edit, 'vertical');
      this.onShow();
    }

    /* auto-selection */
    if(this.#autoSelect){
      this.#select(first);
    }
    this.onFilter(text);
  }

  #moveSelection(down, mode){
    const items = [...this.#ul.querySelectorAll('li:not([hidden])')];
    if(!items.length) return;

    let idx = this.#selected ? items.indexOf(this.#selected) : -1;
    if(mode==='page'){
      const page = Math.floor(this.#ul.clientHeight / (items[0].offsetHeight||1));
      idx += down ? page : -page;
    }else{
      idx += down ? 1 : -1;
    }
    idx = (idx+items.length)%items.length;
    this.#select(items[idx]);
  }

  #select(li){
    if(this.#selected){
      this.#selected.classList.remove('active');
    }
    this.#selected = li;
    li.classList.add('active');
    li.scrollIntoView({block:'nearest'});
  }

  #choose(li){
    if(!this.#edit) return;
    const raw = li.dataset.raw ?? li.textContent;
    const sug = li.textContent.replace(/<[^>]+>/g,'');   // strip markup

    this.#replacer(this.#edit, sug, raw);
    this.onSelect(sug, raw);
    this.hide();
    this.#edit.focus();
  }
}
