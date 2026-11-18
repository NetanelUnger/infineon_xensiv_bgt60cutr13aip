// Educational forward model, independent of acquisition, BLE and RAW analysis.
// The horizontal pair is generic: no RX channel mapping/calibration is assumed.
export function positionModel(rangeM,angleDeg,fovDeg) {
  if(![rangeM,angleDeg,fovDeg].every(Number.isFinite)||rangeM<=0||Math.abs(angleDeg)>90||fovDeg<=0||fovDeg>180)throw RangeError('Invalid geometry');
  const angle=angleDeg*Math.PI/180,wavelengthM=299792458/60e9,spacingM=.00235;
  return {x:rangeM*Math.sin(angle),forward:rangeM*Math.cos(angle),
    phaseDeg:360*spacingM*Math.sin(angle)/wavelengthM,inside:Math.abs(angleDeg)<=fovDeg/2};
}

export function createPositionLesson() {
  const $=id=>document.getElementById(id),origin={x:260,y:300},scale=230;
  const point=(r,a)=>({x:origin.x+scale*r*Math.sin(a*Math.PI/180),y:origin.y-scale*r*Math.cos(a*Math.PI/180)});
  const coords=p=>`${p.x.toFixed(2)} ${p.y.toFixed(2)}`;
  const signed=(v,digits=0)=>`${v>0?'+':''}${v.toFixed(digits)}`;
  function render() {
    const range=Number($('positionRange').value),angle=Number($('positionAngle').value),fov=Number($('positionFov').value);
    const model=positionModel(range,angle,fov),target=point(range,angle),left=point(1,-fov/2),right=point(1,fov/2);
    $('positionRangeValue').value=`${range.toFixed(2)} m`;
    $('positionAngleValue').value=`${signed(angle)}°`;
    $('positionFovValue').value=`${fov}° (±${fov/2}°)`;
    $('positionSector').setAttribute('d',`M260 300 L${coords(left)} A230 230 0 0 1 ${coords(right)} Z`);
    for(const [id,degrees] of [['positionLeftBound',-fov/2],['positionRightBound',fov/2]]) {
      const p=point(1.08,degrees);$(id).setAttribute('x',p.x);$(id).setAttribute('y',p.y-5);$(id).textContent=`${signed(degrees)}°`;
    }
    $('positionRay').setAttribute('x2',target.x);$('positionRay').setAttribute('y2',target.y);
    $('positionTarget').setAttribute('transform',`translate(${coords(target)})`);
    $('positionTarget').classList.toggle('outside',!model.inside);
    $('positionRangeLabel').setAttribute('x',(origin.x+target.x)/2+(angle>=0?32:-32));
    $('positionRangeLabel').setAttribute('y',(origin.y+target.y)/2-8);$('positionRangeLabel').textContent=`${range.toFixed(2)} m`;
    const arc=point(.17,angle);
    $('positionAngleArc').setAttribute('d',angle?`M260 ${300-scale*.17} A${scale*.17} ${scale*.17} 0 0 ${angle>=0?1:0} ${coords(arc)}`:'');
    $('positionX').textContent=`${signed(model.x,2)} m`;$('positionForward').textContent=`${model.forward.toFixed(2)} m`;
    $('positionPhase').textContent=`${signed(model.phaseDeg,1)}°`;
    $('positionStatus').classList.toggle('outside',!model.inside);
    $('positionStatus').textContent=model.inside?'בתוך תחום הפלט שנבחר. הימצאות בתחום לבדה אינה מבטיחה זיהוי.':'מחוץ לתחום הפלט שנבחר. החזר עשוי להיקלט ב־RAW גם כשהאלגוריתם מסנן את המטרה מהפלט.';
    $('positionReading').textContent=`המרחק נשאר ${range.toFixed(2)} מטר כשמשנים רק את הזווית. בהעברה לצד השני, המיקום הצדדי וסימן הפרש הפאזה מתהפכים. שינוי ה־FOV כאן משנה רק את אזור הפלט, ולא את המרחק או את הפאזה המחושבת.`;
    $('positionPlotDesc').textContent=`מבט מלמעלה. החיישן במרכז התחתון, קדימה הוא אפס מעלות. הלוח במרחק ${range.toFixed(2)} מטר ובזווית ${signed(angle)} מעלות. גבולות תחום הפלט: ${-fov/2} עד ${fov/2} מעלות. ${model.inside?'בתוך התחום.':'מחוץ לתחום.'}`;
  }
  for(const id of ['positionRange','positionAngle','positionFov'])$(id).addEventListener('input',render);
  $('positionMirror').onclick=()=>{$('positionAngle').value=-Number($('positionAngle').value);render();};
  render();
}
if(typeof document!=='undefined'&&document.getElementById('positionPlot'))createPositionLesson();
