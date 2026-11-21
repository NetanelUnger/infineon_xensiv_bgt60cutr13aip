import {FILE_MAGIC, RawStats, validateRawMetadata} from './raw-protocol.js';

const N = 64, BINS = 33, CHANNELS = 17 * 3, WIDTH = CHANNELS * BINS;
const window64 = Float64Array.from({length:N}, (_,i) => .5 - .5 * Math.cos(2 * Math.PI * i / (N - 1)));
const gain = window64.reduce((sum,x) => sum + x, 0);
const reverse = Uint8Array.from({length:N}, (_,i) => Number.parseInt(i.toString(2).padStart(6,'0').split('').reverse().join(''),2));
export const powerDb = power => 10 * Math.log10(Math.max(1e-12, power));

// Same DC removal, symmetric Hann window and dBFS convention as research.js.
// Each chirp is transformed separately; power is averaged, never complex phase.
export function chirpPower(frame, chirp, rx) {
  const re = new Float64Array(N), im = new Float64Array(N), samples = new Float64Array(N);
  let mean = 0;
  for (let i=0;i<N;i++) mean += samples[i] = frame.adc(chirp,i,rx) / 65536;
  mean /= N;
  for (let i=0;i<N;i++) re[reverse[i]] = (samples[i] - mean) * window64[i];
  for (let size=2;size<=N;size*=2) {
    const half=size/2;
    for (let j=0;j<half;j++) {
      const wr=Math.cos(-2*Math.PI*j/size), wi=Math.sin(-2*Math.PI*j/size);
      for (let base=0;base<N;base+=size) {
        const a=base+j, b=a+half, tr=wr*re[b]-wi*im[b], ti=wr*im[b]+wi*re[b];
        re[b]=re[a]-tr;im[b]=im[a]-ti;re[a]+=tr;im[a]+=ti;
      }
    }
  }
  return Float64Array.from({length:BINS}, (_,k) => (re[k]**2+im[k]**2) * ((k===0||k===32?2:4)/gain)**2);
}

// Bounded timeline. Every frame contributes to both the exact global mean and
// a time bucket. Widen buckets as needed, retaining counts and missing-data flags.
export class PowerTimeline {
  constructor(maxRows=512) {
    if (!Number.isInteger(maxRows)||maxRows<2) throw Error('Invalid timeline limit');
    this.maxRows=maxRows;this.bucketMs=50;this.rows=[];
  }
  merge(a,b) {
    a.lastMs=b.lastMs;a.timeSum+=b.timeSum;a.frames+=b.frames;
    a.gap ||= b.gap;
    for(let i=0;i<WIDTH;i++) a.sum[i]+=b.sum[i];
  }
  add(ms, powers, gap=false) {
    const row={firstMs:ms,lastMs:ms,timeSum:ms,frames:1,gap,sum:powers.slice()};
    const last=this.rows.at(-1);
    if(last && Math.floor(last.firstMs/this.bucketMs)===Math.floor(ms/this.bucketMs)) this.merge(last,row);
    else this.rows.push(row);
    while(this.rows.length>this.maxRows) {
      this.bucketMs*=2;
      const compact=[];
      for(const r of this.rows) {
        const previous=compact.at(-1);
        if(previous && Math.floor(previous.firstMs/this.bucketMs)===Math.floor(r.firstMs/this.bucketMs)) this.merge(previous,r);
        else compact.push(r);
      }
      this.rows=compact;
    }
  }
  finish() {
    return this.rows.map(({sum,timeSum,...r}) => ({...r,ms:timeSum/r.frames,power:Float32Array.from(sum,x=>x/r.frames)}));
  }
}

// Read at most 1 MiB at a time. File/Blob contents are never loaded wholesale.
class BlobReader {
  constructor(file) {this.file=file;this.offset=0;this.start=0;this.bytes=new Uint8Array();}
  async read(size) {
    if(this.offset+size>this.file.size) return null;
    if(this.offset<this.start || this.offset+size>this.start+this.bytes.length) {
      this.start=this.offset;
      this.bytes=new Uint8Array(await this.file.slice(this.offset,this.offset+Math.max(size,1024*1024)).arrayBuffer());
    }
    const result=this.bytes.slice(this.offset-this.start,this.offset-this.start+size);
    this.offset+=size;return result;
  }
}

function finishQuality(meta, stats, footer, truncated) {
  const warnings=[];
  const s=footer?.server;
  const validServer=s?.type==='stopped' && s.session===meta.session &&
    ['sent','offered','queue_dropped','fifo_errors'].every(k=>Number.isSafeInteger(s[k])&&s[k]>=0) &&
    s.sent===stats.frames && s.offered===s.sent+s.queue_dropped &&
    s.queue_dropped>=stats.queueDropped && s.fifo_errors>=stats.fifoErrors;
  const validClient=!footer?.client || Object.entries(stats.summary()).every(([k,v])=>footer.client[k]===v);
  const complete=!!(footer?.complete===true && validServer && validClient && !truncated);
  const fifo=Math.max(stats.fifoErrors,validServer?s.fifo_errors:0);
  const queue=Math.max(stats.queueDropped,validServer?s.queue_dropped:0);
  if(truncated) warnings.push('סוף הקובץ נקטע. נותחו רק המסגרות השלמות שלפניו.');
  if(!footer) warnings.push('חסר אישור סיום; אי אפשר לוודא שכל ההקלטה נשמרה.');
  else if(!complete) warnings.push('ההקלטה חלקית או שמוני הסיום אינם תואמים לנתונים.');
  if(stats.gaps||fifo||queue) warnings.push(`אובדן מדווח: ${stats.gaps} פערי מסגרות, ${queue} השמטות בתור, ${fifo} שגיאות FIFO. המונים עשויים לחפוף.`);
  return {complete,lossFree:complete&&!stats.gaps&&!fifo&&!queue,warnings,fifo,queue};
}

export async function analyzeRawFile(file,{onProgress=()=>{},maxRows=512}={}) {
  const reader=new BlobReader(file), magic=await reader.read(8);
  if(!magic||magic.some((b,i)=>b!==FILE_MAGIC[i])) throw Error('נדרש קובץ RAW מסוג ‎.radar (RADC0001). קובצי JSON מהקלטת BLE נטענים באזור ״החזרים לאורך זמן״.');
  const decoder=new TextDecoder('utf-8',{fatal:true}), total=new Float64Array(WIDTH), clipped=new Float64Array(CHANNELS);
  const timeline=new PowerTimeline(maxRows);
  let meta,stats,footer,truncated=false,maxGapMs=0;
  onProgress(0);
  while(reader.offset<file.size) {
    if(footer) throw Error('נמצאו נתונים אחרי סיום ההקלטה.');
    const prefix=await reader.read(4);
    if(!prefix) {truncated=true;break;}
    const size=new DataView(prefix.buffer).getUint32(0,true);
    if(size<1||size>1024*1024) throw Error('אורך רשומה לא תקין בקובץ RAW.');
    const bytes=await reader.read(size);
    if(!bytes) {truncated=true;break;}
    if(size>=4 && new DataView(bytes.buffer).getUint32(0,true)===0x31524452) {
      if(!meta) throw Error('חסרות הגדרות ההקלטה לפני הנתונים.');
      const previous=stats.last;
      const f=stats.push(bytes.buffer), powers=new Float64Array(WIDTH);
      const dt=previous?(f.ms-previous.ms)>>>0:0;
      maxGapMs=Math.max(maxGapMs,dt);
      const gap=!!(previous && (((f.sequence-previous.sequence-1)>>>0)>0 || f.fifoErrors>previous.fifoErrors));
      for(let c=0;c<17;c++) for(let r=0;r<3;r++) {
        const channel=c*3+r, offset=channel*BINS, power=chirpPower(f,c,r);
        powers.set(power,offset);
        for(let k=0;k<BINS;k++) total[offset+k]+=power[k];
        for(let i=0;i<N;i++) {const v=f.adc(c,i,r);if(v<=16||v>=65519)clipped[channel]++;}
      }
      timeline.add(stats.elapsedMs,powers,gap);
      if(stats.frames%64===0) onProgress(reader.offset/file.size);
    } else {
      let item;
      try {item=JSON.parse(decoder.decode(bytes));} catch {throw Error('רשומת נתונים פגומה בקובץ RAW.');}
      if(!meta && item?.type==='started') {
        meta=validateRawMetadata(item);
        if(new Set(meta.registers.map(r=>r[0])).size!==meta.registers.length) throw Error('כתובות הגדרה כפולות בקובץ RAW.');
        stats=new RawStats(meta);
      } else if(meta && item?.type==='footer') footer=item;
      else throw Error('רשומה לא צפויה בקובץ RAW.');
    }
  }
  if(!stats?.frames) throw Error('הקובץ אינו מכיל מסגרות RAW שלמות לניתוח.');
  const quality=finishQuality(meta,stats,footer,truncated);
  onProgress(1);
  return {name:file.name||'capture.radar',size:file.size,meta,quality,clipped,
    summary:{...stats.summary(),rate:stats.rate,maxGapMs},bucketMs:timeline.bucketMs,
    mean:Float64Array.from(total,x=>x/stats.frames),timeline:timeline.finish()};
}

export function selectedPower(values,rx=0,chirp=-1) {
  if(!Number.isInteger(rx)||rx<0||rx>2||!Number.isInteger(chirp)||chirp< -1||chirp>16) throw Error('Invalid antenna/chirp');
  const result=new Float64Array(BINS);
  const first=chirp<0?0:chirp, last=chirp<0?17:chirp+1;
  for(let c=first;c<last;c++) for(let k=0;k<BINS;k++) result[k]+=values[(c*3+rx)*BINS+k]/(last-first);
  return result;
}

export function selection(data,rx=0,chirp=-1) {
  const mean=selectedPower(data.mean,rx,chirp);
  let clipped=0;
  for(let c=0;c<17;c++) if(chirp<0||c===chirp)clipped+=data.clipped[c*3+rx];
  return {mean,db:Array.from(mean,powerDb),clipped,
    timeline:data.timeline.map(row=>({...row,db:Array.from(selectedPower(row.power,rx,chirp),powerDb)}))};
}

export function compareMetadata(a,b) {
  const reasons=[];
  for(const k of ['sensor','config','samples','rx','chirps','averages','encoding','order','system_clock_hz','fifo_threshold'])
    if(a[k]!==b[k]) reasons.push(`הגדרת ${k} שונה`);
  const registers=m=>JSON.stringify([...m.registers].sort((x,y)=>x[0]-y[0]));
  if(registers(a)!==registers(b)) reasons.push('הגדרות השידור/הקליטה של החיישן שונות');
  return {compatible:reasons.length===0,reasons};
}
