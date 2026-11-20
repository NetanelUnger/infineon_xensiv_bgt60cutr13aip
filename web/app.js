import {SERVICE,RX,TX,commandPacket,Reassembler,parseFrame,polar,validateSettings,sameSettings} from './protocol.js';
import {createResearchUI} from './research.js';
import {createWifiUI} from './wifi.js';
import {createLiveRxView} from './live-rx.js';
const $=id=>document.getElementById(id);
const state={device:null,rx:null,tx:null,info:null,settings:null,schema:null,ready:false,busy:false,paused:false,frame:null,last:0,times:[],messages:0,errors:0,pending:null};
const ns='http://www.w3.org/2000/svg';
function svg(tag,attrs,parent) {const node=document.createElementNS(ns,tag);for(const [k,v] of Object.entries(attrs))node.setAttribute(k,v);parent.append(node);return node;}
function textSvg(text,attrs,parent) {const el=svg('text',attrs,parent);el.textContent=text;return el;}
function notice(message,error=false){$('notice').textContent=message;$('notice').classList.toggle('error',error);}
function controls(){const linked=!!state.device?.gatt.connected;$('connect').disabled=state.busy||linked;$('disconnect').disabled=!linked;$('pause').disabled=!state.ready;for(const id of ['apply','refresh','export','import'])$(id).disabled=!state.ready||state.busy;for(const input of $('settingsFields').querySelectorAll('input,select'))input.disabled=!state.ready||state.busy;research.update();}
function clearFrame(message){state.frame=null;state.last=0;state.times=[];$('trackCount').textContent='—';$('frameRate').textContent='—';$('latency').textContent='—';$('emptyPlot').hidden=false;$('emptyPlot').textContent=message;render();}
function status(text,kind=''){$('status').textContent=text;$('status').className=`pill ${kind}`;}
function failPending(error){if(state.pending){clearTimeout(state.pending.timer);state.pending.reject(error);state.pending=null;}}
function disconnected(){state.ready=false;state.busy=false;state.rx=null;state.tx=null;reassembler.reset();failPending(Error('החיישן התנתק'));clearFrame('החיישן מנותק');status('לא מחובר');wifi.onBleDisconnected();controls();notice('החיבור נותק. לחץ על חיבור לחיישן כדי להתחבר שוב.');}
const reassembler=new Reassembler(receive,error=>{state.errors++;console.warn(error.message);});
function receive(message){
  if(message.wifi)wifi.onWifi(message.wifi);
  state.messages++;
  if(message.tracks!==undefined){
    if(!state.paused){state.frame=parseFrame(message);state.last=performance.now();state.times.push(state.last);state.times=state.times.filter(t=>state.last-t<3000);render();}
  } else if(state.pending){
    state.pending.records.push(message);
    if(message.cmd?.id===state.pending.id){
      const p=state.pending;state.pending=null;clearTimeout(p.timer);
      const error=p.records.find(x=>x.warning||x.error);
      if(!message.cmd.state||error)p.reject(Error(error?.warning||error?.error||'הפקודה נדחתה'));else p.resolve(p.records);
    }
  }
}
function command(id,payload){
  if(state.pending)return Promise.reject(Error('פקודה קודמת עדיין ממתינה'));
  if(!state.rx||!state.device?.gatt.connected)return Promise.reject(Error('החיישן אינו מחובר'));
  const packet=commandPacket(id,payload);
  return new Promise((resolve,reject)=>{
    state.pending={id,records:[],resolve,reject,timer:setTimeout(()=>failPending(Error('לא התקבלה תשובה מהחיישן. נסה להתחבר מחדש.')),30000)};
    (async()=>{for(let i=0;i<packet.length;i+=20)await state.rx.writeValueWithResponse(packet.slice(i,i+20));})().catch(failPending);
  });
}
async function readSettings(){const response=await command(7);const data=response.find(x=>x.settings);if(!data)throw Error('לא התקבלו הגדרות');validateSettings(state.schema,data.settings);state.settings=data.settings;renderSettings();}
$('connect').addEventListener('click',async()=>{
  if(!navigator.bluetooth||!window.isSecureContext){notice('פתח את הדף ב־Chrome או Edge במחשב Windows, בכתובת localhost או HTTPS.',true);return;}
  state.busy=true;controls();status('מתחבר…');notice('בחר Radar-CSK בחלון Bluetooth של הדפדפן.');
  try{
    const device=await navigator.bluetooth.requestDevice({filters:[{services:[SERVICE]}]});
    state.device?.removeEventListener('gattserverdisconnected',disconnected);
    state.device=device;device.addEventListener('gattserverdisconnected',disconnected);
    state.info=null;state.settings=null;state.schema=null;state.messages=0;state.errors=0;reassembler.reset();
    const server=await device.gatt.connect(),service=await server.getPrimaryService(SERVICE);
    state.rx=await service.getCharacteristic(RX);state.tx=await service.getCharacteristic(TX);
    state.tx.addEventListener('characteristicvaluechanged',e=>reassembler.push(e.target.value));
    await state.tx.startNotifications();status('קורא הגדרות…');notice('מחובר. קורא זיהוי קושחה ותצורת התקנה מהחיישן…');
    const info=await command(1);state.info=info.find(x=>x.algo_info);
    if(!state.info)throw Error('הקושחה לא סיפקה את פרטי האלגוריתם');
    state.schema=state.info.algo_info.user_configuration_schema;
    await readSettings();state.ready=true;state.paused=false;$('pause').textContent='השהיית תצוגה';
    $('deviceInfo').textContent=`${device.name||'Radar-CSK'} · קושחה ${state.info.version} · ${state.info.sensor_info?.name||'BGT60CUTR13'}`;
    status('מחובר · חי','live');notice('החיבור מוכן. בדוק את גובה ההתקנה וכיוון האנטנה, ואז החל הגדרות לפי הצורך.');
    wifi.onBleConnected();
  }catch(error){state.ready=false;state.device?.gatt.disconnect();status('לא מחובר');notice(error.name==='NotFoundError'?'בחירת ההתקן בוטלה. אפשר לנסות שוב.':error.message,true);}
  finally{state.busy=false;controls();}
});
$('disconnect').onclick=()=>state.device?.gatt.disconnect();
$('pause').onclick=()=>{state.paused=!state.paused;$('pause').textContent=state.paused?'חידוש תצוגה':'השהיית תצוגה';if(state.paused){clearFrame('התצוגה מושהית');status('תצוגה מושהית');}else{clearFrame('ממתין לנתונים חדשים');status('מחובר · חי','live');}};
$('range').onchange=()=>{drawGrid();render();};$('showDetections').onchange=render;
function drawGrid(){
  const g=$('grid');g.replaceChildren();const r=Number($('range').value),cx=450,cy=442,scale=390/r;
  svg('path',{d:'M60 442 A390 390 0 0 1 840 442 Z',fill:'url(#radarFill)'},g);
  for(let i=1;i<=6;i++){const rr=390*i/6;svg('path',{d:`M${cx-rr} ${cy} A${rr} ${rr} 0 0 1 ${cx+rr} ${cy}`,class:'grid-line'},g);textSvg(`${(r*i/6).toFixed(r===4?1:0)} m`,{x:cx-rr,y:cy+22,'text-anchor':'middle'},g);}
  for(let deg=-90;deg<=90;deg+=15){const [x,y]=polar(r,deg,scale);svg('line',{x1:cx,y1:cy,x2:cx+x,y2:cy+y,class:'grid-line'},g);const [tx,ty]=polar(r+.65*r/12,deg,scale);textSvg(`${deg}°`,{x:cx+tx,y:cy+ty,'text-anchor':'middle','dominant-baseline':'middle'},g);}
  svg('circle',{cx,cy,r:6,fill:'#173e40'},g);textSvg('החיישן',{x:cx,y:cy+36,'text-anchor':'middle'},g);
}
const fmt=(v,n=2)=>Number.isFinite(v)?v.toFixed(n):'—';
function render(){
  $('points').replaceChildren();$('tracks').replaceChildren();
  const f=state.frame,range=Number($('range').value),scale=390/range;
  if(f){
    $('trackCount').textContent=f.tracks.length;$('latency').textContent=fmt(f.elapsed,0);
    $('emptyPlot').hidden=f.tracks.length>0 || ($('showDetections').checked&&f.detections.length>0);
    $('emptyPlot').textContent='אין כרגע מסלולים מזוהים';
    if($('showDetections').checked)for(const d of f.detections){
      if(d.range>range||Math.abs(d.azimuth)>90)continue;
      const [dx,dy]=polar(d.range,d.azimuth,scale),x=450+dx,y=442+dy;
      const color={1:'#bf438c',2:'#128b9b',3:'#5a64c8'}[d.type]||'#7b8890';
      const points=d.type===2?`${x},${y-6} ${x+6},${y+5} ${x-6},${y+5}`:d.type===3?`${x},${y-6} ${x+6},${y} ${x},${y+6} ${x-6},${y}`:`${x-5},${y-5} ${x+5},${y-5} ${x+5},${y+5} ${x-5},${y+5}`;
      svg('polygon',{points,fill:color,stroke:color,class:'detection'},$('points'));
    }
    for(const t of f.tracks){
      if(t.range<=range&&Math.abs(t.azimuth)<=90){const [dx,dy]=polar(t.range,t.azimuth,scale);svg('circle',{cx:450+dx,cy:442+dy,r:8,class:'track-dot'},$('points'));textSvg(`#${t.id}`,{x:462+dx,y:434+dy,class:'track-label'},$('points'));}
      const tr=document.createElement('tr');for(const value of [`#${t.id}`,fmt(t.range),fmt(t.azimuth,1),fmt(t.elevation,1),fmt(t.speed),fmt(t.activity)]){const td=document.createElement('td');td.textContent=value;tr.append(td);}tr.firstChild.className='track-badge';$('tracks').append(tr);
    }
  }
  if(!f?.tracks.length){const tr=document.createElement('tr'),td=document.createElement('td');td.colSpan=6;td.className='empty-row';td.textContent='אין מסלולים להצגה';tr.append(td);$('tracks').append(tr);}
}
function renderSettings(){
  const root=$('settingsFields');root.replaceChildren();
  function add(schema,value,parent,path=[]){
    for(const [key,s] of Object.entries(schema.entries??{})){
      const next=[...path,key];
      if(s.type==='group'){const details=document.createElement('details'),title=document.createElement('summary');title.textContent=s.title;details.append(title);details.open=path.length===0&&['sensor_mounting','fov'].includes(key);parent.append(details);add(s,value[key],details,next);}
      else{const row=document.createElement('div');row.className='setting-row';const label=document.createElement('label');label.textContent=s.title;label.title=s.description??'';
        const input=document.createElement(s.type==='enum'?'select':'input');input.id=`setting-${next.join('-')}`;input.dataset.path=JSON.stringify(next);label.htmlFor=input.id;
        if(s.type==='enum')for(const [text,v] of Object.entries(s.enumeration)){const option=document.createElement('option');option.textContent=text;option.value=v;input.append(option);}
        else if(s.type==='bool'){input.type='checkbox';input.checked=!!value[key];}
        else{input.type='number';input.required=true;input.step='any';if(s.min!==undefined)input.min=s.min;if(s.max!==undefined)input.max=s.max;}
        if(s.type!=='bool')input.value=value[key];row.append(label,input);parent.append(row);
      }
    }
  }add(state.schema,state.settings,root);controls();
}
function draft(){
  const result=structuredClone(state.settings);
  for(const input of $('settingsFields').querySelectorAll('[data-path]')){
    const path=JSON.parse(input.dataset.path);let obj=result;for(const key of path.slice(0,-1))obj=obj[key];obj[path.at(-1)]=input.type==='checkbox'?Number(input.checked):input.value===''?NaN:Number(input.value);
  }validateSettings(state.schema,result);return result;
}
async function busyAction(action){if(state.busy)return;state.busy=true;controls();try{await action();}catch(error){notice(error.message,true);}finally{state.busy=false;controls();}}
$('settingsForm').onsubmit=event=>{event.preventDefault();busyAction(async()=>{const wanted=draft();notice('מחיל הגדרות ובודק את התשובה מהחיישן…');await command(3,wanted);await readSettings();if(!sameSettings(wanted,state.settings))throw Error('החיישן החזיר הגדרות שונות. הטופס מציג את הערכים בפועל.');notice('ההגדרות הוחלו ונקראו בחזרה בהצלחה.');});};
$('refresh').onclick=()=>busyAction(async()=>{await readSettings();notice('ההגדרות נקראו מחדש מהחיישן.');});
$('export').onclick=()=>{try{const blob=new Blob([JSON.stringify({format:'radar-csk-settings-v1',settings:draft()},null,2)],{type:'application/json'});const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='radar-csk-settings.json';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000);}catch(e){notice(e.message,true);}};
$('import').onchange=async event=>{const file=event.target.files[0];if(!file)return;try{if(file.size>20000)throw Error('קובץ ההגדרות גדול מדי');const data=JSON.parse(await file.text());const settings=data.settings??data;validateSettings(state.schema,settings);state.settings=settings;renderSettings();notice('הקובץ נטען לטופס. לחץ על החל הגדרות כדי לשלוח לחיישן.');}catch(e){notice(e.message,true);}event.target.value='';};
setInterval(()=>{
  if(state.last){const age=(performance.now()-state.last)/1000;$('dataAge').textContent=`עדכון אחרון לפני ${age.toFixed(1)} שניות`;
    $('frameRate').textContent=state.times.length>1?((state.times.length-1)*1000/(state.times.at(-1)-state.times[0])).toFixed(1):'—';
    if(age>3&&!state.paused){clearFrame('הנתונים התיישנו · ממתין לעדכון');if(state.ready)status('אין נתונים חדשים','stale');}
    else if(state.ready&&!state.paused)status('מחובר · חי','live');
  }else $('dataAge').textContent=state.paused?'התצוגה מושהית':'אין נתונים';
  $('diagnostics').textContent=JSON.stringify({connected:!!state.device?.gatt.connected,firmware:state.info?.name,version:state.info?.version,algorithm:state.info?.algo_info?.version,messages:state.messages,fragmentErrors:state.errors,radarFramesDropped:state.frame?.dropped,deviceTime:state.frame?.timestamp},null,2);
},500);
if(!navigator.bluetooth||!window.isSecureContext)notice('נדרש Chrome או Edge עם Web Bluetooth. הפעל את הדף באמצעות קובץ ההפעלה המקומי.',true);
const research=createResearchUI({command,getState:()=>state,updateControls:controls});
const wifi=createWifiUI({command,getState:()=>state,liveView:createLiveRxView()});
drawGrid();render();controls();
