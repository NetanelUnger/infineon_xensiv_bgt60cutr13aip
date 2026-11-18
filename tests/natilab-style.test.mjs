import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
const read=name=>fs.readFileSync(new URL('../web/'+name,import.meta.url),'utf8');
test('both radar surfaces load the shared NatiLab identity and project navigation',()=>{
 for(const name of ['index.html','wiki.html']){
  const html=read(name);
  assert.match(html,/href="natilab.css"/);
  assert.match(html,/class="nl-nav"/);
  assert.match(html,/href="https:\/\/natilab.net\/"/);
  assert.match(html,/aria-current="page"/);
  assert.match(html,/dir="rtl"/);
 }
});
test('shared presentation remains scoped and includes phone and keyboard accessibility',()=>{
 const css=read('natilab.css');
 assert.match(css,/@media\(max-width:800px\)/);
 assert.match(css,/:focus-visible/);
 assert.match(css,/prefers-reduced-motion/);
 assert.match(css,/--paper:#f4efe6/);
 assert.match(css,/--accent:#e54b2f/);
 assert.match(css,/\.demo-page/);
});

test('guide shell is identical on radar wiki and dashboard, before navigation',()=>{
 const pages=['index.html','wiki.html'].map(read);
 const headers=pages.map(html=>html.match(/<header class="nl-header">[\s\S]*?<\/header>/)[0]);
 assert.equal(headers[0],headers[1]);
 for(const html of pages){assert.match(html,/href="guide-shell.css"/);assert.ok(html.indexOf('class="nl-header"')<html.indexOf('class="nl-nav"'));}
 assert.match(read('guide-shell.css'),/@media\(max-width:600px\)/);
});
