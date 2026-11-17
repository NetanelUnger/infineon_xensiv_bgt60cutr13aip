import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {commandPacket,Reassembler,parseFrame,polar,validateSettings,sameSettings} from '../web/protocol.js';
const records=JSON.parse(await readFile(new URL('./fixtures/radar-protocol.json',import.meta.url)));
const identity=records.find(x=>x.algo_info);
const settings=JSON.parse(await readFile(new URL('../recovery/original-settings.json',import.meta.url)));
function fragments(object,mtu){
  const bytes=new TextEncoder().encode(JSON.stringify(object)),packets=[];
  for(let off=0;off<bytes.length;off+=mtu-9){const chunk=bytes.slice(off,off+mtu-9);const p=new Uint8Array(6+chunk.length);const v=new DataView(p.buffer);v.setUint16(0,9,true);v.setUint16(2,off,true);v.setUint16(4,bytes.length,true);p.set(chunk,6);packets.push(v);}return packets;
}
test('UART protocol CRC matches Python binascii.crc_hqx vector used on hardware',()=>assert.equal(Buffer.from(commandPacket(1)).toString('hex'),'d901000084e8'));
for(const mtu of [23,247])test(`real manufacturer identity survives ${mtu}-byte MTU fragmentation`,()=>{
  const received=[],errors=[];const p=new Reassembler(x=>received.push(x),x=>errors.push(x));
  for(const f of fragments(identity,mtu))p.push(f);
  assert.deepEqual(received,[identity]);assert.equal(errors.length,0);
});
test('dropped fragment never produces partial JSON; next message recovers',()=>{
  const received=[],errors=[];const p=new Reassembler(x=>received.push(x),x=>errors.push(x));const fs=fragments(identity,247);
  fs.forEach((f,i)=>{if(i!==2)p.push(f);});assert.equal(received.length,0);assert.ok(errors.length);
  for(const f of fragments({settings},247))p.push(f);assert.deepEqual(received,[{settings}]);
});
test('bounds reject oversized and out-of-range offsets',()=>{
  let errors=0;const p=new Reassembler(()=>assert.fail(),()=>errors++);const b=new DataView(new ArrayBuffer(7));b.setUint16(4,65535,true);p.push(b);assert.equal(errors,1);
});
test('parse radar attribute names, tracks and Cartesian sign',()=>{
  const actual=records.find(x=>x.detections?.data?.length);const f=parseFrame(actual);
  assert.equal(f.detections[0].range,actual.detections.data[0][actual.detections.attributes.indexOf('range')]);
  assert.deepEqual(polar(2,0),[0,-2]);assert.ok(polar(2,30)[0]>0);assert.ok(polar(2,-30)[0]<0);
  assert.deepEqual(parseFrame({tracks:{'7':{range:3,azimuth:-12,speed:.1}}}).tracks,[{id:'7',range:3,azimuth:-12,speed:.1}]);
});
test('schema validation accepts real config and rejects unsafe/empty values',()=>{
  const schema=identity.algo_info.user_configuration_schema;validateSettings(schema,settings);
  for(const mutate of [s=>s.sensor_mounting.mounting_height_m=99,s=>s.sensor_mounting.orientation_tx=8,s=>s.fov.range_m.min=15,s=>s.tracker.micro_sensitivity=NaN]){
    const bad=structuredClone(settings);mutate(bad);assert.throws(()=>validateSettings(schema,bad));
  }
});
test('configuration readback comparison ignores JSON key order',()=>{
  const reordered=Object.fromEntries(Object.entries(settings).reverse());
  assert.ok(sameSettings(settings,reordered));
  const changed=structuredClone(settings);changed.tracker.micro_sensitivity=.5;
  assert.equal(sameSettings(settings,changed),false);
});
