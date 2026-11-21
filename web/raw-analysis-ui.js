import {selection,compareMetadata} from './raw-analysis.js';
import {COLOR_A,COLOR_B,plotLines,plotHeatmap,emptyChart,profileBinAt} from './raw-charts.js';

const number=n=>n.toLocaleString('he-IL');
const db=(v,unit='dBFS')=>Number.isFinite(v)?`${v.toFixed(1)} ${unit}`:'—';
const signed=v=>`${v>0?'+':''}${v.toFixed(2)} dB`;
const ltr=text=>`\u2066${text}\u2069`;

export function createRawAnalysisUI() {
  const $=id=>document.getElementById(id);
  const slots={A:{},B:{}};
  let view={},compatibility=null;
  for(let c=0;c<17;c++) $('rawAnalysisChirp').add(new Option(`צ׳ירפ ${c+1}`,String(c)));

  function details(key,data) {
    const box=$(`rawDetails${key}`);box.replaceChildren();box.hidden=!data;if(!data)return;
    const name=document.createElement('bdi');name.textContent=data.name;box.append(name);
    const metrics=document.createElement('dl');
    for(const [label,value] of [
      ['מסגרות',number(data.summary.frames)],['צ׳ירפים',number(data.summary.frames*17)],
      ['שניות',(data.summary.elapsed_ms/1000).toFixed(2)],['מסגרות לשנייה',data.summary.rate.toFixed(1)],
      ['נפח · MiB',(data.size/1048576).toFixed(2)],['פער מרבי · ms',number(data.summary.maxGapMs)]
    ]) {
      const group=document.createElement('div'),dt=document.createElement('dt'),dd=document.createElement('dd');
      dt.textContent=label;dd.textContent=value;group.append(dt,dd);metrics.append(group);
    }
    box.append(metrics);
    const quality=document.createElement('p');quality.className=data.quality.lossFree?'raw-quality':'raw-warning';
    quality.textContent=data.quality.lossFree?'הקלטה מלאה · ללא אובדן מדווח':data.quality.warnings.join(' ');box.append(quality);
    const meta=document.createElement('p');meta.textContent=`קושחה ${data.meta.firmware||'לא צוינה'} · ${data.meta.config||'תצורה לא צוינה'}${data.meta.recorded_at?' · '+data.meta.recorded_at:''}`;box.append(meta);
  }
  function stop(key,clearInput=true) {
    slots[key].worker?.terminate();slots[key]={};
    $(`rawProgress${key}`).hidden=true;$(`rawClear${key}`).disabled=true;if(clearInput)$(`rawFile${key}`).value='';
    details(key,null);
  }
  function load(key,file) {
    stop(key,!file);render();
    const status=$(`rawLoad${key}`);status.classList.remove('error');
    if(!file) {status.textContent='ממתין לבחירת קובץ.';return;}
    $(`rawClear${key}`).disabled=false;
    const progress=$(`rawProgress${key}`);progress.value=0;progress.hidden=false;
    status.textContent=`קורא ומנתח את ${file.name}…`;
    function fail(message) {
      stop(key);status.textContent=message;status.classList.add('error');render();
    }
    let worker;
    try {worker=new Worker(new URL('./raw-analysis-worker.js',import.meta.url),{type:'module'});}
    catch {fail('לא ניתן להתחיל ניתוח. פתח את הממשק דרך השרת המקומי ורענן את הדף.');return;}
    slots[key].worker=worker;
    worker.onmessage=({data:message})=>{
      if(slots[key].worker!==worker)return;
      if(message.type==='progress') {progress.value=message.value;status.textContent=`מנתח ${file.name} · ${Math.round(message.value*100)}%`;}
      else if(message.type==='error') fail(message.message);
      else if(message.type==='done') {
        worker.terminate();slots[key]={data:message.data};progress.hidden=true;
        status.textContent='הקובץ נטען. אפשר להחליף אותו בכל עת.';
        details(key,message.data);render();
      }
    };
    worker.onerror=event=>{event.preventDefault();if(slots[key].worker===worker)fail('הניתוח הופסק עקב שגיאה. נסה לטעון את הקובץ שוב.');};
    worker.postMessage(file);
  }

  function render() {
    const a=slots.A.data,b=slots.B.data,rx=Number($('rawAnalysisRx').value),chirp=Number($('rawAnalysisChirp').value),bin=Number($('rawAnalysisBin').value);
    const present=[a,b].filter(Boolean);
    $('rawAnalysisResults').hidden=!present.length;$('rawBinValue').value=bin;
    compatibility=a&&b?compareMetadata(a.meta,b.meta):null;
    const status=$('rawCompareStatus');status.classList.toggle('warning',!!compatibility&&!compatibility.compatible);
    if(!present.length) {status.textContent='טען קובץ אחד לבדיקה, או שני קובצי ‎.radar להשוואה.';view={};return;}
    if(!a||!b) status.textContent=`קובץ ${a?'A':'B'} מוצג. טען את הקובץ השני כדי לחשב את ההפרש.`;
    else if(!compatibility.compatible) status.textContent=`ההגדרות אינן תואמות: ${compatibility.reasons.join('; ')}. אפשר לעיין בגרפים, אך חישוב ההפרש אינו זמין.`;
    else status.textContent='הגדרות האיסוף תואמות. ההפרש הוא B פחות A, על פני כל משך כל קובץ; משכי ההקלטות יכולים להיות שונים.';
    view={A:a?selection(a,rx,chirp):null,B:b?selection(b,rx,chirp):null};
    const delta=compatibility?.compatible?view.B.db.map((v,k)=>v-view.A.db[k]):null;
    $('rawValueA').textContent=db(view.A?.db[bin]);$('rawValueB').textContent=db(view.B?.db[bin]);
    $('rawValueDelta').textContent=delta?signed(delta[bin]):'—';
    const profiles=Object.entries(view).filter(([,v])=>v).map(([key,v])=>({color:key==='A'?COLOR_A:COLOR_B,dashed:key==='B',points:v.db.slice(1).map((y,i)=>({x:i+1,y}))}));
    const allDb=profiles.flatMap(s=>s.points.map(p=>p.y));
    let min=Math.floor(Math.min(...allDb)/10)*10,max=Math.ceil(Math.max(...allDb)/10)*10;
    if(max-min<20)min=max-20;
    plotLines($('rawProfileChart'),profiles,{ymin:min,ymax:max,selected:bin});
    if(delta) {
      const extent=Math.max(5,Math.ceil(Math.max(...delta.slice(1).map(Math.abs))/5)*5);
      plotLines($('rawDeltaChart'),[{color:COLOR_B,points:delta.slice(1).map((y,i)=>({x:i+1,y}))}],{ymin:-extent,ymax:extent,ylabel:'Δ dB · B − A',selected:bin,bars:true});
    } else emptyChart($('rawDeltaChart'),a&&b?'לא ניתן להשוות הגדרות שונות':'טען שתי הקלטות לחישוב ההפרש');
    $('rawDeltaCaption').textContent=delta?`בתא ${bin}: ${ltr(signed(delta[bin]))}. ממוצע הספק בכל קובץ, לפני חישוב ההפרש.`:'נדרשים שני קבצים עם אותן הגדרות איסוף.';
    const nominal=data=>data.summary.rate?1000/data.summary.rate:50;
    const durationMs=Math.max(50,...present.map(d=>d.summary.elapsed_ms+nominal(d)));
    const histories=Object.entries(view).filter(([,v])=>v).map(([key,v])=>({color:key==='A'?COLOR_A:COLOR_B,dashed:key==='B',points:v.timeline.map((row,i)=>({x:row.ms/1000,y:row.db[bin],break:row.gap||(i>0&&row.firstMs-v.timeline[i-1].lastMs>nominal(slots[key].data)*3)}))}));
    let low=Infinity,high=-Infinity;
    for(const v of Object.values(view)) if(v) for(const row of v.timeline) for(const value of row.db.slice(1)) {low=Math.min(low,value);high=Math.max(high,value);}
    low=Math.floor(low/10)*10;high=Math.ceil(high/10)*10;if(high-low<20)low=high-20;
    let historyMin=Infinity,historyMax=-Infinity;
    for(const s of histories)for(const p of s.points){historyMin=Math.min(historyMin,p.y);historyMax=Math.max(historyMax,p.y);}
    const historySpan=historyMax-historyMin,padding=Math.max(.25,historySpan*.1),step=historySpan<5?.5:historySpan<20?2:10;
    historyMin=Math.floor((historyMin-padding)/step)*step;historyMax=Math.ceil((historyMax+padding)/step)*step;
    plotLines($('rawHistoryChart'),histories,{xmin:0,xmax:durationMs/1000,ymin:historyMin,ymax:historyMax,xlabel:'זמן · s'});
    $('rawHistoryTitle').textContent=`תא ${bin} · השינוי לאורך זמן`;
    for(const key of ['A','B']) plotHeatmap($(`rawHeat${key}`),view[key]?.timeline,{durationMs,nominalMs:slots[key].data?nominal(slots[key].data):50,min:low,max:high,bin});
    $('rawColorScale').textContent=`${low} … ${high} dBFS`;
    const resolution=present.map(d=>`${d===a?'A':'B'}: ${d.timeline.length} מקטעי תצוגה, עד ${number(Math.max(...d.timeline.map(r=>r.frames)))} מסגרות למקטע`);
    const clipping=Object.entries(view).filter(([,v])=>v).map(([key,v])=>`${key}: ${number(v.clipped)} דגימות סמוכות לרוויה`);
    $('rawResolution').textContent=`${resolution.join(' · ')}. כל המסגרות נכללות בממוצע. ${clipping.join(' · ')} באנטנה ובצ׳ירפ שנבחרו. הזמנים הם זמני קריאת FIFO בתוכנה.`;
    const body=$('rawCompareTable');body.replaceChildren();
    for(let k=1;k<=32;k++) {
      const tr=document.createElement('tr');if(k===bin)tr.className='selected';
      for(const value of [String(k),view.A?.db[k]?.toFixed(2)??'—',view.B?.db[k]?.toFixed(2)??'—',delta?delta[k].toFixed(2):'—']) {const td=document.createElement('td');td.textContent=value;tr.append(td);}
      body.append(tr);
    }
    readout(bin);
  }
  function readout(bin) {
    const parts=[`תא ${bin}`];
    for(const key of ['A','B'])if(view[key])parts.push(ltr(`${key}: ${db(view[key].db[bin])}`));
    if(compatibility?.compatible)parts.push(`הפרש: ${ltr(signed(view.B.db[bin]-view.A.db[bin]))}`);
    $('rawProfileReadout').textContent=parts.join(' · ');
  }
  for(const key of ['A','B']) {
    $(`rawFile${key}`).onchange=e=>{const file=e.target.files[0];if(file)load(key,file);};
    $(`rawClear${key}`).onclick=()=>load(key,null);
  }
  $('rawAnalysisRx').onchange=render;$('rawAnalysisChirp').onchange=render;$('rawAnalysisBin').oninput=render;
  $('rawProfileChart').onclick=e=>{$('rawAnalysisBin').value=profileBinAt(e.currentTarget,e.clientX);render();};
  $('rawProfileChart').onpointermove=e=>readout(profileBinAt(e.currentTarget,e.clientX));
  $('rawProfileChart').onpointerleave=()=>readout(Number($('rawAnalysisBin').value));
  $('rawCompareCsv').onclick=()=>{
    const lines=['range_bin,rx,chirp_1_based_or_all,power_a_dbfs,power_b_dbfs,delta_b_minus_a_db'];
    for(let k=1;k<=32;k++) lines.push([k,Number($('rawAnalysisRx').value)+1,Number($('rawAnalysisChirp').value)<0?'all':Number($('rawAnalysisChirp').value)+1,view.A?.db[k]?.toFixed(6)??'',view.B?.db[k]?.toFixed(6)??'',compatibility?.compatible?(view.B.db[k]-view.A.db[k]).toFixed(6):''].join(','));
    const url=URL.createObjectURL(new Blob([lines.join('\n')],{type:'text/csv;charset=utf-8'})),link=document.createElement('a');
    link.href=url;link.download='radar-raw-comparison.csv';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
  };
  let previousWidth=0,resizePending;
  const resize=new ResizeObserver(entries=>{
    const width=Math.round(entries[0].contentRect.width);
    if(width!==previousWidth){previousWidth=width;cancelAnimationFrame(resizePending);resizePending=requestAnimationFrame(render);}
  });
  resize.observe($('rawAnalysis'));
  render();
}
