export const COLOR_A='#087f8c', COLOR_B='#ba4a24';
const LEFT=50, RIGHT=18, TOP=26, BOTTOM=42;
const stops=[[220,238,242],[80,149,180],[87,68,153],[188,69,98],[247,180,74]];
export function heatColor(value,min,max) {
  const v=Math.max(0,Math.min(1,(value-min)/(max-min)))*(stops.length-1), i=Math.min(stops.length-2,Math.floor(v)),f=v-i;
  return `rgb(${stops[i].map((x,j)=>Math.round(x+(stops[i+1][j]-x)*f)).join(',')})`;
}
function surface(canvas) {
  const w=Math.max(240,Math.round(canvas.getBoundingClientRect().width));
  const h=canvas.id.startsWith('rawHeat')?320:260,scale=globalThis.devicePixelRatio||1;
  canvas.width=Math.round(w*scale);canvas.height=Math.round(h*scale);canvas.style.height=`${h}px`;
  const ctx=canvas.getContext('2d');ctx.setTransform(scale,0,0,scale,0,0);
  ctx.clearRect(0,0,w,h);ctx.font='12px Arial';ctx.lineWidth=1;ctx.setLineDash([]);ctx.direction='ltr';
  return {ctx,w,h,x0:LEFT,y0:TOP,pw:w-LEFT-RIGHT,ph:h-TOP-BOTTOM};
}
function label(ctx,text,x,y,align='center') {ctx.fillStyle='#52645c';ctx.textAlign=align;ctx.fillText(text,x,y);}
export function emptyChart(canvas,message) {
  const {ctx,w,h}=surface(canvas);ctx.direction='rtl';label(ctx,message,w/2,h/2);
}
export function plotLines(canvas,series,{xmin=1,xmax=32,ymin=-100,ymax=0,xlabel='תא',ylabel='dBFS',selected=null,bars=false}={}) {
  const {ctx,w,h,x0,y0,pw,ph}=surface(canvas);
  const x=v=>x0+(v-xmin)/(xmax-xmin)*pw,y=v=>y0+(ymax-v)/(ymax-ymin)*ph;
  for(let j=0;j<=4;j++) {
    const v=ymin+(ymax-ymin)*j/4,py=y(v);
    ctx.strokeStyle='#deded3';ctx.beginPath();ctx.moveTo(x0,py);ctx.lineTo(w-RIGHT,py);ctx.stroke();
    label(ctx,Math.abs(v-Math.round(v))<.001?v.toFixed(0):v.toFixed(1),x0-9,py+5,'right');
  }
  for(let i=0;i<=5;i++) {
    const raw=xmin+(xmax-xmin)*i/5,v=xlabel==='תא'?Math.round(raw):raw;
    label(ctx,xlabel==='תא'?Math.round(v).toString():v.toFixed(xmax<10?1:0),x(v),h-BOTTOM+23);
  }
  label(ctx,ylabel,x0,18,'left');label(ctx,xlabel,x0+pw/2,h-5);
  if(ymin<0&&ymax>0) {ctx.strokeStyle='#78877e';ctx.beginPath();ctx.moveTo(x0,y(0));ctx.lineTo(w-RIGHT,y(0));ctx.stroke();}
  if(selected!==null) {ctx.fillStyle='#0d665b12';ctx.fillRect(x(selected)-pw/62,y0,pw/31,ph);}
  ctx.save();ctx.beginPath();ctx.rect(x0-4,y0-4,pw+8,ph+8);ctx.clip();
  for(const {points,color,dashed=false} of series) {
    ctx.strokeStyle=color;ctx.fillStyle=color;ctx.lineWidth=2.5;ctx.setLineDash(dashed?[8,5]:[]);ctx.beginPath();
    let drawing=false;
    for(const point of points) {
      if(!Number.isFinite(point.y)) {drawing=false;continue;}
      const px=x(point.x),py=y(point.y);
      if(bars) {ctx.fillStyle=point.y>=0?COLOR_B:COLOR_A;ctx.fillRect(px-pw/90,Math.min(y(0),py),pw/45,Math.max(1,Math.abs(y(0)-py)));}
      else {
        if(!drawing||point.break) ctx.moveTo(px,py);else ctx.lineTo(px,py);
        drawing=true;
      }
    }
    if(!bars)ctx.stroke();
    ctx.setLineDash([]);
    // Single-frame captures and isolated points must also be visible.
    if(!bars && points.length<80) for(const p of points) if(Number.isFinite(p.y)) {ctx.beginPath();ctx.arc(x(p.x),y(p.y),2.5,0,Math.PI*2);ctx.fill();}
  }
  ctx.restore();ctx.setLineDash([]);
}

export function plotHeatmap(canvas,rows,{durationMs,nominalMs,min,max,bin}) {
  if(!rows?.length) {emptyChart(canvas,'ממתין לטעינת קובץ');return;}
  const {ctx,w,h,x0,y0,pw,ph}=surface(canvas),cell=pw/32;
  ctx.fillStyle='#fff';ctx.fillRect(x0,y0,pw,ph);
  for(let i=0;i<rows.length;i++) {
    const row=rows[i],start=row.firstMs,end=Math.min(durationMs,row.lastMs+nominalMs,rows[i+1]?.firstMs??Infinity);
    const top=y0+start/durationMs*ph,height=Math.max(.7,(end-start)/durationMs*ph);
    for(let k=1;k<=32;k++) {ctx.fillStyle=heatColor(row.db[k],min,max);ctx.fillRect(x0+(k-1)*cell,top,cell+.3,height);}
    if(row.gap) {ctx.fillStyle='#493321';ctx.fillRect(x0,top,pw,Math.max(1,Math.min(3,height)));}
  }
  ctx.strokeStyle='#17221f88';ctx.lineWidth=1;ctx.strokeRect(x0+(bin-1)*cell,y0,cell,ph);
  for(let i=0;i<=4;i++) {const t=durationMs*i/4;label(ctx,(t/1000).toFixed(1),x0-9,y0+ph*i/4+5,'right');}
  for(const k of [1,8,16,24,32]) label(ctx,String(k),x0+(k-.5)*cell,h-BOTTOM+23);
  label(ctx,'s',x0-12,18,'right');label(ctx,'תא',x0+pw/2,h-5);
}

export function profileBinAt(canvas,clientX) {
  const rect=canvas.getBoundingClientRect();
  return Math.max(1,Math.min(32,Math.round(1+(clientX-rect.left-LEFT)/(rect.width-LEFT-RIGHT)*31)));
}
