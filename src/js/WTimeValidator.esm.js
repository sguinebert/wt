// TimeValidator.js  ---------------------------------------------------------
export default class TimeValidator {
  /** @typedef {{
   *   regexp:   string,                        // e.g. '(\\d\\d?):(\\d\\d)'
   *   getHour:  (RegExpExecArray)=>number,
   *   getMin:   (RegExpExecArray)=>number,
   *   getSec?:  (RegExpExecArray)=>number,
   *   getMs?:   (RegExpExecArray)=>number
   * }} TimeFormat */
  #mandatory; #formats; #min; #max; #msg;

  /**
   * @param {object} opts
   * @param {boolean}          [opts.mandatory=false]
   * @param {TimeFormat[]}     opts.formats                 – at least one!
   * @param {Date|null}        [opts.min]                   – lower bound
   * @param {Date|null}        [opts.max]                   – upper bound
   * @param {object}           opts.messages
   * @param {string}           opts.messages.blank
   * @param {string}           opts.messages.format
   * @param {string}           opts.messages.tooSmall
   * @param {string}           opts.messages.tooLarge
   */
  constructor({
    mandatory = false,
    formats,
    min = null,
    max = null,
    messages : {
      blank    : blankMsg,
      format   : formatMsg,
      tooSmall : tooSmallMsg,
      tooLarge : tooLargeMsg,
    },
  }){
    this.#mandatory = mandatory;
    this.#min       = min;
    this.#max       = max;
    this.#msg       = { blankMsg, formatMsg, tooSmallMsg, tooLargeMsg };

    // pre-compile regexps for speed
    this.#formats   = formats.map(f=>({
      ...f,
      re : new RegExp(`^${f.regexp}$`, 'i')   // ignore case (am/pm)
    }));
  }

  /**
   * Validate a textual time representation.
   * @param  {string} text
   * @return {{valid:true}|{valid:false,message:string}}
   */
  validate(text=''){
    const { blankMsg, formatMsg, tooSmallMsg, tooLargeMsg } = this.#msg;

    // ---------- Step 1 : blank field -------------------------------------
    if(text.trim()===''){
      return this.#mandatory
        ? { valid:false, message:blankMsg }
        : { valid:true };
    }

    // ---------- Step 2 : parse using supplied formats --------------------
    let h=-1,m=-1,s=-1,ms=-1;
    const upper = text.toUpperCase();

    for(const f of this.#formats){
      const mExec = f.re.exec(text);
      if(!mExec) continue;

      h  = f.getHour(mExec);
      m  = f.getMin (mExec);
      s  = f.getSec ? f.getSec(mExec) : 0;
      ms = f.getMs  ? f.getMs (mExec) : 0;

      // deal with AM/PM if present in text
      if(upper.includes('P') && h < 12) h+=12;
      if(upper.includes('A') && h===12) h=0;
      break;
    }
    if(h<0) return { valid:false, message:formatMsg };

    // ---------- Step 3 : range sanity check ------------------------------
    if(h>23||m>59||s>59||ms>999)
      return { valid:false, message:formatMsg };

    const dt = new Date(0,0,0,h,m,s,ms);          // makes overflow impossible
    if(dt.getHours()!==h||dt.getMinutes()!==m||
       dt.getSeconds()!==s||dt.getMilliseconds()!==ms)
      return { valid:false, message:formatMsg };

    // ---------- Step 4 : min / max bounds --------------------------------
    if(this.#min && dt < this.#min)
      return { valid:false, message:tooSmallMsg };
    if(this.#max && dt > this.#max)
      return { valid:false, message:tooLargeMsg };

    return { valid:true };
  }
}
