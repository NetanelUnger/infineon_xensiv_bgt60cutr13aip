import test from 'node:test';
import assert from 'node:assert/strict';
import {echoPeaks, frameProfiles, SampleWindow, strongestChange} from '../web/live-experiment.js';

const frame = (ms,sequence,amplitude=1000) => ({
  ms:ms>>>0, sequence, rx:3, fifoErrors:0,
  adc(chirp,n,rx) {
    assert.equal(chirp,0);
    return 32000+(rx===1?amplitude:100)*Math.sin(2*Math.PI*(rx+7)*n/64);
  }
});
test('Wi-Fi peak comes from the measured receivers and responds to the display threshold',()=>{
  const profiles=frameProfiles(frame(0,0));
  assert.equal(echoPeaks(profiles,-65)[0].bin,8);
  assert.deepEqual(echoPeaks(profiles,-20),[]);
});
test('three-second samples use device time across rollover and leave missing intervals empty',()=>{
  const sample=new SampleWindow(),start=0xffffffd0;
  const add=(ms,seq)=>{
    const f=frame(start+ms,seq);
    return sample.add(f,frameProfiles(f));
  };
  assert.equal(add(0,12),false);
  add(10,13); add(100,15); add(2999,16);
  assert.equal(add(3000,17),true);
  assert.equal(sample.frames,4);
  assert.equal(sample.rows[0].count,2);
  assert.equal(sample.rows[1],null);
  assert.equal(sample.rows[59].count,1);
  assert.equal(sample.gaps,1);
});
test('comparison averages linear power and identifies the changed receiver and bin',()=>{
  const a=new SampleWindow(),b=new SampleWindow();
  for (const [slot,amplitudes] of [[a,[1000,1000]],[b,[2000,2000,2000]]]) {
    amplitudes.forEach((amp,i)=>{
      const f=frame(i*50,i,amp);
      slot.add(f,frameProfiles(f));
    });
  }
  const change=strongestChange(a,b);
  assert.equal(change.rx,2);assert.equal(change.bin,8);
  assert.ok(Math.abs(change.deltaDb-6.0206)<0.001);
  assert.equal(strongestChange(a,a).magnitude,0);
});
