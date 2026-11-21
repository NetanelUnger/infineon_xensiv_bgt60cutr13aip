import {chirpPower, powerDb} from './raw-analysis.js';

// One spectrum per receiver, from the same chirp shown in the FIFO traces.
export function frameProfiles(frame) {
  return Array.from({length: frame.rx}, (_, rx) => chirpPower(frame, 0, rx).slice(1));
}

export function echoPeaks(profiles, thresholdDb) {
  const means = Array.from(profiles[0], (_, bin) =>
    profiles.reduce((sum, rx) => sum + rx[bin], 0) / profiles.length);
  return means.map((power, i) => ({bin: i + 1, db: powerDb(power), power}))
    .filter(({power, db}, i) => db >= thresholdDb &&
      power > (means[i-1] ?? 0) && power >= (means[i+1] ?? 0))
    .sort((a, b) => b.power - a.power);
}

export const windowMs = 3000, bucketMs = 50;

export class SampleWindow {
  constructor() {
    this.rows = Array(windowMs / bucketMs).fill(null);
    this.total = new Float64Array(96);
    this.frames = 0;
    this.first = null;
    this.last = null;
    this.gaps = 0;
  }
  add(frame, profiles) {
    this.first ??= frame;
    const elapsed = (frame.ms - this.first.ms) >>> 0;
    if (elapsed >= windowMs) return true;
    const index = Math.floor(elapsed / bucketMs);
    const row = this.rows[index] ??= {count: 0, power: new Float64Array(96)};
    for (let rx = 0; rx < 3; rx++) {
      for (let bin = 0; bin < 32; bin++) {
        const i = rx * 32 + bin;
        row.power[i] += profiles[rx][bin];
        this.total[i] += profiles[rx][bin];
      }
    }
    if (this.last) this.gaps += (frame.sequence - this.last.sequence - 1) >>> 0;
    this.last = frame;
    row.count++;
    this.frames++;
    return false;
  }
}

export function strongestChange(a, b) {
  // Select the largest absolute change in linear power. A large dB ratio
  // between two near-silent bins should not overshadow a strong reflection.
  let change = null;
  for (let i = 0; i < a.total.length; i++) {
    const before = a.total[i] / a.frames, after = b.total[i] / b.frames;
    const magnitude = Math.abs(after - before);
    if (!change || magnitude > change.magnitude) change = {rx: 1 + Math.floor(i / 32),
      bin: 1 + i % 32, magnitude, deltaDb: powerDb(after) - powerDb(before)};
  }
  return change;
}
