import {frameProfiles, echoPeaks, SampleWindow, strongestChange} from './live-experiment.js';
import {createFifoMap, drawEchoPath, drawProfile, drawRiver} from './live-lab-charts.js';
import {powerDb} from './raw-analysis.js';

export function createLiveLab() {
  const el = id => document.getElementById(id);
  const drawMap = createFifoMap(el('fifoMap'));
  const saved = {A:null, B:null};
  let active = null, latest = null, held = null, receivedAt = null, paintedAt = -Infinity;
  let configuration = null;
  let panel = 'ble';
  const visible = () => !el('wifi').hidden && !el('wifiLab').hidden;
  function placeExperiment() {
    el('reviewSamples').hidden=!(saved.A || saved.B);
    const host=el(panel==='raw' && (saved.A || saved.B) ? 'rawExperimentHost' : 'wifiExperimentHost');
    if (el('experiment').parentElement!==host) host.append(el('experiment'));
  }

  function controls() {
    el('sampleA').disabled = el('sampleB').disabled = !!active || receivedAt === null;
    el('holdFrame').disabled = !latest;
    el('holdFrame').textContent = held ? 'חזרה למסגרת חיה' : 'הקפאת מסגרת לעיון';
  }
  function renderFrame() {
    const view = held || latest;
    if (!view || !visible()) return;
    const {frame,profiles} = view;
    const threshold = Number(el('peakThreshold').value);
    const peaks = echoPeaks(profiles, threshold);
    drawEchoPath(el('echoPath'), peaks[0]);
    const [low,high] = drawMap(frame);
    drawProfile(el('liveProfile'), profiles, threshold, peaks[0]);
    el('frameReading').textContent = [
      (held ? 'מסגרת מוקפאת ' : 'מסגרת ') + frame.sequence,
      'סולם מפת הדגימות ' + low.toFixed(1) + ' עד ' + high.toFixed(1) + ' ביחידות ADC',
      peaks.length ? 'תאי שיא מעל הסף, מהחזק לחלש: ' + peaks.map(p=>p.bin).join(', ') : 'אין שיא מעל הסף שנבחר'
    ].join(' · ');
  }
  function renderSample(key) {
    const sample = saved[key], canvas = el('river'+key);
    canvas.hidden = !sample;
    if (!sample) { el('sampleInfo'+key).textContent = 'טרם נדגם'; return; }
    const missing = sample.rows.filter(row=>!row).length;
    el('sampleInfo'+key).textContent = [
      sample.frames+' מסגרות',
      missing+' מתוך 60 חלונות זמן ריקים',
      'פערי רצף '+sample.gaps,
      'שגיאות FIFO '+(sample.last.fifoErrors-sample.first.fifoErrors)
    ].join(' · ');
    if (canvas.getBoundingClientRect().width) drawRiver(canvas,sample,saved.A && saved.B ? Number(el('compareBin').value) : null);
  }
  function inspectBin() {
    const bin=Number(el('compareBin').value);
    el('compareBinValue').textContent=bin;
    const table=el('binComparison');
    table.replaceChildren();
    for (let rx=0; rx<3; rx++) {
      const i=rx*32+bin-1;
      const a=powerDb(saved.A.total[i]/saved.A.frames), b=powerDb(saved.B.total[i]/saved.B.frames);
      const tr=table.insertRow();
      for (const value of ['RX'+(rx+1),a.toFixed(2),b.toFixed(2),((b>a)?'+':'')+(b-a).toFixed(2)]) {
        const cell=tr.insertCell();cell.dir='ltr';cell.textContent=value;
      }
    }
    redraw();
  }
  el('compareBin').oninput=inspectBin;
  function compare() {
    let text = '';
    el('inspectComparison').hidden=!(saved.A && saved.B);
    if (saved.A && saved.B) {
      const {rx,bin,magnitude,deltaDb} = strongestChange(saved.A,saved.B);
      el('compareBin').value=bin;
      inspectBin();
      text = magnitude === 0 ? 'ממוצעי העוצמה זהים בכל התאים שנדגמו.' :
        'השינוי הגדול ביותר בהספק הממוצע נמצא באנטנה RX'+rx+' בתא '+bin+
        '. אחרי ההזזה ההחזר '+(deltaDb>=0?'התחזק':'נחלש')+
        (Math.abs(deltaDb)<0.1?' בפחות מ־0.1 dB.':' ב־'+Math.abs(deltaDb).toFixed(1)+' dB.');
    }
    el('deltaReading').textContent = text;
  }
  function resetSamples() {
    active=null; saved.A=null; saved.B=null;
    placeExperiment();
    renderSample('A'); renderSample('B'); compare();
    el('sampleStatus').textContent='הדגימה זמינה בזמן קבלת מסגרות מהכרטיס.';
    controls();
  }
  function beginSession(meta) {
    const signature = JSON.stringify(meta.registers);
    if (configuration !== signature) resetSamples();
    configuration = signature;
  }
  for (const key of ['A','B']) el('sample'+key).onclick = () => {
    if (active || receivedAt === null) return;
    active = {key, sample:new SampleWindow()};
    el('sampleStatus').textContent='התחילה דגימה '+key+' למשך שלוש שניות.';
    controls();
  };
  function sampleFrame(frame, profiles) {
    const {key,sample} = active;
    if (sample.add(frame, profiles)) {
      saved[key]=sample; active=null;
      placeExperiment();
      renderSample(key); compare(); controls();
      el('sampleStatus').textContent='דגימה '+key+' הסתיימה.'+
        (key==='A' ? ' אפשר להזיז את החפץ ולדגום את B.' : '');
    } else {
      const elapsed = (frame.ms-sample.first.ms) >>> 0;
      el('sampleInfo'+key).textContent='דוגם · '+(elapsed/1000).toFixed(1)+' מתוך 3 שניות';
    }
  }
  function show(frame) {
    const now = performance.now();
    if (receivedAt === null) { receivedAt=now; controls(); }
    receivedAt=now;
    if (!active && (!visible() || now-paintedAt<120)) return;
    const profiles=frameProfiles(frame);
    if (active) sampleFrame(frame, profiles);
    if (!visible() || now-paintedAt<120) return;
    paintedAt=now; latest={frame,profiles};
    controls();
    if (!held) renderFrame();
  }
  function clear(message) {
    latest=null; held=null; receivedAt=null; paintedAt=-Infinity;
    if (active) {
      const key=active.key;
      active=null;
      renderSample(key);
      el('sampleStatus').textContent='דגימה '+key+' נקטעה. הפעילו את הזרם ודגמו שוב.';
    }
    for (const id of ['echoPath','fifoMap','liveProfile']) {
      const canvas=el(id);
      canvas.getContext('2d').clearRect(0,0,canvas.width,canvas.height);
    }
    el('frameReading').textContent=message;
    controls();
  }
  function redraw() {
    renderFrame();
    for (const key of ['A','B']) if (saved[key] && active?.key!==key) renderSample(key);
  }
  el('peakThreshold').oninput = () => {
    el('peakThresholdValue').textContent=el('peakThreshold').value+' dBFS';
    renderFrame();
  };
  el('holdFrame').onclick = () => {held=held?null:latest;controls();renderFrame();};
  let observedWidth=0;
  new ResizeObserver(([entry])=>{
    if (entry.contentRect.width===observedWidth) return;
    observedWidth=entry.contentRect.width;
    redraw();
  }).observe(el('experiment'));
  return {show, clear, beginSession, resetSamples, setPanel(value) {
    panel=value; placeExperiment(); redraw();
  }, checkFreshness() {
    if (receivedAt!==null && performance.now()-receivedAt>2000) clear('הזרם ממתין למסגרת חדשה.');
  }};
}
