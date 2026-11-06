// From package root: npm ci && npm test
const {JSDOM}=require('jsdom');
const fs=require('fs'),assert=require('assert');
(async()=>{
const dom=new JSDOM('<html><head></head><body></body></html>',{url:'http://hob/',runScripts:'outside-only'}),w=dom.window;
const requests=[];w.EventSource=class{constructor(){w.stream=this;this.handlers={};}addEventListener(k,f){this.handlers[k]=f;}emit(id,state,value){this.handlers.state({data:JSON.stringify({id,state,value})});}};
w.fetch=async(url,options)=>{requests.push({url,options});return {ok:true}};w.confirm=()=>true;
w.eval(fs.readFileSync(require('path').join(__dirname,'../MACS/dashboard.js'),'utf8'));
await new Promise(r=>w.document.addEventListener('DOMContentLoaded',r,{once:true}));w.stream.onopen();
const emit=(...args)=>w.stream.emit(...args),$=s=>w.document.querySelector(s);
emit('text_sensor/Firmware revision','macs-r5-20261029');assert.equal($('#revision').textContent,'Firmware revision: macs-r5-20261029');
emit('select/Mode','Master');emit('binary_sensor/Master ready','OFF',false);assert($('[data-level="6"]').disabled);emit('binary_sensor/Master ready','ON',true);emit('text_sensor/Status','Master, hob on');emit('text_sensor/Hob state','Commanded on');emit('number/Middle','4',4);emit('text_sensor/Middle pan','Present');emit('text_sensor/Middle hot','Hot');
emit('sensor/Middle pan timeout','45',45);assert.equal($('#remaining').textContent,'45 s');assert(!$('#panTimer').hidden);
assert.equal($('#level').textContent,'4');assert.equal($('#heat').textContent,'Hot');assert.equal($('#source').textContent,'Commanded level');
$('[data-level="6"]').click();await new Promise(r=>setTimeout(r,0));assert.equal(requests.at(-1).url,'/number/Middle/set?value=6');assert.equal(requests.at(-1).options.method,'POST');
$('#stop').click();await new Promise(r=>setTimeout(r,0));assert.equal(requests.at(-1).url,'/button/Stop%20all%20zones/press?');
emit('text_sensor/Middle hot','Unknown');assert.equal($('#heat').textContent,'Unknown');assert(!$('[data-z="2"]').classList.contains('hot'));
emit('select/Mode','Auto');assert($('[data-level="6"]').disabled);assert($('#mode').disabled);emit('select/Mode','Listen');assert($('[data-level="6"]').disabled);emit('select/Mode','Dev');assert(!$('[data-level="6"]').disabled);assert.equal($('#source').textContent,'Bus setpoint · one-shot tests');
emit('text_sensor/Bottom left pan','Present');emit('text_sensor/Bottom left raw','level=008 status=00/85');
emit('text_sensor/Top left pan','Present');emit('text_sensor/Top left raw','level=008 status=05/00');
assert.equal(w.document.querySelectorAll('.hob [data-section]').length,4);
assert.equal(w.document.querySelectorAll('.hob .zone').length,7);
assert.equal($('[data-section="1"] .pan').dataset.state,'present');
assert($('[data-section="1"]').classList.contains('hot'));
$('[data-section="0"]').click();assert.equal($('#name').textContent,'Bottom left · paired');
$('[data-section="3"]').click();assert.equal($('#name').textContent,'Top left · paired');
$('[data-z="2"]').click();
assert.equal($('#section0').textContent,'Pan absent · No heat flag · 00');
assert.equal($('#section1').textContent,'Pan present · H flag · 85');
assert.equal($('#section2').textContent,'Pan present · No heat flag · 05');
assert.equal($('#section3').textContent,'Pan absent · No heat flag · 00');
assert.equal(w.getComputedStyle($('#panTimer')).display,'none');
emit('text_sensor/Middle hot','Not hot');assert.equal($('#heat').textContent,'No heat flag');
w.stream.onerror();assert($('[data-level="6"]').disabled);assert.equal($('#heat').textContent,'Unknown');
// A pending ordinary request must not disable or delay Stop.
w.stream.onopen();emit('select/Mode','Master');
let levelSignal,resolveStop;
w.fetch=(url,options)=>{requests.push({url,options});if(url.includes('/number/')){levelSignal=options.signal;return new Promise((resolve,reject)=>options.signal.addEventListener('abort',()=>reject(new Error('aborted'))));}return new Promise(resolve=>{resolveStop=()=>resolve({ok:true});});};
$('[data-level="6"]').click();assert(!$('#stop').disabled);
$('#stop').click();assert(levelSignal.aborted);assert(requests.at(-1).url.includes('/button/Stop%20all%20zones/press'));assert($('#stop').disabled);
await new Promise(r=>setTimeout(r,0));assert($('#stop').disabled); // old request's finally cannot unlock current stop
resolveStop();await new Promise(r=>setTimeout(r,0));assert(!$('#stop').disabled);
emit('binary_sensor/Master ready','OFF',false);emit('binary_sensor/Controller fault','ON',true);assert(!$('#power').disabled); // Power off stays available during a fault while on
assert(!$('#stop').disabled);assert(!$('#reset').hidden);assert($('[data-level="6"]').disabled);assert(!$('[data-level="0"]').disabled);
assert.equal(w.document.querySelectorAll('.feature-grid button:not(:disabled)').length,0);
assert.equal($('#stop').getAttribute('aria-label'),'Stop all zones');
w.fetch=async(url,options)=>{requests.push({url,options});return {ok:true}};
emit('binary_sensor/Master ready','ON',true);emit('binary_sensor/Controller fault','OFF',false);
emit('text_sensor/Hob state','Commanded on');
assert.equal($('.layout').dataset.power,'on');assert.equal($('#power').getAttribute('aria-pressed'),'true');
$('#power').click();await new Promise(r=>setTimeout(r,0));assert.equal(requests.at(-1).url,'/button/Hob%20off/press?');
assert.equal($('.layout').dataset.power,'on'); // no optimistic state change
emit('text_sensor/Middle hot','Hot');emit('text_sensor/Hob state','Commanded off');
assert.equal($('.layout').dataset.power,'off');assert.equal($('#powerState').textContent,'Commanded off');assert($('[data-level="6"]').disabled);assert.equal($('[data-z="2"] .heat').dataset.state,'hot');
$('#power').click();await new Promise(r=>setTimeout(r,0));assert.equal(requests.at(-1).url,'/button/Hob%20on/press?');
emit('text_sensor/Hob state','Unknown');assert($('#power').disabled);assert.equal($('.layout').dataset.power,'unknown');
emit('text_sensor/Hob state','Off');emit('binary_sensor/Master ready','OFF',false);assert($('#power').disabled);
emit('text_sensor/Hob state','On');assert(!$('#power').disabled);w.stream.onerror();assert($('#power').disabled);assert.equal($('#powerState').textContent,'Disconnected');
console.log('Dashboard tests passed, including Stop during a stalled level request');dom.window.close();
})();
