// Isolated learning model: no BLE calls and no hardware settings.
const byId=id=>document.getElementById(id);
function redraw(){
 const r=Number(byId('theoryRange').value),b=Number(byId('theoryBandwidth').value);
 const delayNs=2*r/0.3,beatKhz=b*1e9/1e-4*delayNs*1e-9/1000,resCm=15/b;
 byId('theoryRangeText').textContent=`${r.toFixed(1)} מטר`;
 byId('theoryBandwidthText').textContent=`${b.toFixed(1)} GHz`;
 byId('theoryResult').textContent=`זמן הלוך־וחזור: ${delayNs.toFixed(1)} ns · תדר הפרש: ${beatKhz.toFixed(0)} kHz · הפרדת טווח אידאלית: ${resCm.toFixed(2)} ס״מ. הכפלת המרחק מכפילה את תדר ההפרש; הגדלת רוחב הפס גם משפרת את ההפרדה.`;
 const c=byId('theoryPlot'),ctx=c.getContext('2d');ctx.clearRect(0,0,c.width,c.height);
 ctx.strokeStyle='#9aab9e';ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(65,20);ctx.lineTo(65,205);ctx.lineTo(675,205);ctx.stroke();
 ctx.fillStyle='#304b42';ctx.font='16px Arial';ctx.textAlign='left';ctx.fillText('Frequency (illustration)',75,22);ctx.fillText('Time',630,232);
 const offset=r*18, rise=70+b*20;
 for(const [dx,color,label] of [[0,'#0d665b','TX'],[offset,'#bb7448','Echo']]){ctx.strokeStyle=color;ctx.lineWidth=4;ctx.beginPath();ctx.moveTo(95+dx,190);ctx.lineTo(530+dx,190-rise);ctx.stroke();ctx.fillStyle=color;ctx.fillText(label,535+dx,190-rise);}
 ctx.fillStyle='#627368';ctx.font='13px Arial';ctx.fillText('Delay magnified for visibility',90,245);
}
byId('theoryRange').addEventListener('input',redraw);byId('theoryBandwidth').addEventListener('input',redraw);redraw();
