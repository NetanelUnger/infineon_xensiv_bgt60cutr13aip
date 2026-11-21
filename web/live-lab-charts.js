import {heatColor, plotLines} from './raw-charts.js';
import {powerDb} from './raw-analysis.js';
import {rxColors} from './fifo-traces.js';

function surface(canvas, height) {
  const width = Math.max(200, Math.round(canvas.getBoundingClientRect().width));
  const scale = window.devicePixelRatio || 1;
  canvas.width = Math.round(width * scale);
  canvas.height = Math.round(height * scale);
  canvas.style.height = height + 'px';
  const ctx = canvas.getContext('2d');
  ctx.setTransform(scale, 0, 0, scale, 0, 0);
  ctx.font = '12px Arial';
  ctx.direction = 'ltr';
  ctx.fillStyle = '#43594f';
  return {ctx, width};
}

export function drawEchoPath(canvas, peak) {
  const {ctx, width} = surface(canvas, 180);
  const left = 46, right = width - 24;
  const x = peak ? left + 28 + (right - left - 28) * (peak.bin - 1) / 31 : null;
  function arrow(x1,y1,x2,y2,color) {
    const angle = Math.atan2(y2-y1,x2-x1);
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.beginPath(); ctx.moveTo(x1,y1); ctx.lineTo(x2,y2);
    ctx.lineTo(x2-8*Math.cos(angle-.45),y2-8*Math.sin(angle-.45));
    ctx.moveTo(x2,y2); ctx.lineTo(x2-8*Math.cos(angle+.45),y2-8*Math.sin(angle+.45)); ctx.stroke();
  }
  ctx.fillText('TX', 8, 26);
  rxColors.forEach((color, rx) => {
    const y = 66 + 27 * rx;
    ctx.fillStyle = color; ctx.fillText('RX' + (rx+1), 8, y+4);
    if (peak) arrow(x, 44, left, y, color);
  });
  if (peak) {
    arrow(left, 22, x, 43, '#a67b23');
    ctx.fillStyle = '#17221f'; ctx.fillRect(x, 30, 4, 32);
  }
  ctx.fillStyle = '#43594f'; ctx.textAlign = 'center';
  ctx.fillText(peak ? 'תא ' + peak.bin + ' · שיא מעל הסף' : 'אין שיא מעל הסף', width/2, 158);
}

const fifoBitmap = () => Object.assign(document.createElement('canvas'), {width:192, height:17});
export function createFifoMap(canvas) {
  const bitmap = fifoBitmap(), pixels = bitmap.getContext('2d'), image = pixels.createImageData(192,17);
  const values = new Uint16Array(192*17);
  return frame => {
    let low = 65535, high = 0, i = 0;
    for (let chirp=0; chirp<17; chirp++) for (let n=0; n<64; n++) for (let rx=0; rx<3; rx++) {
      const value = frame.adc(chirp,n,rx);
      values[i++] = value;
      low = Math.min(low,value); high = Math.max(high,value);
    }
    values.forEach((v,i) => {
      const t = (v-low)/Math.max(1,high-low), p = i*4;
      image.data[p] = 15 + t*221; image.data[p+1] = 67 + t*179;
      image.data[p+2] = 66 + t*166; image.data[p+3] = 255;
    });
    pixels.putImageData(image,0,0);
    const {ctx,width} = surface(canvas,180);
    const left=30, top=20, w=width-left-16, h=122;
    ctx.imageSmoothingEnabled=false; ctx.drawImage(bitmap,left,top,w,h);
    ctx.fillText('1',8,top+8); ctx.fillText('17',6,top+h);
    ctx.textAlign='center';
    for (const n of [0,16,32,48,63]) ctx.fillText(String(n),left+(n+.5)*w/64,top+h+17);
    ctx.fillText('מספר דגימה בצ׳ירפ',left+w/2,177);
    return [low/16,high/16];
  };
}

export function drawProfile(canvas, profiles, threshold, peak) {
  const series = profiles.map((power,rx) => ({color:rxColors[rx],
    points:Array.from(power, (p,i) => ({x:i+1, y:powerDb(p)}))}));
  series.push({color:'#666', dashed:true, points:[{x:1,y:threshold},{x:32,y:threshold}]});
  plotLines(canvas, series, {ymin:-120,ymax:0,selected:peak?.bin});
}

const palette = Array.from({length: 141}, (_,i) => heatColor(i,0,140));
export function drawRiver(canvas, sample, selectedBin) {
  const {ctx,width} = surface(canvas, 438);
  const x=38, w=width-x-16, h=96, cell=w/sample.rows.length;
  for (let rx=0; rx<3; rx++) {
    const y=26+rx*132;
    ctx.fillStyle='#fff'; ctx.fillRect(x,y,w,h);
    ctx.fillStyle=rxColors[rx]; ctx.fillText('RX'+(rx+1),x,y-7);
    ctx.textAlign='right';
    for (const bin of [1,16,32]) ctx.fillText(String(bin),x-6,y+h-(bin-.5)*h/32+3);
    ctx.textAlign='left';
    sample.rows.forEach((row,i) => {
      if (!row) return;
      for (let bin=0; bin<32; bin++) {
        const db=powerDb(row.power[rx*32+bin]/row.count);
        ctx.fillStyle=palette[Math.max(0,Math.min(140,Math.round((db+90)*2)))];
        ctx.fillRect(x+i*cell,y+(31-bin)*h/32,cell+.15,h/32+.1);
      }
    });
    ctx.strokeStyle='#c9c5ba';ctx.strokeRect(x,y,w,h);
    if (selectedBin) {
      ctx.strokeStyle='#17221f';ctx.setLineDash([5,4]);
      ctx.strokeRect(x,y+(32-selectedBin)*h/32,w,h/32);ctx.setLineDash([]);
    }
  }
  ctx.fillStyle='#43594f';ctx.textAlign='center';
  for (let s=0; s<=3; s++) ctx.fillText(String(s),x+w*s/3,407);
  ctx.fillText('זמן מתחילת הדגימה (שניות)',x+w/2,429);
}
