import {test} from 'node:test';
import assert from 'node:assert/strict';
import {FILE_MAGIC,fileRecord,RawStats,parseRawFrame} from '../web/raw-protocol.js';
import {analyzeRawFile,chirpPower,powerDb,selection,compareMetadata} from '../web/raw-analysis.js';
import {spectrum} from '../web/research.js';

const meta={type:'started',format:'radar-raw-v1',protocol:1,session:7,sensor:'BGT60CUTR13AIP',config:'test',encoding:'u12.4',byte_order:'little',order:'chirp_sample_rx',header_bytes:40,chirps:17,samples:64,rx:3,averages:16,registers:[[16,100],[32,200]],system_clock_hz:80000000};
function frame(seq=0,ms=0,signal=()=>32768) {
  const buffer=new ArrayBuffer(6568),v=new DataView(buffer);
  v.setUint32(0,0x31524452,true);
  for(const [p,n] of [[4,1],[6,40],[28,17],[30,3],[32,64],[34,16]])v.setUint16(p,n,true);
  for(const [p,n] of [[8,7],[12,seq],[16,ms],[36,6528]])v.setUint32(p,n,true);
  for(let c=0;c<17;c++)for(let s=0;s<64;s++)for(let r=0;r<3;r++)v.setUint16(40+2*((c*64+s)*3+r),Math.round(signal(c,s,r)),true);
  return buffer;
}
const tone=(amplitude=8192,bin=8)=>(c,s,r)=>32768+amplitude*Math.cos(2*Math.PI*bin*s/64);
function file(frames,{metadata=meta,footer=true,footerChange={},tail=[]}={}) {
  const parts=[FILE_MAGIC,fileRecord(metadata),...frames.map(fileRecord)];
  if(footer) {
    const stats=new RawStats(metadata);frames.forEach(f=>stats.push(f));
    parts.push(fileRecord({type:'footer',complete:true,loss_free:true,server:{type:'stopped',session:7,offered:frames.length,sent:frames.length,queue_dropped:0,fifo_errors:0},client:stats.summary(),...footerChange}));
  }
  const result=new Blob([...parts,...tail]);result.name='test.radar';return result;
}

test('FFT agrees with the independent legacy DFT for every bin and RX',()=>{
  const buffer=frame(0,0,(c,s,r)=>31000+7000*Math.cos(2*Math.PI*(r+3)*s/64)+2000*Math.sin(2*Math.PI*11*s/64));
  const f=parseRawFrame(buffer,meta),adc=Array.from({length:192},(_,i)=>f.adc(0,Math.floor(i/3),i%3));
  for(let r=0;r<3;r++) {
    const actual=chirpPower(f,0,r),expected=spectrum(adc,r);
    for(let k=0;k<33;k++)assert.ok(Math.abs(powerDb(actual[k])-expected[k].db)<1e-7,`RX ${r} bin ${k}`);
  }
});

test('DC alone vanishes; known amplitude doubling gives +6.02 dB',async()=>{
  const dc=selection(await analyzeRawFile(file([frame()])));
  assert.ok(dc.db.every(v=>v===-120));
  const a=selection(await analyzeRawFile(file([frame(0,0,tone())])));
  const b=selection(await analyzeRawFile(file([frame(0,0,tone(16384))])));
  assert.ok(Math.abs(b.db[8]-a.db[8]-20*Math.log10(2))<.001);
  assert.equal(a.db.indexOf(Math.max(...a.db)),8);
});

test('all 17 chirps and RX3 contribute; averaging is linear power, not dB',async()=>{
  const data=await analyzeRawFile(file([frame(0,0,(c,s,r)=>32768+(c===16&&r===2?8192*Math.cos(2*Math.PI*8*s/64):0))]));
  const all=selection(data,2,-1),last=selection(data,2,16),other=selection(data,0,-1);
  assert.ok(Math.abs(all.mean[8]*17-last.mean[8])<1e-12);
  assert.ok(Math.abs(last.db[8]-all.db[8]-10*Math.log10(17))<1e-9);
  assert.ok(other.db.every(v=>v===-120));
  assert.throws(()=>selection(data,3,-1));assert.throws(()=>selection(data,0,17));
});

test('bounded timeline retains weighted means, actual times and loss flags',async()=>{
  const frames=Array.from({length:57},(_,i)=>frame(i<30?i:i+2,1000+i*50+(i>=30?100:0),tone(i<30?4000:12000)));
  const data=await analyzeRawFile(file(frames,{footer:false}),{maxRows:8});
  assert.ok(data.timeline.length<=8);
  assert.equal(data.timeline.reduce((s,r)=>s+r.frames,0),57);
  assert.equal(data.timeline[0].firstMs,0);assert.equal(data.timeline.at(-1).lastMs,2900);
  assert.equal(data.summary.sequence_gaps,2);assert.ok(data.timeline.some(r=>r.gap));
  const selected=selection(data);
  const weighted=selected.timeline.reduce((sum,r)=>sum+10**(r.db[8]/10)*r.frames,0)/57;
  assert.ok(Math.abs(weighted-selected.mean[8])<1e-8);
});

test('unsigned software-clock wrap stays relative and monotonic',async()=>{
  const data=await analyzeRawFile(file([frame(0,0xfffffff0),frame(1,34),frame(2,84)]));
  assert.equal(data.summary.elapsed_ms,100);assert.equal(data.summary.rate,20);
  assert.deepEqual(data.timeline.map(r=>r.ms),[0,50,100]);
});

test('complete files are verified against both server and client counters',async()=>{
  const frames=[frame(),frame(1,50)];
  const complete=await analyzeRawFile(file(frames));assert.equal(complete.quality.lossFree,true);
  for(const footerChange of [{client:{frames:999}},{server:{type:'stopped',session:8,sent:2,offered:2,queue_dropped:0,fifo_errors:0}},{complete:false}]) {
    const data=await analyzeRawFile(file(frames,{footerChange}));assert.equal(data.quality.complete,false);assert.ok(data.quality.warnings.length);
  }
  const tailLoss=await analyzeRawFile(file(frames,{footerChange:{server:{type:'stopped',session:7,sent:2,offered:3,queue_dropped:1,fifo_errors:1}}}));
  assert.equal(tailLoss.quality.complete,true);assert.equal(tailLoss.quality.lossFree,false);
  assert.equal(tailLoss.quality.queue,1);assert.equal(tailLoss.quality.fifo,1);
});

test('missing footer and truncated tail recover only complete frames, with warnings',async()=>{
  for(const tail of [[],[new Uint8Array([1,2])],[new Uint8Array([100,0,0,0,1,2,3])]]) {
    const data=await analyzeRawFile(file([frame(),frame(1,50)],{footer:false,tail}));
    assert.equal(data.summary.frames,2);assert.equal(data.quality.complete,false);assert.ok(data.quality.warnings.length);
    if(tail.length)assert.match(data.quality.warnings.join(' '),/נקטע/);
  }
});

test('malformed records, wrong sessions, reordered frames and data after footer are rejected',async()=>{
  const wrong=frame();new DataView(wrong).setUint32(8,99,true);
  const duplicate=frame(1,50);
  const invalid=[new Blob(['not radar']),new Blob([FILE_MAGIC]),new Blob([FILE_MAGIC,fileRecord(wrong)]),
    file([frame()],{tail:[new Uint8Array([0])]}),
    file([frame()],{footer:false,tail:[new Uint8Array([255,255,255,127])]}),
    file([frame()],{footer:false,tail:[fileRecord({type:'unknown'})]}),
    file([frame()],{footer:false,tail:[fileRecord(wrong)]}),
    file([frame(),duplicate],{footer:false,tail:[fileRecord(duplicate)]}),
    file([frame()],{metadata:{...meta,registers:[[16,1],[16,2]]},footer:false})];
  for(const value of invalid)await assert.rejects(analyzeRawFile(value));
});

test('hardware compatibility ignores register ordering and session identity, detects acquisition changes',()=>{
  assert.equal(compareMetadata(meta,{...meta,session:999,registers:[...meta.registers].reverse(),firmware:'different-label'}).compatible,true);
  for(const changes of [{registers:[[16,101],[32,200]]},{system_clock_hz:100000000},{config:'other'},{averages:8}])
    assert.equal(compareMetadata(meta,{...meta,...changes}).compatible,false);
});

test('clipping counts follow the selected chirp and RX, including final chirp',async()=>{
  const data=await analyzeRawFile(file([frame(0,0,(c,s,r)=>c===16&&r===1?(s<32?0:65535):32768)]));
  assert.equal(selection(data,1,16).clipped,64);assert.equal(selection(data,1,-1).clipped,64);
  assert.equal(selection(data,0,-1).clipped,0);assert.equal(selection(data,1,0).clipped,0);
});

test('large files are sliced with a bounded read size and progress reaches completion',async()=>{
  const source=file(Array.from({length:170},(_,i)=>frame(i,i*50,tone()))),progress=[];
  let largestRead=0;
  const bounded={size:source.size,name:'bounded.radar',slice(start,end){largestRead=Math.max(largestRead,end-start);return source.slice(start,end);}};
  const data=await analyzeRawFile(bounded,{onProgress:p=>progress.push(p),maxRows:16});
  assert.ok(source.size>1024*1024);assert.ok(largestRead<=1024*1024);
  assert.equal(data.summary.frames,170);assert.ok(data.timeline.length<=16);
  assert.equal(progress[0],0);assert.equal(progress.at(-1),1);
  assert.ok(progress.every((p,i)=>i===0||p>=progress[i-1]));
});
