import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const html=fs.readFileSync(new URL('../web/wiki.html',import.meta.url),'utf8');
test('wiki preserves unique anchors, working fragment links, and valid command text',()=>{
 const ids=[...html.matchAll(/\bid="([^"]+)"/g)].map(m=>m[1]);
 assert.equal(ids.length,new Set(ids).size);
 for(const m of html.matchAll(/href="#([^"]+)"/g))assert.ok(ids.includes(m[1]),m[1]);
 assert.ok(!/[\x00-\x08\x0b\x0c\x0e-\x1f]/.test(html));
 assert.ok(html.includes('.\\scripts\\build-ble.ps1 -GetLibraries'));
 assert.ok(html.includes('.\\scripts\\flash-ble.ps1'));
 assert.equal([...html.matchAll(/class="wiki-article"/g)].length,21);
 for(const m of html.matchAll(/(?:src|href)="([^"#:]+\.(?:js|css|json))"/g))assert.ok(fs.existsSync(new URL('../web/'+m[1],import.meta.url)),m[1]);
});
test('FMCW illustration computes physical numbers and responds to both inputs without hardware',()=>{
 const ctx=new Proxy({}, {get:()=>()=>{}});
 const elements={};
 for(const id of ['theoryRange','theoryBandwidth','theoryRangeText','theoryBandwidthText','theoryResult','theoryPlot'])elements[id]={value:id==='theoryRange'?'1.5':'1',listeners:{},addEventListener(e,f){this.listeners[e]=f;},getContext:()=>ctx,width:720,height:250};
 const sandbox={document:{getElementById:id=>elements[id]}};
 vm.runInNewContext(fs.readFileSync(new URL('../web/wiki-theory.js',import.meta.url),'utf8'),sandbox);
 assert.match(elements.theoryResult.textContent,/100 kHz/);
 assert.match(elements.theoryResult.textContent,/15.00/);
 elements.theoryRange.value='3';elements.theoryRange.listeners.input();
 assert.match(elements.theoryResult.textContent,/200 kHz/);
 elements.theoryBandwidth.value='2';elements.theoryBandwidth.listeners.input();
 assert.match(elements.theoryResult.textContent,/400 kHz/);
 assert.match(elements.theoryResult.textContent,/7.50/);
});
