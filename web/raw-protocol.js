const encoder = new TextEncoder();
export const FILE_MAGIC = encoder.encode('RADC0001');
export const RAW_HEADER = 40;

export function wifiErrorMessage(wifi) {
  const code=wifi.error_code;
  if(wifi.initialized===false)return `אתחול ה־Wi-Fi בכרטיס נכשל (קוד ${wifi.driver_error||code}).`;
  if(code===33555476)return 'הכרטיס לא הצליח להתחבר בשיטת החיבור שהרשת מפרסמת. קוד 33555476 מציין אי־תאימות בשיטת החיבור, ולא בהכרח סיסמה שגויה.';
  if(code===137232422)return 'הכרטיס לא מצא את הרשת או לא הצליח לזהות את שיטת החיבור שלה. בדוק את שם הרשת והקליטה.';
  if(code===137232432)return 'הכרטיס התחבר לרשת, אך לא קיבל כתובת IP בזמן. בדוק שהנתב מחלק כתובות DHCP.';
  return `החיבור לרשת נכשל (קוד ${code}). בדוק את פרטי הרשת ואת זמינותה.`;
}

export function wifiCredentials(ssid, password) {
  const length = encoder.encode(ssid).length;
  if (!length || length > 32 || ssid.includes('\0')) throw Error('שם הרשת חייב להכיל 1–32 בתים.');
  if (encoder.encode(password).length > 63 || password.includes('\0')) throw Error('סיסמת הרשת ארוכה מדי (עד 63 בתים).');
  return {ssid, password}; // Spaces and case are significant; do not trim.
}

export function wifiEndpoint(wifi) {
  if (wifi?.state !== 'connected') return null;
  const parts = String(wifi.ip).split('.');
  if (parts.length !== 4 || parts.some(p => !/^\d{1,3}$/.test(p) || Number(p) > 255) || parts.every(p => Number(p) === 0)) throw Error('כתובת IP לא תקינה מהכרטיס.');
  if (!Number.isInteger(wifi.port) || wifi.port < 1 || wifi.port > 65535 || wifi.path !== '/radar') throw Error('כתובת שירות ההקלטה אינה תקינה.');
  return `ws://${parts.map(Number).join('.')}:${wifi.port}/radar`;
}

export function validateRawMetadata(meta) {
  if (meta?.type !== 'started' || meta.format !== 'radar-raw-v1' || meta.protocol !== 1 ||
      !Number.isInteger(meta.session) || meta.session < 1 || meta.session > 0xffffffff ||
      meta.encoding !== 'u12.4' || meta.byte_order !== 'little' || meta.order !== 'chirp_sample_rx' ||
      meta.header_bytes !== RAW_HEADER || meta.chirps !== 17 || meta.samples !== 64 || meta.rx !== 3 || meta.averages !== 16 ||
      !Array.isArray(meta.registers) || !meta.registers.length || meta.registers.length > 3000 ||
      meta.registers.some(r => !Array.isArray(r) || r.length !== 2 || r.some(v => !Number.isInteger(v) || v < 0 || v > 0xffffffff))) {
    throw Error('פורמט ההקלטה המלאה אינו נתמך או שהגדרות החיישן השתנו.');
  }
  return meta;
}

export function parseRawFrame(buffer, meta) {
  if (!(buffer instanceof ArrayBuffer) || buffer.byteLength < RAW_HEADER) throw Error('מסגרת RAW חסרה.');
  const v = new DataView(buffer);
  if (v.getUint32(0, true) !== 0x31524452 || v.getUint16(4, true) !== 1 || v.getUint16(6, true) !== RAW_HEADER) throw Error('כותרת RAW לא תקינה.');
  const frame = {session:v.getUint32(8,true), sequence:v.getUint32(12,true), ms:v.getUint32(16,true),
    fifoErrors:v.getUint32(20,true), queueDropped:v.getUint32(24,true), chirps:v.getUint16(28,true),
    rx:v.getUint16(30,true), samples:v.getUint16(32,true), averages:v.getUint16(34,true), payloadBytes:v.getUint32(36,true)};
  if (frame.session !== meta.session || ['chirps','rx','samples','averages'].some(k => frame[k] !== meta[k]) ||
      frame.payloadBytes !== frame.chirps * frame.rx * frame.samples * 2 || buffer.byteLength !== RAW_HEADER + frame.payloadBytes) throw Error('מסגרת RAW אינה תואמת להקלטה.');
  // DataView preserves the little-endian file contract on every host platform.
  frame.adc = (chirp, sample, rx) => {
    if (![chirp,sample,rx].every(Number.isInteger) || chirp < 0 || chirp >= frame.chirps || sample < 0 || sample >= frame.samples || rx < 0 || rx >= frame.rx) throw RangeError('ADC index');
    return v.getUint16(RAW_HEADER + 2 * ((chirp * frame.samples + sample) * frame.rx + rx), true);
  };
  return frame;
}

export class RawStats {
  constructor(meta) { this.meta = validateRawMetadata(meta); this.frames=0;this.bytes=0;this.gaps=0;this.elapsedMs=0;this.last=null;this.fifoErrors=0;this.queueDropped=0; }
  push(buffer) {
    const f = this.validate(buffer);
    const gap = this.last ? (f.sequence - this.last.sequence - 1) >>> 0 : f.sequence;
    if (this.last) this.elapsedMs += (f.ms - this.last.ms) >>> 0;
    this.gaps += gap; this.frames++; this.bytes += buffer.byteLength;
    this.last=f;this.fifoErrors=f.fifoErrors;this.queueDropped=f.queueDropped;
    return f;
  }
  validate(buffer) {
    const f = parseRawFrame(buffer, this.meta);
    const gap = this.last ? (f.sequence - this.last.sequence - 1) >>> 0 : f.sequence;
    if (gap > 0x7fffffff) throw Error('מסגרות RAW כפולות או בסדר שגוי.');
    if (f.fifoErrors < this.fifoErrors || f.queueDropped < this.queueDropped) throw Error('מוני האובדן חזרו לאחור.');
    if (this.last) {
      const delta = (f.ms - this.last.ms) >>> 0;
      if (delta > 0x7fffffff) throw Error('זמני RAW חזרו לאחור.');
    }
    return f;
  }
  get rate() { return this.elapsedMs ? (this.frames-1)*1000/this.elapsedMs : 0; }
  summary() { return {frames:this.frames,bytes:this.bytes,sequence_gaps:this.gaps,fifo_errors:this.fifoErrors,queue_dropped:this.queueDropped,elapsed_ms:this.elapsedMs}; }
}

export function fileRecord(value) {
  const payload = value instanceof ArrayBuffer ? new Uint8Array(value) : encoder.encode(JSON.stringify(value));
  const result = new Uint8Array(4 + payload.byteLength);
  new DataView(result.buffer).setUint32(0, payload.byteLength, true); result.set(payload,4);
  return result;
}

/* Bounded asynchronous disk queue; a slow disk stops capture rather than growing
 * browser memory indefinitely. Rejections are observed even before finish(). */
export class CaptureSink {
  constructor(writer=null, {memoryLimit=64*1024*1024, pendingLimit=8*1024*1024}={}) {
    this.writer=writer;this.memoryLimit=memoryLimit;this.pendingLimit=pendingLimit;
    this.parts=[];this.bytes=0;this.pending=0;this.error=null;this.chain=Promise.resolve();this.closed=false;
    this.append(FILE_MAGIC);
  }
  append(bytes) {
    if (this.closed) throw Error('הקובץ כבר נסגר.');
    if (this.error) throw this.error;
    if (!this.writer) {
      if (this.bytes+bytes.byteLength > this.memoryLimit) throw Error('ההקלטה הגיעה למגבלת הזיכרון. שמור אותה והתחל הקלטה חדשה.');
      this.parts.push(bytes);this.bytes+=bytes.byteLength;return;
    }
    if (this.pending+bytes.byteLength > this.pendingLimit) throw Error('הדיסק אינו עומד בקצב ההקלטה. ההקלטה נעצרה.');
    this.pending+=bytes.byteLength;this.bytes+=bytes.byteLength;
    this.chain=this.chain.then(async()=>{if(!this.error)await this.writer.write(bytes);})
      .catch(e=>{this.error=e;}).finally(()=>{this.pending-=bytes.byteLength;});
  }
  async finish(footer) {
    if(this.closed)return;
    // Let accepted disk writes drain so the footer cannot hit the queue budget.
    await this.chain;
    // Reserve a small footer even if the in-memory frame budget was exhausted.
    if(!this.writer)this.memoryLimit+=8192;
    try { this.append(fileRecord(footer)); } catch(e) { this.error ||= e; }
    this.closed=true;await this.chain;
    if(this.writer) { try { await this.writer.close(); } catch(e) { this.error ||= e; } }
    if(this.error)throw this.error;
  }
  blob() { return new Blob(this.parts,{type:'application/octet-stream'}); }
}
