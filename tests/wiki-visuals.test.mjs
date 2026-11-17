import test from 'node:test';
import assert from 'node:assert/strict';
import {teachingSpectrum} from '../web/wiki-visuals.js';
test('synthetic demo shows the two chosen tones with normalized amplitude',()=>{
 const {samples,bins}=teachingSpectrum(12,.6,false);assert.equal(samples.length,64);assert.equal(bins.length,33);assert.ok(Math.abs(bins[5]-1)<1e-10);assert.ok(Math.abs(bins[12]-.6)<1e-10);assert.ok(bins[8]<1e-10);
});
test('Hann reduces distant leakage for non-integer tone',()=>{
 const a=teachingSpectrum(12.5,.6,true).bins,b=teachingSpectrum(12.5,.6,false).bins;assert.ok(a[20]<b[20]);assert.ok(a.every(Number.isFinite));
});
