// Record the real device using Node's WebSocket and the browser's file/decoder code.
import {open,writeFile} from 'node:fs/promises';
import {CaptureSink,RawStats,fileRecord,wifiEndpoint} from '../web/raw-protocol.js';

const [ip,secondsText='20',filename='logs/wifi-hardware.radar']=process.argv.slice(2);
const seconds=Number(secondsText);
if(!ip||!Number.isFinite(seconds)||seconds<1||seconds>3600)throw Error('Usage: node scripts/probe_websocket.mjs IP [seconds] [output.radar]');
const endpoint=wifiEndpoint({state:'connected',ip,port:8080,path:'/radar'});
const file=await open(filename,'w');
const sink=new CaptureSink({async write(bytes){let offset=0;while(offset<bytes.length){const {bytesWritten}=await file.write(bytes,offset,bytes.length-offset);if(!bytesWritten)throw Error('No progress writing capture');offset+=bytesWritten;}},async close(){await file.close();}});
const ws=new WebSocket(endpoint);ws.binaryType='arraybuffer';
let stats=null,finished=false,stopTimer,watchdog;
let first=null,last=null,min=65535,max=0;
const result=await new Promise((resolve,reject)=>{
  async function finish(error,server=null){
    if(finished)return;finished=true;clearTimeout(watchdog);clearTimeout(stopTimer);
    const complete=!error&&!!stats&&server?.session===stats.meta.session&&server.sent===stats.frames&&server.offered===server.sent+server.queue_dropped;
    const lossFree=complete&&!stats.gaps&&!server.queue_dropped&&!server.fifo_errors;
    try{
      await sink.finish({type:'footer',complete,loss_free:lossFree,reason:error?'connection_error':'requested',server,client:stats?.summary()??null});
      const summary={complete,loss_free:lossFree,firmware:stats?.meta.firmware,client:stats?.summary(),server,first_adc:first,last_adc:last,min_adc:min,max_adc:max,file:filename};
      await writeFile(`${filename}.json`,JSON.stringify(summary,null,2));
      ws.close();if(error)reject(error);else if(!complete)reject(Error('Final counters do not match'));else resolve(summary);
    }catch(e){ws.close();reject(e);}
  }
  watchdog=setTimeout(()=>void finish(Error('Recording timeout')), (seconds+25)*1000);
  ws.onmessage=event=>{
    try{
      if(event.data instanceof ArrayBuffer){
        if(!stats)throw Error('Binary before metadata');
        stats.validate(event.data);sink.append(fileRecord(event.data));const f=stats.push(event.data);
        first??=f.adc(0,0,0);last=f.adc(16,63,2);
        for(let c=0;c<17;c++)for(let s=0;s<64;s++)for(let r=0;r<3;r++){const x=f.adc(c,s,r);min=Math.min(min,x);max=Math.max(max,x);}
      }else{
        const value=JSON.parse(event.data);
        if(value.type==='hello')ws.send(JSON.stringify({command:'start'}));
        else if(value.type==='started'){
          stats=new RawStats(value);sink.append(fileRecord(value));
          stopTimer=setTimeout(()=>ws.send(JSON.stringify({command:'stop'})),seconds*1000);
        }else if(value.type==='stopped')void finish(null,value);
        else if(value.type==='error')void finish(Error(value.reason));
      }
    }catch(e){void finish(e);}
  };
  ws.onerror=()=>void finish(Error('WebSocket connection failed'));
  ws.onclose=()=>{if(!finished)void finish(Error('WebSocket closed before stop'));};
});
console.log(JSON.stringify(result,null,2));
