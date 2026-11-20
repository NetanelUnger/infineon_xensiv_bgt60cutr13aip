import {createFifoTraces} from './fifo-traces.js';

// All three channels use chirp zero and the same ADC scale. U12.4 has
// four fractional bits. Keep channel DC offsets and relative amplitudes.
export function liveChirp(frame) {
  const channels = Array.from({length: frame.rx}, (_, rx) =>
    Array.from({length: frame.samples}, (_, sample) => frame.adc(0, sample, rx) / 16));
  const values = channels.flat();
  const low = Math.floor(Math.min(...values));
  const high = Math.max(low + 1, Math.ceil(Math.max(...values)));
  return {low, high, channels: channels.map(row => row.map(v => 2 * (v - low) / (high - low) - 1))};
}

export function createLiveRxView() {
  const plot = createFifoTraces(document.getElementById('liveRxTraces'));
  const caption = document.getElementById('liveRxReading');
  let pending = null, paint = 0, lastFrameAt = 0;
  function clear(message) {
    cancelAnimationFrame(paint);
    paint = 0;
    pending = null;
    lastFrameAt = 0;
    plot([[], [], []]);
    caption.textContent = message;
  }
  function show(frame) {
    pending = frame;
    lastFrameAt = performance.now();
    if (paint) return;
    paint = requestAnimationFrame(() => {
      paint = 0;
      const {low, high, channels} = liveChirp(pending);
      plot(channels);
      caption.textContent = `מסגרת ${pending.sequence} · טווח אנכי משותף: ${low} עד ${high} ביחידות ADC`;
      pending = null;
    });
  }
  function checkFreshness() {
    if (lastFrameAt && performance.now() - lastFrameAt > 2000) clear('ממתין למסגרת חדשה מהכרטיס.');
  }
  return {show, clear, checkFreshness};
}
