import test from 'node:test';
import assert from 'node:assert/strict';
import {spectrum,histogram,validateCapture,acceptPage,analyze} from '../web/research.js';
const meta={protocol:1,id:3,count:2,duration_ms:2000,samples:64,rx:3,encoding:'u12.4',order:'sample_rx'};
const tone=(bin,amp=.4,offset=.5)=>Array.from({length:192},(_,j)=>Math.round(65535*(offset+amp*Math.cos(2*Math.PI*bin*Math.floor(j/3)/64))));
const row=(ms=0)=>({ms,dropped:0,adc:tone(5)});
test('range transform locates a known tone and preserves stationary reflections',()=>{
 const s=spectrum(tone(5),0);assert.equal(s.findIndex(x=>x.db===Math.max(...s.map(x=>x.db))),5);
 assert.ok(Math.abs(s[5].db-20*Math.log10(.8))<.02);assert.ok(s[15].db<-65);
 const a=analyze({meta,rows:[row(0),row(50)]});assert.ok(a.mean[5]>-2);assert.equal(a.rate,20);
});
test('RX channels are deinterleaved and DC offset removed without cross-frame filtering',()=>{
 const adc=tone(5);for(let i=0;i<64;i++)adc[i*3+1]=32768;
 assert.ok(spectrum(adc,0)[5].db>-2);assert.ok(spectrum(adc,1).every(x=>x.db===-120));
 assert.ok(Math.abs(spectrum(tone(5,.1,.2),0)[5].db-spectrum(tone(5,.1,.7),0)[5].db)<.002);
});
test('histogram counts each sample once and includes last edge',()=>{assert.deepEqual(histogram([-120,-100,-99,-1,0],[-120,-100,-80,-60,-40,-20,0]),[1,2,0,0,0,2]);});
test('capture validation rejects damaged data and out-of-order timestamps',()=>{
 assert.doesNotThrow(()=>validateCapture(meta,[row(1),row(51)]));
 assert.throws(()=>validateCapture(meta,[row(51),row(1)]));
 assert.throws(()=>validateCapture(meta,[row(1)]));
 const r=row(1);r.adc[5]=-1;assert.throws(()=>validateCapture(meta,[r,row(51)]));
 assert.throws(()=>validateCapture({...meta,order:'rx_sample'},[row(1),row(51)]));
});
test('paged transfer rejects old capture ids, duplicates and missing offsets',()=>{
 const rows=[];acceptPage(meta,rows,{id:3,offset:0,rows:[row(1)]});
 assert.throws(()=>acceptPage(meta,rows,{id:2,offset:1,rows:[row(51)]}));
 assert.throws(()=>acceptPage(meta,rows,{id:3,offset:0,rows:[row(51)]}));
 acceptPage(meta,rows,{id:3,offset:1,rows:[row(51)]});validateCapture(meta,rows);
});
