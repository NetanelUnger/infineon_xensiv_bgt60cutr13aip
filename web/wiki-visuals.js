// Synthetic teaching signal only. Never accesses BLE or modifies radar configuration.
export function teachingSpectrum(frequency=12,amplitude=.6,hann=true){
 const samples=Array.from({length:64},(_,n)=>Math.sin(2*Math.PI*5*n/64)+amplitude*Math.sin(2*Math.PI*frequency*n/64));
 const weights=samples.map((_,n)=>hann?.5-.5*Math.cos(2*Math.PI*n/63):1);
 const gain=weights.reduce((a,b)=>a+b,0);
 const bins=Array.from({length:33},(_,k)=>{let re=0,im=0;for(let n=0;n<64;n++){const v=samples[n]*weights[n];re+=v*Math.cos(2*Math.PI*k*n/64);im-=v*Math.sin(2*Math.PI*k*n/64);}return Math.hypot(re,im)/gain*(k===0||k===32?1:2);});
 return {samples,bins};
}
function paint(canvas,values,bars,max,label){
 const ctx=canvas.getContext('2d'),w=canvas.width,h=canvas.height,L=52,T=25,H=h-75,W=w-75;
 ctx.clearRect(0,0,w,h);ctx.font='15px Arial';ctx.fillStyle='#36574a';ctx.textAlign='right';
 for(let i=0;i<=4;i++){const value=max-i*max/4,y=T+i*H/4;ctx.strokeStyle='#ddd9cb';ctx.beginPath();ctx.moveTo(L,y);ctx.lineTo(L+W,y);ctx.stroke();ctx.fillText(value.toFixed(1),L-7,y+5);}
 ctx.strokeStyle='#176e59';ctx.fillStyle='#b97749';ctx.lineWidth=2;ctx.beginPath();
 values.forEach((v,i)=>{const x=L+i*W/(values.length-1),y=T+H*(1-v/max);if(bars)ctx.fillRect(x-4,y,8,T+H-y);else if(i)ctx.lineTo(x,y);else ctx.moveTo(x,y);});if(!bars)ctx.stroke();
 ctx.fillStyle='#36574a';ctx.textAlign='center';for(const i of [0,8,16,24,32,...(bars?[]:[40,48,56,63])])ctx.fillText(String(i),L+i*W/(values.length-1),h-28);ctx.fillText(label,L+W/2,h-7);
}
if(typeof document!=='undefined'){
 const el=id=>document.getElementById(id);
 const render=()=>{const f=Number(el('waveFreq').value),a=Number(el('waveAmp').value),hann=el('waveWindow').checked,{samples,bins}=teachingSpectrum(f,a,hann);
 el('waveFreqOut').textContent=f.toFixed(2)+' מחזורים';el('waveAmpOut').textContent=a.toFixed(2);
 // Offset is shown explicitly so the graph uses a simple, fixed unipolar scale.
 paint(el('waveTime'),samples.map(x=>x+2),false,4,'Sample index · signal + 2');paint(el('waveSpectrum'),bins,true,2,'Frequency bin');
 el('waveExplain').textContent='ההחזר הראשון קבוע בתא 5. השני: '+f.toFixed(2)+'. '+(hann?'חלון Hann מקטין זליגה לצדדים, אך מרחיב את השיא.':'ללא חלון, תדר שאינו נופל בדיוק על תא מפזר אנרגיה לתאים נוספים.')+' לגרף הזמן נוסף היסט של 2 לציור בלבד; ההיסט אינו נכנס להתמרה. שני ההחזרים מתחילים באותו מופע.';
 };
 for(const id of ['waveFreq','waveAmp','waveWindow'])el(id).addEventListener('input',render);render();
}
