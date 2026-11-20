export const SERVICE = '83f09d00-62e6-4b88-9c74-2d519c840cb1';
export const RX = '83f09d01-62e6-4b88-9c74-2d519c840cb1';
export const TX = '83f09d02-62e6-4b88-9c74-2d519c840cb1';
export function commandPacket(id, payload) {
  if (![1,3,7,20,21,22,23,30,31,32].includes(id)) throw Error('Unsupported command');
  const bytes = payload === undefined ? new Uint8Array() : new TextEncoder().encode(JSON.stringify(payload));
  if (bytes.length>2048) throw Error('Configuration exceeds device limit');
  const p = new Uint8Array(bytes.length+6), view = new DataView(p.buffer);
  p[0]=0xd9;p[1]=id;view.setUint16(2,bytes.length,true);p.set(bytes,4);
  let crc=0xffff;
  for(const b of p.subarray(0,-2)) {crc^=b<<8;for(let i=0;i<8;i++) crc=((crc&0x8000)?(crc<<1)^0x1021:crc<<1)&0xffff;}
  view.setUint16(p.length-2,crc,true);return p;
}
export class Reassembler {
  constructor(onMessage,onError=()=>{}) {this.onMessage=onMessage;this.onError=onError;this.reset();}
  reset() {this.pending=null;}
  push(view) {
    try {
      if(view.byteLength<7) throw Error('Short notification');
      const id=view.getUint16(0,true),off=view.getUint16(2,true),total=view.getUint16(4,true);
      if(!total || total>12288) throw Error('Message size out of bounds');
      if(off===0) this.pending={id,total,used:0,bytes:new Uint8Array(total),time:Date.now()};
      const p=this.pending;
      if(!p || id!==p.id || total!==p.total || off!==p.used || Date.now()-p.time>20000 || off+view.byteLength-6>total) throw Error('Incomplete notification sequence');
      p.bytes.set(new Uint8Array(view.buffer,view.byteOffset+6,view.byteLength-6),off);p.used+=view.byteLength-6;
      if(p.used===total) {this.pending=null;this.onMessage(JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(p.bytes)));}
    } catch(error) {this.reset();this.onError(error);}
  }
}
export function polar(range,azimuth,scale=1) {
  const radians=azimuth*Math.PI/180;
  return [range*Math.sin(radians)*scale,-range*Math.cos(radians)*scale];
}
export function parseFrame(raw) {
  const finite = (...values)=>values.every(Number.isFinite);
  const tracks=Object.entries(raw.tracks??{}).map(([id,t])=>({id,...t})).filter(t=>finite(t.range,t.azimuth)&&t.range>=0);
  const {attributes=[],data=[]}=raw.detections??{};
  const detections=data.map(row=>Object.fromEntries(attributes.map((k,i)=>[k,row[i]]))).filter(d=>finite(d.range,d.azimuth)&&d.range>=0);
  return {tracks,detections,elapsed:raw.elapsed_ms,dropped:raw.frames_dropped,timestamp:raw.ts};
}
export function validateSettings(schema,value,path='') {
  if(schema.type==='group') {
    if(!value || typeof value!=='object') throw Error(`${path}: missing group`);
    for(const [key,entry] of Object.entries(schema.entries??{})) validateSettings(entry,value[key],`${path}${path?'.':''}${key}`);
  } else {
    if(!Number.isFinite(value)) throw Error(`${path}: required number`);
    if(schema.type==='enum' && !Object.values(schema.enumeration).includes(value)) throw Error(`${path}: invalid choice`);
    if(schema.type==='bool' && ![0,1].includes(value)) throw Error(`${path}: expected 0 or 1`);
    if(schema.min!==undefined && value<schema.min || schema.max!==undefined && value>schema.max) throw Error(`${path}: outside allowed range`);
  }
  if(!path && value.fov) for(const key of ['range_m','angle_deg']) {
    const {min,max}=value.fov[key];if(min>=max) throw Error(`Field of view: ${key} minimum must be below maximum`);
  }
}
export function sameSettings(a,b) {
  if(a===b)return true;
  if(!a||!b||typeof a!=='object'||typeof b!=='object')return false;
  const keys=Object.keys(a).sort(),other=Object.keys(b).sort();
  return keys.length===other.length&&keys.every((k,i)=>k===other[i]&&sameSettings(a[k],b[k]));
}
