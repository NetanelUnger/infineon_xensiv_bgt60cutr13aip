/* Experimental SEQ0 recording analysis. No conversion to temperature or material.
 * Physical distance is deliberately not inferred from an unverified chirp slope.
 * Plot bins until a range calibration for the acquisition config is established. */
export function validateCapture(meta, rows) {
  if(meta?.protocol!==1||meta.samples!==64||meta.rx!==3||meta.encoding!=='u12.4'||meta.order!=='sample_rx')throw Error('פורמט הקלטה לא נתמך');
  if(!Number.isInteger(meta.id)||meta.id<1||![2000,5000,10000].includes(meta.duration_ms)||!Array.isArray(rows)||!Number.isInteger(meta.count)||meta.count<1||meta.count>256||rows.length!==meta.count)throw Error('ההקלטה חסרה או ריקה');
  let previous=-1;
  for(const row of rows){
    if(!Number.isInteger(row.ms)||row.ms<0||row.ms>=meta.duration_ms||row.ms<=previous)throw Error('סדר הזמנים בהקלטה אינו תקין');
    if(!Number.isInteger(row.dropped)||row.dropped<0||row.adc?.length!==192||!row.adc.every(n=>Number.isInteger(n)&&n>=0&&n<=65535))throw Error('נתוני ADC לא תקינים');
    previous=row.ms;
  }
}
export function acceptPage(meta, rows, page){
  if(page?.id!==meta.id||page.offset!==rows.length||!Array.isArray(page.rows)||page.rows.length<1||page.rows.length>2||rows.length+page.rows.length>meta.count)throw Error('מקטע הקלטה חסר או שייך לניסוי אחר');
  rows.push(...page.rows);
}
export function spectrum(adc,rx){
  if(adc.length!==192||![0,1,2].includes(rx))throw Error('Invalid ADC shape/channel');
  // Unsigned U12.4 normalized to full ADC range. DC subtraction is only along
  // fast time within this chirp; stationary range-bin echoes are preserved.
  const x=Array.from({length:64},(_,i)=>adc[i*3+rx]/65536);
  const mean=x.reduce((a,b)=>a+b,0)/64;
  const win=x.map((_,i)=>.5-.5*Math.cos(2*Math.PI*i/63)),gain=win.reduce((a,b)=>a+b,0);
  return Array.from({length:33},(_,k)=>{
    let re=0,im=0;
    for(let i=0;i<64;i++){const a=(x[i]-mean)*win[i],angle=2*Math.PI*k*i/64;re+=a*Math.cos(angle);im-=a*Math.sin(angle);}
    // 0 dBFS is a peak amplitude of half the unsigned ADC span.
    const amplitude=Math.hypot(re,im)/gain*(k===0||k===32?2:4);
    return {db:20*Math.log10(Math.max(1e-6,amplitude)),phase:Math.atan2(im,re)};
  });
}
export function histogram(values,edges){
  const counts=Array(edges.length-1).fill(0);
  for(const v of values){if(!Number.isFinite(v))continue;const i=edges.findIndex((a,i)=>i<edges.length-1&&v>=a&&(v<edges[i+1]||i===edges.length-2&&v===edges[i+1]));if(i>=0)counts[i]++;}
  return counts;
}
export function analyze(capture,rx=0){
  validateCapture(capture.meta,capture.rows);
  const profiles=capture.rows.map(r=>spectrum(r.adc,rx).map(x=>x.db));
  const mean=Array.from({length:33},(_,k)=>10*Math.log10(profiles.reduce((s,r)=>s+10**(r[k]/10),0)/profiles.length));
  const rate=capture.rows.length>1?(capture.rows.length-1)*1000/(capture.rows.at(-1).ms-capture.rows[0].ms):0;
  const gaps=capture.rows.slice(1).map((r,i)=>r.ms-capture.rows[i].ms);
  return {profiles,mean,rate,maxGap:gaps.length?Math.max(...gaps):0,clipped:capture.rows.reduce((s,r)=>s+r.adc.filter((v,i)=>i%3===rx&&(v<=16||v>=65519)).length,0)};
}
export function createResearchUI({command,getState,updateControls}){
  const $=id=>document.getElementById(id);let recording=null,analysis=null,baseline=null,running=false;
  const say=(text,error=false)=>{$('researchStatus').textContent=text;$('researchStatus').classList.toggle('error',error);};
  function supported(){const v=(getState().info?.version||'0.0').split('.').map(Number);return v[0]>0||v[1]>=2;}
  function update(){const s=getState();for(const id of ['captureStart','captureRecover'])$(id).disabled=!s.ready||s.busy||!supported();$('captureDuration').disabled=running;$('captureExport').disabled=!recording;$('captureCsv').disabled=!recording;$('captureBaseline').disabled=!recording;$('researchEmpty').hidden=!!recording;}
  async function status(){const r=await command(21);const m=r.find(x=>x.research)?.research;if(!m)throw Error('לא התקבל מצב ההקלטה');return m;}
  async function download(meta){
    if(meta.active)throw Error('הכרטיס עדיין מקליט');if(!meta.count)throw Error(`אין מדידות בהקלטה (${meta.reason})`);
    const rows=[];
    while(rows.length<meta.count){say(`מוריד ב־BLE: ${rows.length} מתוך ${meta.count} מדידות…`);const r=await command(22,{id:meta.id,offset:rows.length});acceptPage(meta,rows,r.find(x=>x.research_page)?.research_page);}
    validateCapture(meta,rows);
    recording={format:'radar-research-v1',downloadedAt:new Date().toISOString(),firmware:getState().info?.version,meta,rows};
    render();say(`ההקלטה הורדה: ${rows.length} מדידות · ${analysis.rate.toFixed(1)} Hz בפועל · סיום: ${meta.reason}.`);
  }
  async function run(start){
    running=true;getState().busy=true;updateControls();
    try{
      let meta;
      if(start){const r=await command(20,{duration_ms:Number($('captureDuration').value)});meta=r.find(x=>x.research)?.research;if(!meta)throw Error('לא התקבל אישור התחלת הקלטה');}
      else meta=await status();
      const id=meta.id,deadline=Date.now()+15000;
      while(meta.active){if(Date.now()>deadline)throw Error('ההקלטה לא הסתיימה; אפשר להתחבר שוב ולהוריד את ההקלטה האחרונה');say(`מקליט על הכרטיס… ${meta.count} מדידות. משך מבוקש ${meta.duration_ms/1000} שניות.`);await new Promise(r=>setTimeout(r,600));meta=await status();if(meta.id!==id)throw Error('ההקלטה הוחלפה מממשק אחר');}
      await download(meta);
    }catch(e){say(e.message,true);}finally{running=false;getState().busy=false;updateControls();update();}
  }
  function chart(id,values,{min=-100,max=0,bars=false,labels=null,color='#087f8c'}={}){
    const canvas=$(id),ctx=canvas.getContext('2d'),W=canvas.width,H=canvas.height,p=45;ctx.clearRect(0,0,W,H);ctx.fillStyle='#53666a';ctx.font='12px sans-serif';ctx.textAlign='right';
    for(let j=0;j<=4;j++){const y=p+(H-2*p)*j/4;ctx.strokeStyle='#dce7e5';ctx.beginPath();ctx.moveTo(p,y);ctx.lineTo(W-15,y);ctx.stroke();ctx.fillText((max-(max-min)*j/4).toFixed(0),p-6,y+4);}
    const step=(W-p-20)/values.length;ctx.strokeStyle=color;ctx.fillStyle=color;ctx.beginPath();
    values.forEach((v,i)=>{const x=p+(i+.5)*step,y=H-p-(Math.max(min,Math.min(max,v))-min)/(max-min)*(H-2*p);if(bars)ctx.fillRect(x-step*.35,y,step*.7,H-p-y);else if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);});if(!bars)ctx.stroke();
    ctx.fillStyle='#53666a';ctx.textAlign='center';for(let i=0;i<values.length;i+=Math.max(1,Math.ceil(values.length/8)))ctx.fillText(labels?labels[i]:String(i+1),p+(i+.5)*step,H-20);
  }
  function render(){
    if(!recording)return;const rx=Number($('researchRx').value),bin=Math.max(1,Math.min(32,Math.round(Number($('researchBin').value)||1)));$('researchBin').value=bin;analysis=analyze(recording,rx);
    const b=baseline&&baseline.meta.config===recording.meta.config?analyze(baseline,rx):null;
    chart('rangeProfile',analysis.mean.slice(1),{bars:true});
    const values=analysis.profiles.map(r=>r[bin]);chart('echoHistory',values,{labels:recording.rows.map(r=>(r.ms/1000).toFixed(1))});
    const edges=Array.from({length:15},(_,i)=>-120+i*10),counts=histogram(values,edges);chart('echoHistogram',counts,{min:0,max:Math.max(1,...counts),bars:true,labels:edges.slice(0,-1).map(v=>`${v}`)});
    const c=$('rangeHistory'),ctx=c.getContext('2d');ctx.clearRect(0,0,c.width,c.height);
    // Absolute time positions retain visible gaps instead of compressing them.
    const total=recording.meta.duration_ms,cellW=(c.width-60)/32,nominal=analysis.rate?1000/analysis.rate:50;
    analysis.profiles.forEach((row,i)=>{const t=recording.rows[i].ms,y=20+(c.height-50)*t/total,h=Math.max(1,(c.height-50)*nominal/total);for(let k=1;k<=32;k++){const v=b?row[k]-b.mean[k]:row[k];ctx.fillStyle=b?`hsl(${v>=0?12:205} 80% ${96-Math.min(1,Math.abs(v)/20)*57}%)`:`hsl(${220-Math.max(0,Math.min(1,(v+90)/70))*210} 75% 48%)`;ctx.fillRect(45+(k-1)*cellW,y,cellW+.3,h);}});
    ctx.fillStyle='#53666a';ctx.font='12px sans-serif';ctx.fillText('0 s',3,28);ctx.fillText(`${total/1000} s`,3,c.height-30);for(let k=1;k<=32;k+=4)ctx.fillText(String(k),45+(k-1)*cellW,c.height-8);
    $('heatmapCaption').textContent=b?'שינוי לעומת הקלטת הבסיס: אדום התחזקות, כחול היחלשות; רוויה בצבע ב־±20 dB.':'עוצמה לפי תא מרחק וזמן: כחול חלש (−90 dBFS), אדום חזק (−20 dBFS).';
    $('captureDetails').textContent=`${recording.rows.length} מדידות · קצב ממוצע ${analysis.rate.toFixed(1)} Hz · פער מרבי ${analysis.maxGap} ms · מסגרות שדווחו כחסרות: ${recording.meta.dropped} · דגימות סמוכות לרוויה: ${analysis.clipped}`;
    $('baselineName').textContent=baseline?`בסיס: הקלטה ${baseline.meta.id}, ${baseline.downloadedAt}`:'טרם נבחרה הקלטת בסיס';update();
  }
  function save(name,text,type){const url=URL.createObjectURL(new Blob([text],{type})),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
  $('captureStart').onclick=()=>run(true);$('captureRecover').onclick=()=>run(false);
  $('researchRx').onchange=render;$('researchBin').onchange=render;
  $('captureBaseline').onclick=()=>{baseline=structuredClone(recording);render();say('ההקלטה נשמרה כבסיס להשוואה עם ההקלטה הבאה.');};
  $('captureExport').onclick=()=>save(`radar-research-${recording.meta.id}.json`,JSON.stringify(recording),'application/json');
  $('captureCsv').onclick=()=>{const lines=['time_ms,rx,range_bin,power_dbfs'];for(let rx=0;rx<3;rx++){const a=analyze(recording,rx);a.profiles.forEach((p,i)=>p.slice(1).forEach((v,k)=>lines.push(`${recording.rows[i].ms},${rx+1},${k+1},${v.toFixed(4)}`)));}save(`radar-research-${recording.meta.id}.csv`,lines.join('\n'),'text/csv');};
  $('captureImport').onchange=async e=>{try{const file=e.target.files[0];if(!file)return;if(file.size>1500000)throw Error('קובץ גדול מדי');const c=JSON.parse(await file.text());if(c.format!=='radar-research-v1')throw Error('זה אינו קובץ הקלטת רדאר');validateCapture(c.meta,c.rows);recording=c;render();say('הקלטה נטענה מקובץ; זו אינה תצוגה חיה.');}catch(e){say(e.message,true);}finally{e.target.value='';}};
  return {update};
}
