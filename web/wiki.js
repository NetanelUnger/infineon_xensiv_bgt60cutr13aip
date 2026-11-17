import {analyze,spectrum,validateCapture} from './research.js';

const $=id=>document.getElementById(id);
const articles=[...document.querySelectorAll('.wiki-article')];
const links=[...document.querySelectorAll('.contents nav a')];
const normalize=s=>s.normalize('NFKC').toLocaleLowerCase().replace(/[\u0591-\u05C7]/g,'').trim();
function search(){
  const terms=normalize($('wikiSearch').value).split(/\s+/).filter(Boolean);
  let count=0;
  for(const article of articles){
    const haystack=normalize(article.textContent+' '+(article.dataset.keywords||''));
    const match=terms.every(term=>haystack.includes(term));
    article.hidden=!match;
    const link=links.find(a=>a.hash==='#'+article.id);if(link)link.hidden=!match;
    if(match)count++;
  }
  $('searchStatus').textContent=terms.length?`${count} פרקים מתאימים מתוך ${articles.length}`:'';
  $('noResults').hidden=count!==0;
}
for(const event of ['input','change','search'])$('wikiSearch').addEventListener(event,search);
function revealHash(){
  const id=decodeURIComponent(location.hash.slice(1)),target=document.getElementById(id);
  if(!target)return;
  if(target.hidden||target.closest('.wiki-article')?.hidden){$('wikiSearch').value='';search();target.scrollIntoView();}
  const sectionId=target.closest('.wiki-article')?.id||id;
  links.forEach(a=>{if(a.hash==='#'+sectionId)a.setAttribute('aria-current','location');else a.removeAttribute('aria-current');});
}
window.addEventListener('hashchange',revealHash);revealHash();
// When this guide was opened by the dashboard, return to that window without
// navigating it or creating another BLE client. Direct visits retain a real link.
$('dashboardLink').addEventListener('click',e=>{
  try{if(window.opener&&!window.opener.closed&&window.opener.location.origin===location.origin){e.preventDefault();window.opener.focus();}}
  catch{/* A direct link remains available when the opener is inaccessible. */}
});

function plot(id,values,{min,max,bars=false,selected=-1,xLabel,yLabel,start=0}){
  const c=$(id),ctx=c.getContext('2d'),W=c.width,H=c.height,left=57,right=18,top=27,bottom=48;
  const width=W-left-right,height=H-top-bottom;
  ctx.clearRect(0,0,W,H);ctx.font='14px Arial';ctx.textAlign='right';
  for(let j=0;j<=4;j++){
    const y=top+height*j/4,v=max-(max-min)*j/4;
    ctx.strokeStyle='#dedbd0';ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(W-right,y);ctx.stroke();
    ctx.fillStyle='#52645c';ctx.fillText(v.toFixed(0),left-8,y+5);
  }
  const step=width/values.length;ctx.beginPath();ctx.lineWidth=2;ctx.strokeStyle='#0d665b';
  values.forEach((v,i)=>{
    const x=left+(i+.5)*step,y=top+(max-Math.min(max,Math.max(min,v)))/(max-min)*height;
    if(bars){ctx.fillStyle=i===selected?'#e54b2f':'#0d665b';ctx.fillRect(x-step*.33,y,step*.66,H-bottom-y);}
    else if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  });if(!bars)ctx.stroke();
  ctx.fillStyle='#52645c';ctx.textAlign='center';
  for(let i=0;i<values.length;i+=Math.max(1,Math.floor(values.length/8)))ctx.fillText(String(i+start),left+(i+.5)*step,H-bottom+21);
  ctx.fillText(xLabel,left+width/2,H-6);ctx.textAlign='left';ctx.fillText(yLabel,8,16);
}
let capture,analysisByRx;
function renderSample(){
  if(!capture)return;
  const frame=Math.min(capture.rows.length-1,Math.max(0,Number($('labFrame').value)||0));
  const rx=Number($('labRx').value),bin=Math.max(1,Math.min(32,Math.round(Number($('labBin').value)||1)));
  $('labBin').value=bin;
  const row=capture.rows[frame],values=Array.from({length:64},(_,i)=>row.adc[i*3+rx]/16);
  const profile=spectrum(row.adc,rx).map(x=>x.db);
  $('frameInfo').textContent=`${frame+1} / ${capture.rows.length} · זמן ${(row.ms/1000).toFixed(3)} שניות`;
  plot('adcPreview',values,{min:0,max:4096,xLabel:'sample index',yLabel:'ADC / 16'});
  plot('fftPreview',profile.slice(1),{min:-100,max:0,bars:true,selected:bin-1,start:1,xLabel:'range bin',yLabel:'dBFS'});
  $('labReading').textContent=`RX${rx+1}, תא ${bin}: ${profile[bin].toFixed(1)} dBFS במסגרת הזו; ממוצע כל ההקלטה ${analysisByRx[rx].mean[bin].toFixed(1)} dBFS. העמודה הכתומה היא התא שבחרת.`;
  $('sampleJson').textContent=JSON.stringify({meta:{encoding:capture.meta.encoding,order:capture.meta.order,samples:64,rx:3},frame_index:frame,ms:row.ms,dropped:row.dropped,adc_preview:row.adc.slice(0,12),adc_total_values:row.adc.length},null,2);
}
for(const id of ['labRx','labFrame','labBin'])$(id).addEventListener('input',renderSample);
try{
  const response=await fetch('wiki-capture.json');if(!response.ok)throw Error('לא ניתן לטעון את הקובץ');
  capture=await response.json();validateCapture(capture.meta,capture.rows);
  analysisByRx=[0,1,2].map(rx=>analyze(capture,rx));
  $('labFrame').max=capture.rows.length-1;
  $('labStatus').textContent=`${capture.rows.length} מסגרות · ${(capture.meta.duration_ms/1000)} שניות · ${analysisByRx[0].rate.toFixed(1)} Hz בפועל · מקור: הקלטת בדיקה מ־17.11.2026`;
  renderSample();
}catch(e){
  $('labStatus').textContent=`דוגמת הנתונים אינה זמינה: ${e.message}. פתח את המדריך דרך Start-Radar.cmd; הטקסט עדיין זמין לקריאה.`;
  for(const id of ['labRx','labFrame','labBin'])$(id).disabled=true;
}
