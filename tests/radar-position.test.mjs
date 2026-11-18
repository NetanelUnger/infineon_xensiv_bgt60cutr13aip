import {test} from 'node:test';
import assert from 'node:assert/strict';
import {positionModel} from '../web/radar-position.js';

test('one range can represent opposite sides; spatial receive phase changes sign',()=>{
  const right=positionModel(.8,30,90),left=positionModel(.8,-30,90);
  assert.ok(Math.abs(right.x-.4)<1e-12);
  assert.ok(Math.abs(right.forward-.692820323)<1e-9);
  assert.ok(Math.abs(Math.hypot(right.x,right.forward)-.8)<1e-12);
  assert.equal(right.forward,left.forward);assert.equal(right.x,-left.x);
  assert.equal(right.phaseDeg,-left.phaseDeg);
  // 2.35 mm RX spacing at 60 GHz: ~84.66°, not the doubled round-trip phase.
  assert.ok(Math.abs(right.phaseDeg-84.658567)<1e-5);
  assert.equal(positionModel(.8,0,90).phaseDeg,0);
});

test('software FOV filtering changes acceptance without changing geometry or phase',()=>{
  const wide=positionModel(.8,30,90),narrow=positionModel(.8,30,40);
  assert.equal(wide.inside,true);assert.equal(narrow.inside,false);
  for(const k of ['x','forward','phaseDeg'])assert.equal(wide[k],narrow[k]);
  assert.equal(positionModel(.8,45,90).inside,true);
  assert.equal(positionModel(.8,45.01,90).inside,false);
  assert.equal(positionModel(.4,30,90).phaseDeg,wide.phaseDeg);
});
