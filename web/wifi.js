import {wifiCredentials,wifiEndpoint,wifiErrorMessage,RawStats,CaptureSink,fileRecord} from './raw-protocol.js';

export function createWifiUI({command,getState,liveView,lab}) {
  const $=id=>document.getElementById(id);
  let wifi=null, socket=null, socketReady=false, stage='idle', sink=null, stats=null;
  let poll=null, startTimer=null, stopTimer=null, ending=null, localBusy=false, preparedSettings=null;
  let savedName='', lastBlob=null, requestedReason='connection_lost';
  let previewAfterHello=false, socketError='', boardId=null;
  let tabActive=false;
  let directConnection=false;
  const say=(text,error=false)=>{$('wifiStatus').textContent=text;$('wifiStatus').classList.toggle('error',error);};
  const rawSay=(text,error=false)=>{$('rawStatus').textContent=text;$('rawStatus').classList.toggle('error',error);};
  const supported=()=>getState().info?.capabilities?.raw_websocket===1;
  const canBle=()=>getState().ready&&!getState().busy&&!getState().pending&&!localBusy;
  function update() {
    const hasBle=canBle()&&supported();
    const joined=wifi?.state==='connected';
    $('wifiSetup').hidden=joined;
    $('wifiLab').hidden=!joined;
    $('wifiSsid').disabled=$('wifiPassword').disabled=!hasBle||wifi?.busy;
    $('directWifiSetup').hidden=getState().ready;
    $('directWifiConnect').disabled=!!socket||stage!=='idle';
    $('wifiJoin').disabled=!hasBle||wifi?.busy;
    $('wifiRefresh').disabled=!hasBle;
    $('wifiLeave').disabled=directConnection?!socket:!hasBle||wifi?.busy||!joined;
    $('wifiLeave').textContent=directConnection?'סגור חיבור ישיר':'ניתוק מרשת Wi-Fi';
    $('rawConnect').disabled=!!socket||wifi?.state!=='connected';
    $('rawDisconnect').disabled=!socket;
    $('rawRecord').disabled=!socketReady||stage!=='idle';
    $('rawStop').disabled=!sink||!['starting','recording'].includes(stage);
    $('liveRxStart').disabled=stage!=='idle'||(socket?!socketReady:wifi?.state!=='connected');
    $('liveRxStop').disabled=!!sink||!(previewAfterHello||['starting','streaming'].includes(stage));
    $('rawSave').disabled=!lastBlob;
    $('rawSave').hidden=!lastBlob;
    $('wifiUnavailable').hidden=!getState().ready||supported();
  }
  function showStats() {
    $('rawFrames').textContent=stats?.frames??0;
    $('rawChirps').textContent=(stats?.frames??0)*(stats?.meta.chirps??17);
    $('rawBytes').textContent=((stats?.bytes??0)/1048576).toFixed(2);
    $('rawRate').textContent=stats?.rate.toFixed(1)??'0.0';
    $('rawLoss').textContent=stats?`${Math.max(stats.gaps,stats.queueDropped)} / ${stats.fifoErrors}`:'0 / 0';
  }
  function onWifi(value) {
    if(value?.protocol!==1)return;
    let previous,endpoint;
    try {previous=wifiEndpoint(wifi);endpoint=wifiEndpoint(value);}catch(e){say(e.message,true);return;}
    wifi=value;
    $('wifiAddress').textContent=endpoint??'—';
    const labels={idle:'הרשת עדיין לא הוגדרה.',connecting:'הכרטיס מתחבר לרשת…',connected:'הכרטיס מחובר לרשת.',disconnected:'ה־Wi-Fi מנותק. חיבור ה־BLE נשאר זמין.'};
    say(value.state==='error'?wifiErrorMessage(value):(labels[value.state]??value.state),value.state==='error');
    if(socket&&previous&&endpoint!==previous) { requestedReason='network_changed'; socket.close(); }
    clearTimeout(poll);
    if(value.busy||value.state==='connecting')poll=setTimeout(refreshIfIdle,2000);
    else if(value.state==='connected')poll=setTimeout(refreshIfIdle,10000);
    if(tabActive&&endpoint&&!previous&&!socket)connectSocket(true);
    update();
  }
  async function refreshIfIdle() {
    if(!getState().ready)return;
    if(!supported())return;
    if(!canBle()){poll=setTimeout(refreshIfIdle,1000);return;}
    try {const records=await command(31);for(const record of records)if(record.wifi)onWifi(record.wifi);}
    catch(e){say(e.message,true);}finally{update();}
  }
  $('wifiForm').addEventListener('submit',async event=>{
    event.preventDefault();
    if(!canBle()||!supported())return;
    localBusy=true;update();
    try {
      const credentials=wifiCredentials($('wifiSsid').value,$('wifiPassword').value);
      await command(30,credentials);$('wifiPassword').value='';
      say('פרטי הרשת נשלחו. ממתין לכתובת IP…');
      poll=setTimeout(refreshIfIdle,1000);
    }catch(e){say(e.message,true);}finally{localBusy=false;update();}
  });
  $('wifiRefresh').onclick=refreshIfIdle;
  $('wifiLeave').onclick=async()=>{if(directConnection){requestedReason='disconnected';socket?.close();return;}try{await command(32);say('מנתק את הרשת…');poll=setTimeout(refreshIfIdle,1000);}catch(e){say(e.message,true);}finally{update();}};

  async function finish(reason,server=null) {
    if(ending)return ending;
    clearTimeout(startTimer);clearTimeout(stopTimer);
    liveView?.clear('התצוגה החיה הסתיימה.');
    lab?.clear('התצוגה החיה הסתיימה.');
    if(!sink){stage='idle';update();return;}
    stage='saving';update();
    const current=sink;
    ending=(async()=>{
      let complete=reason==='requested'&&!!stats&&server?.session===stats.meta.session&&server.sent===stats.frames&&server.offered===server.sent+server.queue_dropped;
      if(stats&&server){stats.queueDropped=Math.max(stats.queueDropped,server.queue_dropped??0);stats.fifoErrors=Math.max(stats.fifoErrors,server.fifo_errors??0);showStats();}
      const lossFree=complete&&!stats.gaps&&!stats.queueDropped&&!stats.fifoErrors;
      try {
        await current.finish({type:'footer',complete,loss_free:lossFree,reason,server,client:stats?.summary()??null,finished_at:new Date().toISOString()});
        lastBlob=current.writer?null:current.blob();
        rawSay(`${complete?'ההקלטה הסתיימה':'נשמרה הקלטה חלקית'} · ${stats?.frames??0} מסגרות. ${current.writer?'הקובץ נשמר בדיסק.':'לחץ על שמירת ההקלטה.'}${stats?.gaps||stats?.queueDropped||stats?.fifoErrors?' קיימים פערים באיסוף.':''}`,!lossFree);
      }catch(e){rawSay(`שמירת ההקלטה נכשלה: ${e.message}`,true);if(!current.writer)lastBlob=current.blob();}
      finally{sink=null;stage='idle';ending=null;update();}
    })();
    return ending;
  }

  function abort(reason,error) {
    socketError=error?.message??'החיבור לנתונים הופסק.';
    rawSay(socketError,true);requestedReason=reason;
    // Disconnect terminates the producer even if a stop acknowledgement is lost.
    if(socket)socket.close();
    void finish(reason);
  }

  function message(event, directAddress) {
    try {
      if(event.data instanceof ArrayBuffer) {
        if(!stats||!['streaming','recording','stopping'].includes(stage))throw Error('התקבלה מסגרת מחוץ להפעלה.');
        // Rejected frames must not inflate the count of records saved to disk.
        if(sink) {
          stats.validate(event.data);
          try{sink.append(fileRecord(event.data));}catch(e){abort('storage_limit',e);return;}
        }
        const frame=stats.push(event.data);
        liveView?.show(frame);
        lab?.show(frame);
        if(frame.sequence%10===0)showStats();
        return;
      }
      const value=JSON.parse(event.data);
      if(value.type==='hello') {
        if(value.protocol!==1)throw Error('גרסת WebSocket אינה נתמכת.');
        if(directAddress) {
          directConnection=true;wifi=directAddress;boardId=null;
          lab?.resetSamples();
          $('wifiAddress').textContent=wifiEndpoint(wifi);
          say('חיבור ישיר לכרטיס דרך Wi-Fi.');
        }
        socketReady=true;rawSay('חיבור הנתונים מוכן.');
        if(previewAfterHello){previewAfterHello=false;beginStream();}
      }else if(value.type==='started') {
        if(stage!=='starting')throw Error('התחלת הזרם אינה צפויה.');
        clearTimeout(startTimer);stats=new RawStats(value);
        lab?.beginSession(value);
        if(sink)sink.append(fileRecord({...value,recorded_at:new Date().toISOString(),settings:preparedSettings}));
        stage=sink?'recording':'streaming';
        rawSay(sink?'מקליט את כל הצ׳ירפים…':'הזרם פעיל דרך Wi-Fi.');
        liveView?.clear('ממתין למסגרת הראשונה מהכרטיס.');showStats();
      }else if(value.type==='stopped') {
        if(!stats||value.session!==stats.meta.session)throw Error('סיום הזרם אינו תואם להפעלה.');
        if(!sink)rawSay('תצוגת האנטנות נעצרה.');
        showStats();void finish(value.reason,value);
      }else if(value.type==='error')throw Error(`הכרטיס דחה את הפעולה: ${value.reason}`);
      update();
    }catch(e){abort('protocol_error',e);}
  }

  function connectSocket(preview=false, directAddress=null) {
    if(socket)return;
    try {
      const endpoint=wifiEndpoint(directAddress??wifi);if(!endpoint)throw Error('הכרטיס עדיין אינו מחובר לרשת.');
      requestedReason='connection_lost';socketReady=false;socketError='';previewAfterHello=preview;
      const ws=new WebSocket(endpoint);socket=ws;ws.binaryType='arraybuffer';
      const timeout=setTimeout(()=>{if(!socketReady&&socket===ws){socketError='לא התקבלה תשובה מהכרטיס. בדקו הרשאת רשת מקומית ושהכרטיס פנוי לחיבור.';ws.close();}},20000);
      ws.onmessage=event=>{if(socket===ws&&ws.readyState===WebSocket.OPEN)message(event,directAddress);};
      ws.onopen=()=>rawSay('חיבור הרשת נפתח. ממתין לכרטיס…');
      ws.onerror=()=>{socketError='חיבור ה־Wi-Fi נכשל. ודאו שהכרטיס והמחשב באותה רשת, ואשרו גישה לרשת המקומית בדפדפן. אם החיבור נחסם, פתחו את הדמו המקומי באמצעות Start-Radar.cmd.';rawSay(socketError,true);};
      ws.onclose=()=>{
        clearTimeout(timeout);
        if(socket!==ws)return;
        socket=null;socketReady=false;previewAfterHello=false;
        if(directConnection){
          directConnection=false;wifi=null;
          $('wifiAddress').textContent='—';say('החיבור הישיר נסגר.');
        }
        if(!sink)rawSay(socketError||'חיבור הנתונים נותק. אפשר להתחבר שוב.',!!socketError);
        void finish(requestedReason);update();
      };
      rawSay('מתחבר לנתונים דרך הרשת…');update();
    }catch(e){previewAfterHello=false;rawSay(e.message+' · אפשר לפתוח את הדמו המקומי באמצעות Start-Radar.cmd.',true);update();}
  }
  function beginStream() {
    stats=null;showStats();requestedReason='connection_lost';
    stage='starting';update();
    socket.send(JSON.stringify({command:'start'}));
    startTimer=setTimeout(()=>abort('start_timeout',Error('הכרטיס לא התחיל לשלוח דגימות.')),10000);
  }
  $('rawConnect').onclick=()=>connectSocket();
  $('directWifiForm').addEventListener('submit',event=>{
    event.preventDefault();
    if(socket||getState().ready||stage!=='idle')return;
    connectSocket(true,{state:'connected',ip:$('directWifiIp').value.trim(),port:8080,path:'/radar'});
  });
  $('liveRxStart').onclick=()=>{
    if(stage!=='idle')return;
    if(!socket){connectSocket(true);return;}
    if(socketReady)try{beginStream();}catch(e){abort('start_failed',e);}
  };
  $('rawDisconnect').onclick=()=>{requestedReason='disconnected';socket?.close();};
  $('rawRecord').onclick=async()=>{
    if(!socketReady||stage!=='idle')return;
    stage='preparing';update();let writer=null;
    savedName=`radar-full-${new Date().toISOString().replace(/[:.]/g,'-')}.radar`;
    try {
      if('showSaveFilePicker'in window) {
        const handle=await window.showSaveFilePicker({suggestedName:savedName,types:[{description:'Radar raw capture',accept:{'application/octet-stream':['.radar']}}]});
        writer=await handle.createWritable();
      }
      if(!socketReady||socket?.readyState!==WebSocket.OPEN){await writer?.close();throw Error('החיבור נותק לפני תחילת ההקלטה.');}
      preparedSettings=directConnection?null:structuredClone(getState().settings);
      sink=new CaptureSink(writer);lastBlob=null;beginStream();
      rawSay('מתחיל הקלטה מלאה…');
    }catch(e){if(sink)abort('start_failed',e);else{await writer?.close().catch(()=>{});stage='idle';if(e.name!=='AbortError')rawSay(e.message,true);}}
    update();
  };
  function stopStream() {
    if(previewAfterHello){previewAfterHello=false;socket?.close();return;}
    if(!socketReady||!['starting','streaming','recording'].includes(stage))return;
    // If start is pending, wait for its metadata before accepting binary frames.
    if(stage==='starting'){abort('cancelled_before_start');return;}
    stage='stopping';socket.send(JSON.stringify({command:'stop'}));
    stopTimer=setTimeout(()=>abort('stop_timeout',Error('לא התקבל אישור לסיום.')),8000);
    rawSay(sink?'מסיים ושומר את ההקלטה…':'עוצר את התצוגה החיה…');update();
  }
  $('rawStop').onclick=stopStream;
  $('liveRxStop').onclick=stopStream;
  $('rawSave').onclick=()=>{
    if(!lastBlob)return;
    const url=URL.createObjectURL(lastBlob),a=document.createElement('a');a.href=url;a.download=savedName;a.click();setTimeout(()=>URL.revokeObjectURL(url),30000);
  };
  setInterval(()=>{if(stats&&['recording','streaming'].includes(stage)){showStats();liveView?.checkFreshness();lab?.checkFreshness();if(sink?.error)abort('disk_error',sink.error);}update();},500);
  window.addEventListener('beforeunload',event=>{if(sink){event.preventDefault();event.returnValue='';}});
  function onBleConnected() {
    const id=getState().device?.id;
    if(boardId!==id) {
      wifi=null;directConnection=false;
      lab?.clear('ממתין לדגימות מהכרטיס.');
      lab?.resetSamples();
      $('wifiAddress').textContent='—';
      if(socket){requestedReason='device_changed';socket.close();}
    }
    boardId=id;
    if(supported())poll=setTimeout(refreshIfIdle,500);
    update();
  }
  function setTabActive(active) {
    const entering=active&&!tabActive;
    tabActive=active;
    if(entering && !socket && wifi?.state==='connected' && stage==='idle')connectSocket(true);
  }
  return {update,onWifi,onBleConnected,setTabActive,onBleDisconnected:()=>{clearTimeout(poll);update();}};
}
