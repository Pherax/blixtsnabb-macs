(() => {
'use strict';
function boot(){
 document.title='BLIXTSNABB';
 const meta=document.createElement('meta');meta.name='viewport';meta.content='width=device-width,initial-scale=1';document.head.append(meta);
 const names=['Bottom left','Top left','Middle','Bottom right','Top right'];
 const zones=names.map(name=>({name,level:null,setting:'Unknown',pan:'Unknown',hot:'Unknown',raw:'—',remaining:null}));
 let masterReady=false,controllerFault=false;
 let selected=2,mode='Unknown',status='Connecting',hob='Unknown',online=false,busy=false,stopBusy=false,lastEvent=0,requestSerial=0,activeRequest=null;
 const ids=new Map(),values=new Map();
 const icons={pan:'<path d="M4 10h14v6a4 4 0 0 1-4 4H8a4 4 0 0 1-4-4zM18 12h4M6 6h10"/>',heat:'<path d="M6 21c-5-5 5-7 0-12M12 21c-5-6 5-8 0-16M18 21c-5-5 5-7 0-12"/>',power:'<path d="M12 2v10M6 5a9 9 0 1 0 12 0"/>',stop:'<rect x="5" y="5" width="14" height="14" rx="2"/>',off:'<circle cx="12" cy="12" r="8"/>',reset:'<path d="M3 11a9 9 0 1 1 2 7M3 4v7h7"/>'};
 const icon=name=>'<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">'+icons[name]+'</svg>';
 const minimal=document.createElement('style');minimal.textContent=`
:root{color-scheme:dark;--oak:#d2b18a;--white:#f3f4f2;--muted:#8b9cab;--line:#ffffff15}*{box-sizing:border-box}body{margin:0;background:#07121e;color:var(--white);font:14px -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}button,select{font:inherit;color:inherit;cursor:pointer}button{touch-action:manipulation}button:disabled{opacity:.3;cursor:default}button:focus-visible,select:focus-visible,summary:focus-visible{outline:2px solid var(--oak);outline-offset:5px}[hidden]{display:none!important}main{max-width:1050px;margin:0 auto;padding:48px 36px}header{display:flex;justify-content:space-between;align-items:center;margin-bottom:32px}h1{margin:0;font-size:12px;letter-spacing:5px;font-weight:600}.connection{font-size:11px;color:var(--muted);display:flex;align-items:center;gap:9px}.connection:before{content:"";width:5px;height:5px;background:#88b5a6;border-radius:50%}.layout{padding:28px 28px 23px;background:linear-gradient(135deg,#102236,#0a192a);border:1px solid #ffffff12;border-radius:30px;box-shadow:0 30px 80px #0003;position:relative}.layout:after{content:"";position:absolute;bottom:-5px;left:28px;right:28px;height:5px;border-radius:0 0 8px 8px;background:linear-gradient(90deg,#9e7e59,#d5b58c 28%,#b59570 60%,#d2b08a)}.wood{padding:0;background:none}.hob{display:grid;position:relative;grid-template-columns:1fr 1.08fr 1fr;grid-template-rows:repeat(4,78px);gap:8px 38px;padding:8px 18px 25px}.zone{position:relative;display:flex;flex-direction:column;justify-content:center;align-items:center;gap:10px;border:1px solid #ffffff21;border-radius:40px;background:transparent;transition:background .2s,border-color .2s}.zone:before{content:"";position:absolute;inset:7px;border:1px solid #ffffff06;border-radius:inherit;pointer-events:none}.zone:hover{background:#ffffff04;border-color:#ffffff40}.zone.sel{border-color:var(--oak);background:radial-gradient(ellipse at center,#d2b18a0a,transparent 75%)}.zone.sel:before{border-color:#d2b18a16}.zone strong{font-size:52px;font-weight:300;line-height:1;letter-spacing:-2px;font-variant-numeric:tabular-nums}.zone small{display:none}.zone span{display:flex;align-items:center;justify-content:center;height:18px;font-size:10px}.zone svg{width:23px;height:18px}.zone .heat{position:absolute;right:20px;top:19px}.zone .heat[data-state="clear"]{visibility:hidden}.zone .heat[data-state="hot"]{color:#edb183}.zone .pan[data-state="absent"]{opacity:.16}.zone .pan[data-state="unknown"],.zone .heat[data-state="unknown"]{color:var(--muted)}.zone[data-z="2"]{grid-area:1/2/5/3;align-self:center;aspect-ratio:1;border-radius:50%}.zone[data-z="3"]{grid-area:3/3/5/4}.zone[data-z="4"]{grid-area:1/3/3/4}.zone.left-section{border:0;border-radius:0;z-index:1;overflow:visible}.zone.left-section:before{display:none}.zone.left-section.sel{background:#d2b18a08}.zone.left-section[data-section="3"]{grid-area:1/1;border-radius:32px 32px 0 0}.zone.left-section[data-section="2"]{grid-area:2/1}.zone.left-section[data-section="1"]{grid-area:3/1}.zone.left-section[data-section="0"]{grid-area:4/1;border-radius:0 0 32px 32px}.hob:before{content:"";grid-area:1/1/5/2;border:1px solid #ffffff21;border-radius:34px;pointer-events:none}.zone[data-section="2"]:after{content:"";position:absolute;bottom:-5px;left:22px;right:22px;height:1px;background:#ffffff20}.zone.left-section strong{position:absolute;left:33%;top:calc(100% - 21px);font-size:42px;z-index:2;pointer-events:none}.zone[data-section="2"] strong,.zone[data-section="0"] strong{display:none}.zone.left-section .pan{position:absolute;right:20px;top:50%;transform:translateY(-50%)}.zone.left-section .heat{right:50px;top:50%;transform:translateY(-50%)}.zone.left-section svg{width:18px;height:16px}aside{border-top:1px solid var(--line);padding-top:22px}aside>small,aside h2,.readout,#source,aside>.facts{display:none}aside>#panTimer:not([hidden]){display:flex;justify-content:space-between;margin-bottom:16px;color:var(--oak);font-size:12px}.levels{display:flex;padding:4px;background:#06132080;border:1px solid #ffffff08;border-radius:17px;gap:5px}.levels button{background:none;border:0;flex:1;min-width:0;height:48px;border-radius:12px;color:#a8b6c4;font-size:17px}.levels button:hover:not(:disabled){background:#ffffff09;color:white}.levels button.chosen{background:var(--oak);color:#142332;box-shadow:0 3px 12px #0003}.actions{display:flex;align-items:center;gap:12px;margin:27px 0 0}.actions button{display:grid;place-items:center;background:#112339;border:1px solid #ffffff10;width:48px;height:48px;border-radius:50%}.actions svg{width:20px;height:20px}.actions #power{border-color:#d2b18a50;color:var(--oak)}.actions #stop{margin-left:auto;border-radius:14px;width:76px;color:var(--oak);background:#d2b18a0a;border-color:#d2b18a35}.actions #reset{color:#f0b185}.actions #mode{display:none}#feedback{font-size:11px;color:var(--oak);min-height:14px;margin-top:15px}footer{font-size:11px;color:var(--muted);line-height:1.5}.tbd{display:none}details{margin-top:20px;color:var(--muted);font-size:12px}summary{cursor:pointer;list-style:none;width:fit-content;padding:8px 0}main>details:not(.technical)>summary{font-size:24px;letter-spacing:4px;line-height:1}main>details:not(.technical){border-top:1px solid var(--line);margin-top:22px}.technical{font-size:11px}.technical p{max-width:750px;line-height:1.6}.feature-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin:18px 0}.feature-grid button{background:#102237;border:1px solid var(--line);border-radius:12px;padding:15px;text-align:left;font-size:12px}.feature-grid small{display:block;margin-top:7px;font-size:10px}.feature-grid button:disabled{opacity:.55}select{background:#12263c;border:1px solid var(--line);padding:10px;border-radius:10px;max-width:130px}.settings-mode{margin:20px 0}.settings-mode label{margin-right:14px}.legend{display:flex;gap:12px;align-items:center;flex-wrap:wrap;font-size:11px}.legend svg{width:18px;height:18px}.facts{display:flex;justify-content:space-between;padding:12px 0;border-bottom:1px solid var(--line)}pre{white-space:pre-wrap;font-size:10px}.connection[data-live="false"]:before{background:#d5a276}#selection{display:flex;justify-content:space-between;align-items:center;padding:0 3px 14px;font-size:11px;letter-spacing:.3px;color:var(--muted)}#selection span:last-child{color:var(--oak)}@media(max-width:650px){main{padding:25px 16px}header{margin-bottom:23px}.layout{padding:18px 12px 16px;border-radius:23px}.hob{padding:4px 4px 20px;grid-template-rows:repeat(4,61px);gap:7px 18px}.zone{border-radius:28px}.zone strong{font-size:38px}.zone.left-section strong{font-size:32px;left:24%}.zone.left-section .pan{right:10px}.zone.left-section .heat{right:32px}.zone .heat{right:12px;top:14px}.zone svg{width:18px;height:16px}.zone[data-section="2"]:after{left:12px;right:12px}.levels{gap:1px;padding:3px;border-radius:13px}.levels button{font-size:14px;height:43px;border-radius:9px}.feature-grid{grid-template-columns:repeat(2,1fr)}.layout:after{left:22px;right:22px}.actions{margin-top:24px}}aside>#panTimer[hidden]{display:none!important}
.actions #power[data-power="on"]{background:var(--oak);color:#122135;border-color:var(--oak);box-shadow:0 0 0 5px #d2b18a0a}
.actions #power[data-power="off"]{color:#a2afbb;background:#122132;border-color:#ffffff25}
#powerState{font-size:12px;color:var(--muted)}
.layout[data-power="off"]{background:#111c27;border-color:#ffffff09;box-shadow:0 18px 60px #0002}
.layout[data-power="off"]:after{background:#56616b}
.layout[data-power="off"] .zone{border-color:#ffffff12;background:transparent;color:#687784}
.layout[data-power="off"] .zone:before,.layout[data-power="off"] .hob:before{border-color:#ffffff0e}
.layout[data-power="off"] .zone.sel{background:#ffffff02;border-color:#788692}
.layout[data-power="off"] .left-section.sel{background:#ffffff03}
.layout[data-power="off"] .zone .pan{color:#7c8b97}
.layout[data-power="off"] .levels .chosen{background:#435361;color:#d8dfe5}
.layout[data-power="off"] #selectedValue{color:#8c9aa5}
.layout[data-power="off"] .zone .heat[data-state="hot"]{color:#ffc18d;opacity:1}
.layout[data-power="off"] .zone .heat[data-state="unknown"]{color:#a9b4be;opacity:1}

.zone .indicators .pan[data-state="absent"]{display:none}
.zone .indicators{display:flex;align-items:center;justify-content:center;gap:6px;height:20px}
.zone .indicators .pan,.zone .indicators .heat{position:static;transform:none;margin:0}
.zone.left-section .indicators{position:absolute;right:12px;top:50%;transform:translateY(-50%);gap:5px}
@media(max-width:650px){.zone.left-section .indicators{right:6px;gap:3px}.zone .indicators{gap:5px}}
@media(prefers-reduced-motion:reduce){*{transition:none!important}}
`;document.head.append(minimal);
 document.body.innerHTML=`<main><header><h1>BLIXTSNABB</h1><span class="connection" id="connection">Connecting</span></header><div class="layout"><section class="wood"><div class="hob">${[3,2,1,0].map(i=>`<button class="zone left-section" data-z="${i<2?0:1}" data-section="${i}" aria-label="Left section ${i+1}, ${i<2?'bottom-left':'top-left'} pair control"><small>Left ${i+1}</small><strong>—</strong><span class="pan">Pan unknown</span><span class="heat">Heat unknown</span></button>`).join('')}${zones.slice(2).map((z,j)=>`<button class="zone" data-z="${j+2}" aria-label="Select ${z.name}"><small>${z.name}</small><strong>—</strong><span class="pan">Pan unknown</span><span class="heat">Heat unknown</span></button>`).join('')}</div></section><aside><small>SELECTED ZONE</small><h2 id="name"></h2><div class="readout" id="level">—</div><small id="source">Observed setting</small><div class="facts"><span>Pan</span><span id="pan">Unknown</span></div><div class="facts"><span>Heat</span><span id="heat">Unknown</span></div><div class="facts" id="panTimer" hidden><span>No-pan stop</span><span id="remaining"></span></div><div class="levels">${Array.from({length:11},(_,i)=>`<button data-level="${i}">${i===10?'P':i}</button>`).join('')}</div></aside></div><div class="actions"><button id="power" aria-label="Power"><span></span></button><span id="powerState" role="status">Unknown</span><button id="stop" class="stop">Stop all</button><button id="reset">Reset fault</button><select id="mode" aria-label="Control mode"><option disabled>Auto</option><option>Listen</option><option>Dev</option><option>Master</option></select></div><div id="feedback" role="status"></div><footer><span id="status">Connecting</span><div class="tbd"><span>No pan · 60 s (Master)</span><span>Energy · TBD</span><span>Flex · TBD</span></div></footer><details><summary>Details</summary><p>Dev sends one-shot tests. The original panel remains master and can shut the hob off after an injected command; the ESP does not retry. Master requires the touch panel disconnected. Bus setpoints may follow pan position and differ from panel selections. Left pan states combine two sections per zone in default mode; Flex regrouping is not supported. Commanded levels are not confirmation of heating. Unknown heat does not mean cold.</p><div class="sections" aria-label="Left induction section status"><small>LEFT SECTIONS · TOP TO BOTTOM</small><div class="facts"><span>4 · Top</span><span id="section3">Unknown</span></div><div class="facts"><span>3 · Upper middle</span><span id="section2">Unknown</span></div><div class="facts"><span>2 · Lower middle</span><span id="section1">Unknown</span></div><div class="facts"><span>1 · Bottom</span><span id="section0">Unknown</span></div></div><p id="revision">Firmware revision: waiting</p><pre id="raw"></pre></details></main>`;
 document.querySelectorAll('.zone').forEach(zone=>{const indicators=document.createElement('span');indicators.className='indicators';indicators.append(zone.querySelector('.pan'),zone.querySelector('.heat'));zone.append(indicators);});
 const technical=document.querySelector('main>details');technical.classList.add('technical');technical.querySelector('summary').textContent='Diagnostics';
 const features=document.createElement('details');features.innerHTML='<summary>•••</summary><div class="legend">'+icon('pan')+' Pan detected · hidden: absent · ?: unknown '+icon('heat')+' Residual heat · ?: unknown</div><div class="settings-mode"><label>Controller mode </label></div><div class="feature-grid"></div><p>These manual features need verified firmware support before they can be enabled. A browser timer is not used to stop heating.</p>';
 technical.before(features);features.querySelector('summary').setAttribute('aria-label','Features and settings');features.querySelector('.settings-mode').append(document.getElementById('mode'));document.getElementById('mode').style.display='inline-block';
 const unavailable=['Zone timer · 0–99 min','Minute minder','Pause / resume','Cleaning lock','Child lock','Bridge · right zones','Multi-Flexi · 3 + 1','Multi-Flexi · 4 sections','Preset cooking','Sound settings'];
 features.querySelector('.feature-grid').innerHTML='<button disabled>Multi-Flexi · 2 + 2<small>Current supported layout</small></button>'+unavailable.map(name=>'<button disabled title="MACS command not verified">'+name+'<small>Not available yet</small></button>').join('');
 const protections=document.createElement('p');protections.textContent='No-pan stop: 60 s in Master. Manufacturer power sharing, Booster duration and automatic shutoff behaviour are not verified with the original panel removed.';features.append(protections);
 for(const [id,label,glyph] of [['power','Power','power'],['stop','Stop all zones','stop'],['reset','Reset controller fault','reset']]){const b=document.getElementById(id);b.innerHTML=icon(glyph);b.title=label;b.setAttribute('aria-label',label);}
 function symbols(el,pan,heat){
 const p=el.querySelector('.pan'),h=el.querySelector('.heat');
 const present=/present/i.test(pan),absent=/absent/i.test(pan),hot=heat==='Hot'||heat==='H · Hot',clear=heat==='Not hot'||heat==='No heat flag';
 p.dataset.state=present?'present':absent?'absent':'unknown';p.innerHTML=icon('pan')+(!present&&!absent?'?':'');p.title=present?'Pan detected':absent?'Pan absent':'Pan state unknown';p.setAttribute('aria-label',p.title);
 h.dataset.state=hot?'hot':clear?'clear':'unknown';h.innerHTML=icon('heat')+(!hot&&!clear?'?':'');h.title=hot?'Residual heat':clear?'No residual heat flag; surface may still be hot':'Heat state unknown';h.setAttribute('aria-label',h.title);
 }
 const selection=document.createElement('div');selection.id='selection';selection.innerHTML='<span id=selectedCaption></span><span id=selectedValue></span>';document.querySelector('aside').prepend(selection);
 const $=id=>document.getElementById(id),norm=s=>String(s).toLowerCase().replace(/[^a-z0-9]+/g,'_');
 function render(){
 const live=online&&Date.now()-lastEvent<20000,control=live&&!busy&&mode!=='Listen'&&mode!=='Auto'&&mode!=='Unknown'&&!status.startsWith('Stopping');
 $('connection').textContent=live?'Connected':'Disconnected';$('connection').dataset.live=String(live);
 for(const el of document.querySelectorAll("[data-z]")){let i=Number(el.dataset.z),z=zones[i];let v=mode==='Master'?z.level:z.setting;el.classList.toggle('sel',selected===i);el.classList.toggle('hot',live&&z.hot==='Hot');el.querySelector('strong').textContent=!live||v===null||v==='Unknown'?'—':v===10?'P':v===0?'Off':v;el.querySelector('.pan').textContent=live?z.pan:'Unknown';el.querySelector('.heat').textContent=live&&z.hot==='Hot'?'H · Hot':live&&z.hot==='Not hot'?'No heat flag':'Heat unknown';}
 for(let i=0;i<4;i++){
 const zone=zones[i<2?0:1],match=zone.raw.match(/status=([0-9a-f]{2})\/([0-9a-f]{2})/i);
 let text='Unknown',panText='Pan unknown',heatText='Heat unknown',hot=false;
 if(live&&match){const raw=parseInt(match[1+i%2],16),code=raw&0x6F;
 const pan=zone.pan==='Unknown'?'Pan unknown':code===5?'Pan present':code===0?'Pan absent':'Pan unknown';
 panText=pan;hot=!!(raw&0x80);heatText=hot?'H · Hot':'No heat flag';
 text=pan+' · '+(raw&0x80?'H flag':'No heat flag')+' · '+raw.toString(16).toUpperCase().padStart(2,'0');}
 $('section'+i).textContent=text;
 const tile=document.querySelector('[data-section="'+i+'"]');
 tile.querySelector('.pan').textContent=panText;tile.querySelector('.heat').textContent=heatText;
 tile.classList.toggle('hot',hot);

 }
 for(const el of document.querySelectorAll('[data-z]')){symbols(el,el.querySelector('.pan').textContent,el.querySelector('.heat').textContent);if(el.querySelector('strong').textContent==='Off')el.querySelector('strong').textContent='0';el.setAttribute('aria-pressed',el.classList.contains('sel')?'true':'false');}
 const z=zones[selected];$('panTimer').hidden=!live||mode!=='Master'||z.remaining===null;$('remaining').textContent=z.remaining===null?'':z.remaining+' s';document.querySelector('.levels').setAttribute('aria-label',z.name+' power level');document.querySelector('.levels').title=z.name+(selected<2?' · paired':'');$('name').textContent=z.name+(selected<2?' · paired':'');$('level').textContent=document.querySelector(`[data-z="${selected}"] strong`).textContent;$('pan').textContent=live?z.pan:'Unknown';$('heat').textContent=live?(z.hot==='Not hot'?'No heat flag':z.hot):'Unknown';$('source').textContent=mode==='Master'?'Commanded level':mode==='Dev'?'Bus setpoint · one-shot tests':'Observed bus setpoint';
 document.querySelectorAll('[data-level]').forEach(b=>{b.disabled=!control||(Number(b.dataset.level)>0&&mode==='Master'&&!masterReady);b.classList.toggle('chosen',live&&Number(b.dataset.level)===z.level);});
 const powerState=live?(/^(commanded )?on$/i.test(hob)?'on':/^(commanded )?off$/i.test(hob)?'off':'unknown'):'unknown';
 document.querySelector('.layout').dataset.power=powerState;
 $('power').dataset.power=powerState;
 $('power').setAttribute('aria-pressed',powerState==='unknown'?'mixed':String(powerState==='on'));
 $('power').title=powerState==='on'?'Switch hob off':powerState==='off'?'Switch hob on':'Power state unavailable';
 $('power').setAttribute('aria-label',$('power').title);
 $('power').disabled=!control||powerState==='unknown'||(powerState==='off'&&mode==='Master'&&!masterReady);
 $('powerState').textContent=!live?'Disconnected':powerState==='unknown'?'Power unknown':/^commanded /i.test(hob)?(powerState==='on'?'Commanded on':'Commanded off'):(powerState==='on'?'On':'Off');
 if(powerState!=='on')document.querySelectorAll('[data-level]').forEach(b=>{if(Number(b.dataset.level)>0)b.disabled=true;});
 $('reset').disabled=!live||busy||mode!=='Master';
 $('reset').hidden=mode!=='Master'||(!controllerFault&&masterReady);
 $('stop').disabled=!live||stopBusy||mode==='Listen'||mode==='Auto'||mode==='Unknown';
 $('mode').disabled=!live||busy||mode==='Auto'||status.startsWith('Stopping');if(mode!=='Unknown')$('mode').value=mode;
 $('status').textContent=live?`${hob} · ${status}`:'Connection lost · controls unavailable';
 $('selectedCaption').textContent=zones[selected].name+(selected<2?' · paired':'');$('selectedValue').textContent=$('level').textContent;
 $('raw').textContent=zones.map(z=>`${z.name}: ${z.raw}`).join('\n');
 }
 function ingest(d){const id=String(d.name_id||d.id||''), key=norm(id);if(!id)return;ids.set(key,id);values.set(key,d.state);const matches=(domain,name)=>key===norm(domain+'/'+name)||key.endsWith('_'+norm(name));
 if(matches('binary_sensor','Master ready'))masterReady=d.value===true||d.state==='ON';
 if(matches('binary_sensor','Controller fault'))controllerFault=d.value===true||d.state==='ON';
 if(matches('text_sensor','Firmware revision'))$('revision').textContent='Firmware revision: '+d.state;
 if(matches('select','Mode'))mode=d.state;
 if(matches('text_sensor','Status'))status=d.state;
 if(matches('text_sensor','Hob state'))hob=d.state;
 for(const z of zones){if(key===norm('sensor/'+z.name+' pan timeout')){const n=Number(d.value??d.state);z.remaining=Number.isFinite(n)?n:null;}if(key===norm('number/'+z.name)){let n=Number(d.value??d.state);z.level=Number.isFinite(n)?n:null;}for(const f of ['setting','pan','hot','raw'])if(matches('text_sensor',z.name+' '+f))z[f]=String(d.state??'Unknown');}render();}
 async function command(domain,name,method,args={}){
 const priority=domain==='button'&&name==='Stop all zones';
 if(priority?stopBusy:busy)return;
 if(priority&&activeRequest)activeRequest.abort();
 const serial=++requestSerial,controller=new AbortController();activeRequest=controller;
 busy=true;stopBusy=priority;render();$('feedback').textContent=priority?'Sending stop…':'Sending…';
 const timer=setTimeout(()=>controller.abort(),5000);
 try{
 const identifier=ids.get(norm(domain+'/'+name))||domain+'/'+name;
 const url='/'+identifier.split('/').map(encodeURIComponent).join('/')+'/'+method+'?'+new URLSearchParams(args);
 const r=await fetch(url,{method:'POST',signal:controller.signal});if(!r.ok)throw Error('HTTP '+r.status);
 if(serial===requestSerial)$('feedback').textContent=priority?'Stop requested · check hob state':'Request sent · check reported state';
 }catch(e){if(serial===requestSerial)$('feedback').textContent='Request failed: '+e.message;}
 finally{clearTimeout(timer);if(serial===requestSerial){busy=false;stopBusy=false;activeRequest=null;render();}}
 }

 document.querySelectorAll('[data-z]').forEach(b=>b.onclick=()=>{selected=Number(b.dataset.z);render();});document.querySelectorAll('[data-level]').forEach(b=>b.onclick=()=>command('number',zones[selected].name,'set',{value:b.dataset.level}));
 $('reset').onclick=()=>command('button','Reset fault','press');$('power').onclick=()=>{if($('power').disabled)return;command('button',$('power').dataset.power==='on'?'Hob off':'Hob on','press');};$('stop').onclick=()=>command('button','Stop all zones','press');$('mode').onchange=()=>{const target=$('mode').value;if(target==='Master'&&!confirm('Master replaces the touch panel. Is the original panel disconnected?')){render();return;}command('select','Mode','set',{option:target});};
 const es=new EventSource('/events');es.onopen=()=>{online=true;lastEvent=Date.now();render();};es.onerror=()=>{online=false;render();};es.addEventListener('ping',()=>{lastEvent=Date.now();});es.addEventListener('state',e=>{lastEvent=Date.now();try{ingest(JSON.parse(e.data));}catch(_){}});setInterval(render,1000);render();
}
if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',boot);else boot();
})();
