import {test} from 'node:test';
import assert from 'node:assert/strict';
import {wifiCredentials,wifiEndpoint,wifiErrorMessage,RawStats,parseRawFrame,CaptureSink,fileRecord,FILE_MAGIC} from '../web/raw-protocol.js';
import {commandPacket} from '../web/protocol.js';

export const meta={type:'started',format:'radar-raw-v1',protocol:1,session:19,encoding:'u12.4',byte_order:'little',order:'chirp_sample_rx',header_bytes:40,chirps:17,samples:64,rx:3,averages:16,registers:[[16,4294967295]]};
export function frame(sequence=0,ms=1000,queue=0) {
  const b=new ArrayBuffer(6568),v=new DataView(b);
  new Uint8Array(b).set(new TextEncoder().encode('RDR1'));
  for(const [p,x] of [[4,1],[6,40],[28,17],[30,3],[32,64],[34,16]])v.setUint16(p,x,true);
  for(const [p,x] of [[8,19],[12,sequence],[16,ms],[24,queue],[36,6528]])v.setUint32(p,x,true);
  for(let c=0;c<17;c++)for(let s=0;s<64;s++)for(let r=0;r<3;r++)v.setUint16(40+2*((c*64+s)*3+r),c*3000+s*3+r,true);
  return b;
}
test('Wi-Fi mode mismatch is distinguished from initialization, missing AP and DHCP failures',()=>{
  assert.match(wifiErrorMessage({initialized:true,error_code:33555476}),/אי־תאימות/);
  assert.match(wifiErrorMessage({initialized:false,error_code:137232395,driver_error:67240448}),/אתחול.*67240448/);
  assert.match(wifiErrorMessage({initialized:true,error_code:137232422}),/לא מצא/);
  assert.match(wifiErrorMessage({initialized:true,error_code:137232432}),/DHCP/);
});
test('provisioning preserves significant spaces and validates UTF-8 byte limits',()=>{
  assert.deepEqual(wifiCredentials(' My network ',' secret '),{ssid:' My network ',password:' secret '});
  assert.doesNotThrow(()=>wifiCredentials('א'.repeat(16),''));
  for(const ssid of ['', 'א'.repeat(17),'a\0b'])assert.throws(()=>wifiCredentials(ssid,''));
  assert.throws(()=>wifiCredentials('x','א'.repeat(32)));
  for(const id of [30,31,32])assert.ok(commandPacket(id,id===30?wifiCredentials('net','password'):undefined).length>=6);
  assert.equal(wifiEndpoint({state:'connected',ip:'192.168.1.20',port:8080,path:'/radar'}),'ws://192.168.1.20:8080/radar');
  assert.equal(wifiEndpoint({state:'disconnected'}),null);
  assert.throws(()=>wifiEndpoint({state:'connected',ip:'192.168.1.300',port:8080,path:'/radar'}));
});
test('all 17 chirps and three interleaved antennas survive the binary frame',()=>{
  const f=parseRawFrame(frame(),meta);
  for(let c=0;c<17;c++)for(let s=0;s<64;s++)for(let r=0;r<3;r++)assert.equal(f.adc(c,s,r),c*3000+s*3+r);
  assert.throws(()=>f.adc(17,0,0));
  assert.throws(()=>parseRawFrame(frame().slice(0,-1),meta));
  assert.throws(()=>parseRawFrame(frame(),{...meta,session:20}));
});
test('sequence gaps, initial dropped frames, duplicate rejection and clock wrap',()=>{
  const stats=new RawStats(meta);
  stats.push(frame(2,0xfffffff0,2));stats.push(frame(4,34,3));
  assert.equal(stats.gaps,3);assert.equal(stats.elapsedMs,50);assert.equal(stats.rate,20);
  assert.throws(()=>stats.push(frame(4,34,3)));assert.equal(stats.frames,2);
  assert.throws(()=>stats.push(frame(5,84,2)));assert.equal(stats.frames,2);
  assert.throws(()=>new RawStats({...meta,chirps:1}));
});
test('memory limit stops before accepting a frame, retaining a readable partial footer',async()=>{
  const sink=new CaptureSink(null,{memoryLimit:7200});
  sink.append(fileRecord(meta));sink.append(fileRecord(frame()));
  assert.throws(()=>sink.append(fileRecord(frame(1,1050))));
  await sink.finish({type:'footer',complete:false,reason:'storage_limit'});
  const b=new Uint8Array(await sink.blob().arrayBuffer());
  assert.deepEqual(b.slice(0,8),FILE_MAGIC);
  const records=[];for(let p=8;p<b.length;){const n=new DataView(b.buffer).getUint32(p,true);records.push(b.slice(p+4,p+4+n));p+=4+n;}
  assert.equal(records.length,3);assert.equal(records[1].length,6568);
  assert.equal(JSON.parse(new TextDecoder().decode(records[2])).complete,false);
});
test('slow disk queue drains in order and still accepts the footer after saturation',async()=>{
  const writes=[];let release;const blocked=new Promise(resolve=>release=resolve);
  const writer={async write(bytes){await blocked;writes.push(bytes.slice());},async close(){writes.push('closed');}};
  const sink=new CaptureSink(writer,{pendingLimit:6600});sink.append(fileRecord(frame()));
  assert.throws(()=>sink.append(fileRecord(frame(1,1050))));
  const finish=sink.finish({type:'footer',complete:false});release();await finish;
  assert.equal(writes.length,4);assert.equal(writes[1].length,6572);assert.equal(writes[3],'closed');
  assert.equal(sink.pending,0);
});
test('disk write failures are observed and never reported as a successful file',async()=>{
  let closed=false;
  const sink=new CaptureSink({async write(){throw Error('disk full');},async close(){closed=true;}});
  await assert.rejects(sink.finish({type:'footer',complete:true}),/disk full/);assert.ok(closed);
});
