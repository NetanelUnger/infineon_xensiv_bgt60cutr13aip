import {test} from 'node:test';
import assert from 'node:assert/strict';
import {createWifiUI} from '../web/wifi.js';

const meta={type:'started',format:'radar-raw-v1',protocol:1,session:7,encoding:'u12.4',byte_order:'little',order:'chirp_sample_rx',header_bytes:40,chirps:17,samples:64,rx:3,averages:16,registers:[[1,2]]};
const tick=()=>new Promise(resolve=>setImmediate(resolve));
function setup() {
  const elements=new Map(), timers=new Map();let timerId=0;
  globalThis.document={getElementById(id){if(!elements.has(id))elements.set(id,{value:'',disabled:false,hidden:false,textContent:'',classList:{toggle(){}},addEventListener(event,callback){this[event]=callback;}});return elements.get(id);}};
  globalThis.setInterval=()=>0;
  globalThis.setTimeout=callback=>{timers.set(++timerId,callback);return timerId;};
  globalThis.clearTimeout=id=>timers.delete(id);
  const writes=[];
  globalThis.window={addEventListener(){},async showSaveFilePicker(){return{async createWritable(){return{async write(bytes){writes.push(bytes.slice());},async close(){writes.push('closed');}};}};}};
  const sockets=[];
  globalThis.WebSocket=class {
    static OPEN=1;
    constructor(url){this.url=url;this.readyState=1;this.sent=[];sockets.push(this);}
    send(s){this.sent.push(JSON.parse(s));}
    close(){this.readyState=3;this.onclose?.();}
    receive(value){this.onmessage({data:value instanceof ArrayBuffer?value:JSON.stringify(value)});}
  };
  const state={ready:true,info:{capabilities:{raw_websocket:1}},settings:{height:2}};
  const commands=[];
  const ui=createWifiUI({command:async(id,payload)=>{commands.push({id,payload});return [];},getState:()=>state});
  const el=id=>document.getElementById(id);
  ui.onWifi({protocol:1,state:'connected',ip:'192.168.1.4',port:8080,path:'/radar',busy:false});
  el('rawConnect').onclick();const ws=sockets[0];ws.receive({type:'hello',protocol:1});
  return {ui,el,ws,writes,state,commands,timers,sockets};
}
function dataFrame(seq=0){const b=new ArrayBuffer(6568),v=new DataView(b);new Uint8Array(b).set([82,68,82,49]);for(const [p,n]of[[4,1],[6,40],[28,17],[30,3],[32,64],[34,16]])v.setUint16(p,n,true);for(const[p,n]of[[8,7],[12,seq],[16,1000+seq*50],[36,6528]])v.setUint32(p,n,true);return b;}
function jsonRecords(writes){return writes.filter(v=>v instanceof Uint8Array&&v.length>4&&v[4]===123).map(v=>JSON.parse(new TextDecoder().decode(v.slice(4))));}
test('BLE provisioning and full capture work concurrently; stop waits for the final frame',async()=>{
  const {ui,el,ws,writes,state,commands}=setup();
  el('wifiSsid').value=' network ';el('wifiPassword').value='secret123';
  await el('wifiForm').submit({preventDefault(){}});
  assert.deepEqual(commands[0],{id:30,payload:{ssid:' network ',password:'secret123'}});assert.equal(el('wifiPassword').value,'');
  await el('rawRecord').onclick();assert.deepEqual(ws.sent,[{command:'start'}]);
  ws.receive(meta);ws.receive(dataFrame());
  state.ready=false;ui.onBleDisconnected();assert.equal(ws.readyState,1);
  el('rawStop').onclick();assert.deepEqual(ws.sent[1],{command:'stop'});
  ws.receive(dataFrame(1));ws.receive({type:'stopped',session:7,reason:'requested',offered:2,sent:2,queue_dropped:0,fifo_errors:0});
  await tick();
  const [saved,footer]=jsonRecords(writes);assert.equal(saved.settings.height,2);assert.equal(footer.complete,true);assert.equal(footer.loss_free,true);assert.equal(footer.client.frames,2);assert.equal(writes.at(-1),'closed');
  assert.match(el('rawStatus').textContent,/2 מסגרות/);
});
test('unexpected disconnect closes the file with a partial footer and no auto restart',async()=>{
  const {el,ws,writes}=setup();await el('rawRecord').onclick();ws.receive(meta);ws.receive(dataFrame());ws.close();await tick();
  const footer=jsonRecords(writes).at(-1);assert.equal(footer.complete,false);assert.equal(footer.reason,'connection_lost');assert.equal(footer.client.frames,1);assert.equal(el('rawRecord').disabled,true);
});
test('bad frame is not written and terminates acquisition',async()=>{
  const {el,ws,writes}=setup();await el('rawRecord').onclick();ws.receive(meta);ws.receive(dataFrame().slice(0,-1));await tick();
  const footer=jsonRecords(writes).at(-1);assert.equal(footer.complete,false);assert.equal(footer.client.frames,0);assert.equal(footer.reason,'protocol_error');assert.equal(ws.readyState,3);
});
test('trailing drops reported only in stop summary remain visible and mark loss_free false',async()=>{
  const {el,ws,writes}=setup();await el('rawRecord').onclick();ws.receive(meta);ws.receive(dataFrame());
  el('rawStop').onclick();ws.receive({type:'stopped',session:7,reason:'requested',offered:4,sent:1,queue_dropped:3,fifo_errors:2});await tick();
  const footer=jsonRecords(writes).at(-1);assert.equal(footer.complete,true);assert.equal(footer.loss_free,false);assert.equal(el('rawLoss').textContent,'3 / 2');
});
test('Wi-Fi setup yields to the lab on join, returns on leave, and starts preview only in the Wi-Fi tab',()=>{
  const {ui,el,ws}=setup();
  assert.equal(el('wifiLab').hidden,false);
  assert.equal(el('wifiSetup').hidden,true);
  ui.onWifi({protocol:1,state:'disconnected'});
  assert.equal(el('wifiLab').hidden,true);
  assert.equal(el('wifiSetup').hidden,false);
  assert.equal(ws.readyState,3);
  ui.setTabActive(true);
  ui.onWifi({protocol:1,state:'connected',ip:'192.168.1.4',port:8080,path:'/radar'});
  assert.equal(el('rawDisconnect').disabled,false);
  assert.equal(el('liveRxStop').disabled,false);
});
test('known-address mode works without BLE and reveals the lab only after the protocol handshake',()=>{
  const {ui,el,state,sockets}=setup();
  ui.onWifi({protocol:1,state:'disconnected'});state.ready=false;ui.onBleDisconnected();
  el('directWifiIp').value='192.168.1.42';
  el('directWifiForm').submit({preventDefault(){}});
  const ws=sockets.at(-1);
  assert.equal(ws.url,'ws://192.168.1.42:8080/radar');
  assert.equal(el('wifiLab').hidden,true);
  ws.receive({type:'hello',protocol:1});
  assert.equal(el('wifiLab').hidden,false);
  assert.deepEqual(ws.sent,[{command:'start'}]);
  el('wifiLeave').onclick();
  assert.equal(el('wifiLab').hidden,true);
  assert.equal(el('wifiSetup').hidden,false);
});
