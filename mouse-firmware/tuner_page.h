#ifndef TUNER_PAGE_H
#define TUNER_PAGE_H

// The tuner web page, served by mouse-firmware.ino at http://192.168.4.1
// Self-contained (no internet at the maze): HTML + CSS + JS in one string.

static const char TB_PAGE[] PROGMEM = R"TBPAGE(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Micromouse tuner</title>
<style>
:root{--bg:#0e1116;--p1:#161a21;--p2:#1d222b;--ln:#2b323d;--tx:#e7eaef;--mu:#98a2b0;--ac:#45b8ff;--ok:#39c98a;--wa:#f2b33d;--bd:#ff5a5f;--mono:ui-monospace,Menlo,Consolas,monospace}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{position:sticky;top:0;z-index:5;background:var(--p1);border-bottom:1px solid var(--ln)}
.bar{display:flex;align-items:center;gap:8px;padding:8px 12px;max-width:900px;margin:0 auto}
.bar h1{font-size:16px;margin:0;white-space:nowrap;flex:1}
#dot{width:10px;height:10px;border-radius:50%;background:var(--bd);flex:none}
#dot.on{background:var(--ok)}
#msg{max-width:900px;margin:0 auto;padding:0 12px 6px;color:var(--mu);font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#msg.busy{color:var(--wa)}
button{font:inherit;color:var(--tx);background:var(--p2);border:1px solid var(--ln);border-radius:8px;padding:8px 12px;cursor:pointer;touch-action:manipulation}
button:active{transform:translateY(1px)}
button.pri{background:#1c4f73;border-color:#2a6f9f}
button.warn{background:#6b4b12;border-color:#9a6c19}
button:disabled{opacity:.45}
#stop{background:var(--bd);border-color:#ff8a8d;color:#fff;font-weight:700;padding:8px 16px}
#save.dirty{background:#6b4b12;border-color:#f2b33d}
nav{display:flex;gap:4px;overflow-x:auto;padding:0 8px 6px;max-width:900px;margin:0 auto;scrollbar-width:none}
nav button{flex:none;background:transparent;border-color:transparent;color:var(--mu);padding:6px 8px;font-size:14px}
nav button.on{color:var(--tx);border-color:var(--ln);background:var(--p2)}
main{max-width:900px;margin:0 auto;padding:12px}
section{display:none}
section.on{display:block}
.card{background:var(--p1);border:1px solid var(--ln);border-radius:12px;padding:12px;margin-bottom:12px}
.card h2{font-size:15px;margin:0 0 8px}
.card h3{font-size:13px;margin:12px 0 6px;color:var(--mu);text-transform:uppercase;letter-spacing:.04em}
.note{color:var(--mu);font-size:13px;margin:0;padding-left:18px}
.note li{margin:2px 0}
.prm{border-top:1px solid var(--ln);padding:10px 0}
.prm:first-of-type{border-top:0}
.ph{display:flex;align-items:baseline;gap:8px;flex-wrap:wrap}
.pid{font-family:var(--mono);font-size:13px;font-weight:600}
.pdef{font-size:12px;color:var(--mu);margin-left:auto}
.pdef.chg{color:var(--wa)}
.pc{display:flex;align-items:center;gap:6px;margin-top:6px}
.pc input[type=range]{flex:1;min-width:0;accent-color:var(--ac);height:28px}
.pc input[type=number]{width:84px;font:inherit;color:var(--tx);background:var(--bg);border:1px solid var(--ln);border-radius:8px;padding:6px}
.pc .u{font-size:12px;color:var(--mu);width:54px}
.pc button{padding:6px 11px;min-width:38px}
.ed{padding:3px 12px;font-size:12.5px}
.prm:not(.lock) .ed{background:#1c4f73;border-color:#2a6f9f}
.prm.lock .pc{opacity:.5;pointer-events:none}
.help{font-size:12.5px;color:var(--mu);margin-top:4px}
.seg{display:flex;gap:4px}
.seg button.on{background:#1c4f73;border-color:#2a6f9f}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin:6px 0}
.row input{font:inherit;color:var(--tx);background:var(--bg);border:1px solid var(--ln);border-radius:8px;padding:7px;width:100px}
.row input.wide{flex:1;min-width:160px}
.small{font-size:12.5px;color:var(--mu)}
.line{margin:6px 0}
.sum{margin:8px 0;padding:8px;border:1px solid var(--ln);border-radius:10px;background:var(--p2)}
td.r1{text-align:left;white-space:normal;min-width:170px}
.log:empty{display:none}
.res{margin-top:8px;overflow-x:auto}
table{border-collapse:collapse;width:100%;font-size:13px}
th,td{padding:5px 6px;border-bottom:1px solid var(--ln);text-align:right;white-space:nowrap}
th{color:var(--mu);font-weight:500}
td.l,th.l{text-align:left}
td.why{text-align:left;white-space:normal;color:var(--wa);min-width:160px}
.ok{color:var(--ok)}.bad{color:var(--bd)}.wa{color:var(--wa)}
canvas{width:100%;height:190px;background:var(--bg);border:1px solid var(--ln);border-radius:8px;display:block;margin-top:8px}
.mouse{display:grid;grid-template-columns:1fr 150px 1fr;grid-template-rows:auto 1fr 1fr auto;gap:8px;align-items:center;max-width:460px;margin:0 auto}
.tof{background:var(--p2);border:1px solid var(--ln);border-radius:10px;padding:8px;text-align:center}
.tof b{display:block;font-size:22px;font-family:var(--mono)}
.tof span{font-size:11px;color:var(--mu)}
.body{grid-column:2;grid-row:2/4;height:190px;border:3px solid var(--ln);border-radius:14px;display:flex;flex-direction:column;justify-content:center;align-items:center;color:var(--mu);font-size:12px;text-align:center}
.body.wl{border-left-color:var(--ok)}.body.wr{border-right-color:var(--ok)}.body.wf{border-top-color:var(--ok)}
.kv{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:8px;margin-top:10px}
.kv div{background:var(--p2);border:1px solid var(--ln);border-radius:10px;padding:8px}
.kv b{display:block;font-family:var(--mono);font-size:18px}
.kv span{font-size:11.5px;color:var(--mu)}
#mazec{height:auto;aspect-ratio:1;max-width:520px;margin:8px auto 0}
.log{font-family:var(--mono);font-size:12px;max-height:260px;overflow:auto;background:var(--bg);border:1px solid var(--ln);border-radius:8px;padding:8px;margin-top:8px;white-space:pre-wrap}
textarea{width:100%;height:340px;white-space:pre;overflow:auto;font:12px/1.35 var(--mono);color:var(--tx);background:var(--bg);border:1px solid var(--ln);border-radius:8px;padding:8px}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:var(--p2);border:1px solid var(--ln);border-radius:10px;padding:10px 14px;max-width:92vw;display:none;z-index:9}
.chip{display:inline-block;font-size:12px;padding:2px 8px;border-radius:999px;border:1px solid var(--ln);color:var(--mu)}
</style>
</head><body>
<header>
 <div class="bar"><span id="dot"></span><h1>Micromouse tuner</h1>
  <button id="save" title="Save the current values in the mouse's flash">Save</button>
  <button id="stop">STOP</button></div>
 <div id="msg">connecting...</div>
 <nav id="tabs"></nav>
</header>
<main>

<section id="t-live">
 <div class="card"><h2>Live sensors</h2>
  <div class="mouse">
   <div></div><div class="tof" id="s0"><b>-</b><span>F</span></div><div></div>
   <div class="tof" id="s1"><b>-</b><span>LF</span></div>
   <div class="body" id="mbody">yaw<br><b id="lyaw" style="font-size:22px;color:var(--tx)">0.0</b>deg</div>
   <div class="tof" id="s3"><b>-</b><span>RF</span></div>
   <div class="tof" id="s2"><b>-</b><span>LR</span></div>
   <div class="tof" id="s4"><b>-</b><span>RR</span></div>
  </div>
  <div class="kv" id="lkv"></div>
  <div class="row"><button onclick="cmd('zeroyaw')">Zero yaw</button><button onclick="cmd('zeroenc')">Zero encoders</button>
   <span class="small">Green edge = wall seen (F / L / R). Small numbers = raw before calibration.</span></div>
 </div>
 <div class="card"><h2>Checks before tuning</h2><ul class="note">
  <li>Cover one ToF at a time with a card: only that value may drop. If another one moves, the XSHUT pins in pins.h do not match the wiring.</li>
  <li>Turn the mouse LEFT by hand: yaw must go positive, about +90 after a quarter turn. Negative means GYRO_Z_INVERTED 1 in sensors.h.</li>
  <li>Centre the mouse between two walls: the four side values should agree within about 5 mm.</li></ul></div>
</section>

<section id="t-setup">
 <div class="card"><h2>Setup: do these once</h2><ol class="note">
  <li>Wheels off the floor: run the LEFT and RIGHT direction checks. The named wheel must roll forward and only its encoder should count.</li>
  <li>Distance: Import (if you saved values in the test sketch), or run the 900 mm encoder move on the floor, measure it, and enter the ruler distance.</li>
  <li>Save when done. Saved values come back after a reset.</li></ol></div>
 <div class="card"><h2>Direction check</h2>
  <div class="row"><span class="small">PWM</span><input id="dpwm" type="number" value="90"><span class="small">ms</span><input id="dms" type="number" value="400">
   <button class="pri" onclick="runDir(0)">Left wheel</button><button class="pri" onclick="runDir(1)">Right wheel</button></div>
  <div class="res" id="r-dir"></div></div>
 <div class="card"><h2>Distance calibration</h2>
  <div class="row"><span class="small">Drive</span><input id="emm" type="number" value="900"><span class="small">mm on encoders only</span><button class="pri" onclick="run('enc',{n:v('emm')})">Run</button></div>
  <div class="row"><span class="small">Ruler said</span><input id="rmm" type="number" placeholder="mm"><button onclick="cmd('calcpr',v('rmm'))">Correct counts per rev</button>
   <button onclick="cmd('import')">Import from test sketch</button></div>
  <div class="res" id="r-enc"></div></div>
 <div class="card"><h2>Values</h2><div class="params" data-tab="setup"></div></div>
</section>

<section id="t-straight">
 <div class="card"><h2>Straight moves</h2><ol class="note">
  <li>Start at a cell centre, no side walls, a long open row ahead.</li>
  <li>1 cell: adjust CELL_DISTANCE_MM until it stops on the next cell centre (within 5 mm).</li>
  <li>5 cells: heading at rest under 3 deg and L / R mm within 2%. Drifts: raise KP_GYRO_HEADING. Wobbles: lower it.</li></ol>
  <div class="row"><button class="pri" onclick="run('straight',{n:1})">1 cell</button><button class="pri" onclick="run('straight',{n:3})">3 cells</button><button class="pri" onclick="run('straight',{n:5})">5 cells</button><span class="small" id="dlylbl"></span></div>
  <div class="res" id="r-straight"></div>
  <canvas id="c-straight1"></canvas><canvas id="c-straight2"></canvas></div>
 <div class="card"><h2>Values</h2><div class="params" data-tab="straight"></div></div>
</section>

<section id="t-turns">
 <div class="card"><h2>Turns</h2><ol class="note">
  <li>Run 4 x left and 4 x right on the open floor. Aim for 90 +/- 2 deg every time.</li>
  <li>The summary suggests a new stop angle for that side: tap Apply and repeat.</li>
  <li>Angles scatter: lower TURN_SLOW_PWM or TURN_SLOW_START_DEG. Keep the peak rate under 450 deg/s.</li></ol>
  <div class="row"><button class="pri" onclick="run('turnL',{n:1})">Left x1</button><button class="pri" onclick="run('turnR',{n:1})">Right x1</button>
   <button class="pri" onclick="run('turnL',{n:4})">Left x4</button><button class="pri" onclick="run('turnR',{n:4})">Right x4</button><button class="pri" onclick="run('uturn')">U-turn</button></div>
  <div class="res" id="r-turns"></div>
  <canvas id="c-turns1"></canvas><canvas id="c-turns2"></canvas></div>
 <div class="card"><h2>Values</h2><div class="params" data-tab="turns"></div></div>
</section>

<section id="t-walls">
 <div class="card"><h2>Walls and centring</h2><ol class="note">
  <li>Centre the mouse between two walls, Capture, then "Use as target".</li>
  <li>Capture with a wall one cell ahead (Front: wall) and with none (Front: open): the threshold is set halfway.</li>
  <li>Capture with the nose touching a wall: FRONT_EMERGENCY_MM must be above that reading, or it only fires after contact.</li>
  <li>Corridor test from 10 mm off centre: raise KP_SIDE_LATERAL until it centres within 2 cells; add KP_SIDE_ANGLE if it swings.</li></ol>
  <div class="row"><button class="pri" onclick="run('capture',{n:30,d:0})">Capture 30 samples</button>
   <button class="pri" onclick="run('straight',{n:3})">Corridor 3 cells</button><button class="pri" onclick="run('straight',{n:5})">Corridor 5 cells</button></div>
  <div class="res" id="r-capture"></div>
  <div class="res" id="r-corr"></div>
  <canvas id="c-walls1"></canvas><canvas id="c-walls2"></canvas></div>
 <div class="card"><h2>Values</h2><div class="params" data-tab="walls"></div></div>
</section>

<section id="t-maze">
 <div class="card"><h2>Maze runs</h2><ol class="note">
  <li>Sequence: F forward, L / R turn, U U-turn. FFRFFRFFRFFR drives a square.</li>
  <li>Explore follows open walls for N steps. Full maze runs the real floodfill: search, return, fast runs.</li>
  <li>The map shows what the mouse believed. Compare it with the real maze to find wall-detection errors.</li></ol>
  <div class="row"><input id="seq" class="wide" value="FFRFFRFFRFFR"><button class="pri" onclick="run('seq',{s:$('#seq').value})">Run sequence</button></div>
  <div class="row"><button class="pri" onclick="run('follow',{n:10})">Explore 10 steps</button><button class="warn" onclick="run('maze')">Full maze run</button></div>
  <div class="res" id="r-maze"></div>
  <canvas id="mazec"></canvas>
  <div class="small" id="mzinfo"></div>
  <div class="log" id="mzlog"></div>
  <canvas id="c-maze1"></canvas></div>
 <div class="card"><h2>Values</h2><div class="params" data-tab="maze"></div></div>
</section>

<section id="t-export">
 <div class="card"><h2>Hard-code the values</h2><ol class="note">
  <li>Copy the block below.</li>
  <li>In mouse-firmware.ino, replace everything from BEGIN TUNED VALUES to END TUNED VALUES with it.</li>
  <li>For the competition set TUNER_WIFI 0 (rules: radio off) and upload before the inspection.</li></ol>
  <div class="row"><button class="pri" onclick="copyExport()">Copy</button><button onclick="loadExport()">Refresh</button>
   <button onclick="cmd('save')">Save to mouse</button><button class="warn" onclick="if(confirm('Discard all changes and go back to the TUNED VALUES block?'))cmd('defaults')">Back to TUNED VALUES block</button></div>
  <div class="small" id="chg"></div>
  <textarea id="exp" readonly wrap="off"></textarea></div>
 <div class="card"><h2>Debug log</h2>
  <div class="row"><button class="pri" onclick="downloadLog()">Download log</button>
   <span class="small">Values, live state, last result, maze log and trace in one text file</span></div></div>
 <div class="card"><h2>Start delay</h2><div class="row"><span class="small">Seconds between pressing a test and the mouse moving</span><input id="dly" type="number" value="2" min="0" max="10" step="0.5"></div></div>
</section>
</main>
<div id="toast"></div>
<script>
const $=(s,e=document)=>e.querySelector(s);
const TABS=[['live','Live'],['setup','Setup'],['straight','Straight'],['turns','Turns'],['walls','Walls'],['maze','Maze'],['export','Export']];
let P=[],S={},resSeq=-1,runTab='',wasBusy=false,online=false,capMarks={};
let tab='live';
try{tab=localStorage.getItem('mm-tab')||'live'}catch(e){}
try{const d=localStorage.getItem('mm-dly');if(d!==null)$('#dly').value=d}catch(e){}

function toast(t,ms){const e=$('#toast');e.textContent=t;e.style.display='block';clearTimeout(e._t);e._t=setTimeout(()=>e.style.display='none',ms||2600)}
function v(id){return parseFloat($('#'+id).value)}
function dec(st){if(st>=1)return 0;return Math.min(4,Math.max(0,Math.ceil(-Math.log10(st)-1e-9)))}
function fmt(x,st){return Number(x).toFixed(dec(st))}
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
async function api(path,text){
 const c=new AbortController();const t=setTimeout(()=>c.abort(),3500);
 try{const r=await fetch(path,{signal:c.signal,cache:'no-store'});if(!r.ok)throw new Error(r.status);return text?await r.text():await r.json()}
 finally{clearTimeout(t)}}

function showTab(t){tab=t;try{localStorage.setItem('mm-tab',t)}catch(e){}
 for(const [id] of TABS){$('#t-'+id).classList.toggle('on',id===t);$('#b-'+id).classList.toggle('on',id===t)}
 if(t==='export')loadExport();if(t==='maze')pollMaze();redrawCharts()}
function buildTabs(){const n=$('#tabs');n.innerHTML='';for(const [id,l] of TABS){const b=document.createElement('button');b.id='b-'+id;b.textContent=l;b.onclick=()=>showTab(id);n.appendChild(b)}showTab(TABS.some(x=>x[0]===tab)?tab:'live')}

// ---------------- parameters ----------------
function pById(id){return P.find(p=>p.id===id)}
function paramHtml(p){
 const k=p.k,id=p.id;let ctl;
 if(k===2){ctl=`<div class="seg"><button data-v="0">0</button><button data-v="1">1</button></div><span class="u">${esc(p.u)}</span>`}
 else if(k===3){const o=(p.u||'').split(',').map(s=>s.trim().replace(/^\d+\s*/,''));ctl=`<div class="seg">${o.map((l,i)=>`<button data-v="${i}">${esc(l)}</button>`).join('')}</div>`}
 else{ctl=`<button data-d="-1">-</button><input type="range" min="${p.lo}" max="${p.hi}" step="${p.st}"><button data-d="1">+</button><input type="number" step="${p.st}"><span class="u">${esc(p.u)}</span>`}
 return `<div class="prm lock" id="p-${id}"><div class="ph"><span class="pid">${id}</span><span class="pdef"></span><button class="ed">edit</button></div><div class="pc">${ctl}</div><div class="help">${esc(p.h)}</div></div>`}
// Controls stay locked until "edit" is pressed, so scrolling the page over a
// slider cannot change a value. One parameter is open at a time; it locks
// again on "done" or 20 s after the last change.
let edId=null,edT=null;
function lockParam(){clearTimeout(edT);const el=edId&&$('#p-'+edId);if(el){el.classList.add('lock');el.querySelector('.ed').textContent='edit'}edId=null}
function touchParam(){clearTimeout(edT);edT=setTimeout(lockParam,20000)}
function editParam(id){const was=edId;lockParam();if(was===id)return;const el=$('#p-'+id);if(!el)return;
 edId=id;el.classList.remove('lock');el.querySelector('.ed').textContent='done';touchParam()}
// The mouse wheel changes a focused number box: drop the focus instead.
document.addEventListener('wheel',e=>{const a=document.activeElement;if(a&&a.type==='number'&&e.target===a)a.blur()},{passive:true});
function buildParams(){
 clearTimeout(edT);edId=null;
 document.querySelectorAll('.params').forEach(box=>{
  const t=box.dataset.tab;let h='',g='';
  for(const p of P.filter(p=>p.tab===t)){if(p.grp!==g){g=p.grp;h+=`<h3>${esc(g)}</h3>`}h+=paramHtml(p)}
  box.innerHTML=h});
 for(const p of P){const el=$('#p-'+p.id);if(!el)continue;
  el.querySelector('.ed').onclick=()=>editParam(p.id);
  el.querySelectorAll('.seg button').forEach(b=>b.onclick=()=>{touchParam();setParam(p.id,parseFloat(b.dataset.v))});
  const r=el.querySelector('input[type=range]'),n=el.querySelector('input[type=number]');
  if(r){r.oninput=()=>{touchParam();n.value=fmt(r.value,p.st)};r.onchange=()=>setParam(p.id,parseFloat(r.value));
   n.onchange=()=>{touchParam();setParam(p.id,parseFloat(n.value))};
   el.querySelectorAll('button[data-d]').forEach(b=>b.onclick=()=>{touchParam();setParam(p.id,p.v+parseFloat(b.dataset.d)*p.st)})}
  showParam(p)}}
function showParam(p){const el=$('#p-'+p.id);if(!el)return;
 const r=el.querySelector('input[type=range]'),n=el.querySelector('input[type=number]');
 if(r){r.value=p.v;if(document.activeElement!==n)n.value=fmt(p.v,p.st)}
 el.querySelectorAll('.seg button').forEach(b=>b.classList.toggle('on',parseFloat(b.dataset.v)===p.v));
 const d=el.querySelector('.pdef'),chg=Math.abs(p.v-p.d)>p.st/2;
 d.classList.toggle('chg',chg);d.textContent='block '+fmt(p.d,p.st)+(chg?'  (changed)':'')}
async function setParam(id,val){const p=pById(id);if(!p||isNaN(val))return;
 val=Math.min(p.hi,Math.max(p.lo,val));p.v=val;showParam(p);
 try{const r=await api(`/api/set?id=${id}&v=${val}`);if(r.ok){p.v=r.v;showParam(p)}}catch(e){toast('not sent: no connection')}}
async function loadParams(){try{const r=await api('/api/params');P=r.params;buildParams();if(r.flash)toast('Loaded the values saved on the mouse')}
 catch(e){setTimeout(loadParams,2000)}}

// ---------------- commands ----------------
function delay(){const d=parseFloat($('#dly').value);return isNaN(d)?2:d}
async function run(t,o){o=o||{};runTab=tab;
 const q=new URLSearchParams({t});for(const k in o)q.set(k,o[k]);if(!('d' in o))q.set('d',delay());
 try{const r=await api('/api/run?'+q.toString());if(!r.ok)toast('Busy: wait or press STOP')}catch(e){toast('not sent: no connection')}}
function runDir(m){run('dir',{n:m,p:v('dpwm'),ms:v('dms')})}
async function cmd(c,n){runTab=tab;try{const r=await api(`/api/cmd?c=${c}`+(n!==undefined?`&n=${n}`:''));if(!r.ok)toast('Busy: wait or press STOP')}catch(e){toast('not sent: no connection')}}
$('#stop').onclick=async()=>{try{await api('/api/stop')}catch(e){}toast('STOP sent')};
$('#save').onclick=()=>cmd('save');
$('#dly').onchange=()=>{try{localStorage.setItem('mm-dly',$('#dly').value)}catch(e){}showDelay()};
function showDelay(){$('#dlylbl').textContent='starts '+delay()+' s after you press'}

// ---------------- live state ----------------
function mm(x){return x>=8190?'--':x}
function renderState(s){
 const m=$('#msg');m.textContent=s.busy?('RUNNING '+s.task+': '+s.msg):(s.sens?s.msg:'ToF / MPU6050 not started: '+s.msg);m.classList.toggle('busy',!!s.busy);
 $('#save').classList.toggle('dirty',!!s.dirty);$('#save').textContent=s.dirty?'Save *':'Save';
 for(let i=0;i<5;i++){const e=$('#s'+i);e.querySelector('b').textContent=mm(s.tof[i]);e.querySelector('span').textContent=['F','LF','LR','RF','RR'][i]+'  raw '+mm(s.raw[i])}
 const b=$('#mbody');b.classList.toggle('wf',!!s.wall[0]);b.classList.toggle('wl',!!s.wall[1]);b.classList.toggle('wr',!!s.wall[2]);
 $('#lyaw').textContent=s.yaw.toFixed(1);
 const cpr=(pById('ENCODER_COUNTS_PER_REV')||{v:660}).v,dia=(pById('WHEEL_DIAMETER_MM')||{v:32}).v,k=Math.PI*dia/cpr;
 const kv=[['lateral error',s.lat.toFixed(1)+' mm'],['angle error',s.ang.toFixed(1)+' deg'],['turn rate',s.rate+' deg/s'],
  ['encoders L / R',s.enc[0]+' / '+s.enc[1]],['distance L / R',(Math.abs(s.enc[0])*k).toFixed(0)+' / '+(Math.abs(s.enc[1])*k).toFixed(0)+' mm'],
  ['PWM L / R',s.pwm[0]+' / '+s.pwm[1]],['side clearance',mm(s.lmin)+' / '+mm(s.rmin)+' mm'],['uptime / heap',s.up+' s / '+Math.round(s.heap/1024)+' k']];
 $('#lkv').innerHTML=kv.map(x=>`<div><b>${x[1]}</b><span>${x[0]}</span></div>`).join('')}
async function poll(){
 try{const s=await api('/api/state');S=s;online=true;$('#dot').classList.add('on');renderState(s);
  if(s.res!==resSeq){const first=resSeq<0;resSeq=s.res;if(!first)loadResult()}
  if(wasBusy&&!s.busy)onRunEnded();wasBusy=!!s.busy;
  if(!P.length)loadParams()}
 catch(e){online=false;$('#dot').classList.remove('on');$('#msg').textContent='no connection to the mouse (join its WiFi)'}
 setTimeout(poll,document.hidden?1500:300)}
function onRunEnded(){loadTrace();if(tab==='maze')pollMaze()}

// ---------------- results ----------------
async function loadResult(){try{const j=await api('/api/result');showResult(j.r)}catch(e){}}
function cls(ok){return ok?'ok':'bad'}
function stepTable(rows){
 let h='<table><tr><th>#</th><th class="l">step</th><th>ok</th><th class="l">result</th><th>ms</th><th class="l">why</th></tr>';
 for(const r of rows){const t=r.c==='F';
  const res=t?`L ${r.mmL} / R ${r.mmR} mm, heading ${r.yaw.toFixed(1)} deg, lat ${r.lat} mm, min side ${mm(r.lmin)} / ${mm(r.rmin)}, front ${mm(r.front)}`
             :`<b>${r.deg.toFixed(1)} deg</b>, peak ${r.rate} deg/s`;
  h+=`<tr><td>${r.i}</td><td class="l">${t?'cell':(r.c==='L'?'turn L':'turn R')}</td><td class="${cls(r.ok)}">${r.ok?'yes':'NO'}</td><td class="r1">${res}</td><td>${r.ms}</td><td class="why">${esc(r.why||'')}</td></tr>`}
 return h+'</table>'}
function turnSummary(rows){
 let h='';for(const side of ['L','R']){const a=rows.filter(r=>r.c===side&&r.ok).map(r=>r.deg);if(!a.length)continue;
  const avg=a.reduce((x,y)=>x+y,0)/a.length,mn=Math.min(...a),mx=Math.max(...a),pk=Math.max(...rows.filter(r=>r.c===side).map(r=>r.rate));
  const id=side==='L'?'TURN_STOP_DEG_LEFT':'TURN_STOP_DEG_RIGHT',p=pById(id),sug=p?Math.round((p.v+(90-avg))*10)/10:null;
  h+=`<div class="sum"><span class="chip">${side==='L'?'LEFT':'RIGHT'}</span> average <b>${avg.toFixed(1)} deg</b>, spread ${mn.toFixed(1)} to ${mx.toFixed(1)}, peak ${pk} deg/s`+
   (pk>450?'<div class="bad">Peak over 450 deg/s: near the gyro limit (500). Lower TURN_PWM.</div>':'')+
   (mx-mn>4?'<div class="wa">Spread over 4 deg: slow the end of the turn (TURN_SLOW_PWM, TURN_SLOW_START_DEG).</div>':'')+
   (p&&Math.abs(90-avg)>0.4?`<div class="row"><button onclick="setParam('${id}',${sug})">Apply ${id} = ${sug}</button></div>`:'<div class="ok">On target.</div>')+'</div>'}
 return h}
function straightSummary(rows){const c=rows.filter(r=>r.c==='F'&&r.ok);if(!c.length)return'';
 const L=c.reduce((x,r)=>x+r.mmL,0),R=c.reduce((x,r)=>x+r.mmR,0),d=(L-R)/((L+R)/2)*100,hd=c[c.length-1].yaw;
 return `<div class="line small">avg per cell ${((L+R)/2/c.length).toFixed(1)} mm (encoders), L-R ${d.toFixed(1)}%${Math.abs(d)>2?' <span class="wa">over 2%</span>':''}, heading after last cell ${hd.toFixed(1)} deg${Math.abs(hd)>3?' <span class="wa">over 3</span>':''}</div>`}
function dirDiag(r){
 const own=r.motor==='LEFT'?r.cL:r.cR,other=r.motor==='LEFT'?r.cR:r.cL,flag=r.motor==='LEFT'?'MOTOR_A_INVERTED':'MOTOR_B_INVERTED',p=pById(flag);
 let d=`<div class="line">${r.motor} motor at PWM ${r.pwm} for ${r.ms} ms: left encoder <b>${r.cL}</b>, right encoder <b>${r.cR}</b></div>`;
 if(Math.abs(own)<20&&Math.abs(other)<20)d+='<div class="bad">No counts at all: motor not powered (STBY, battery) or encoder wiring.</div>';
 else if(Math.abs(other)>Math.abs(own))d+=`<div class="bad">The OTHER wheel's encoder counted: encoders A and B are swapped (pins.h) or the motors are. Fix before any tuning; set KP_ENCODER_SYNC to 0 meanwhile.</div>`;
 else d+=`<div class="ok">The ${r.motor.toLowerCase()} encoder counted.</div>`+(own<0?`<div class="wa">Its count went NEGATIVE: either the wheel turned backwards (flip below) or only the encoder sign is reversed, which is harmless because moves use absolute counts.</div>`:'');
 if(p)d+=`<div class="line">Did the ${r.motor} wheel roll FORWARD?</div><div class="row"><button onclick="toast('Good')">Yes</button><button class="warn" onclick="setParam('${flag}',${p.v?0:1});toast('${flag} flipped: run it again')">No, backwards: flip ${flag}</button></div>`;
 return d}
function capTable(rows){
 let h='<table><tr><th class="l">sensor</th><th>mm</th><th>raw</th><th>min</th><th>max</th><th>n</th></tr>';
 for(const r of rows)h+=r.n?`<tr><td class="l">${r.s}</td><td><b>${r.cal.toFixed(1)}</b></td><td>${r.raw.toFixed(1)}</td><td>${r.min}</td><td>${r.max}</td><td>${r.n}</td></tr>`:`<tr><td class="l">${r.s}</td><td colspan="5" class="wa">no valid reading (nothing in range)</td></tr>`;
 h+='</table>';const side=rows.slice(1).filter(r=>r.n);
 if(side.length===4){const avg=side.reduce((x,r)=>x+r.cal,0)/4,spread=Math.max(...side.map(r=>r.cal))-Math.min(...side.map(r=>r.cal));
  h+=`<div class="line">Side average ${avg.toFixed(1)} mm, spread ${spread.toFixed(1)} mm.`+(spread>12?' <span class="wa">Spread over 12 mm: the mouse is not centred and square, or a sensor needs a trim. Fix that and capture again.</span></div>':(spread>6?' <span class="wa">Over 6: check centring or trims.</span>':'')+`</div><div class="row"><button onclick="setParam('TARGET_SIDE_DISTANCE_MM',${avg.toFixed(1)})">Use ${avg.toFixed(1)} as TARGET_SIDE_DISTANCE_MM</button></div>`)}
 const f=rows[0];
 h+=`<div class="line">Front reading:</div><div class="row"><button onclick="markCap('fw',${f.n?f.cal:9999})">a WALL one cell ahead</button><button onclick="markCap('fo',${f.n?f.cal:9999})">OPEN ahead</button><button onclick="markCap('ft',${f.n?f.cal:9999})">nose TOUCHING a wall</button></div>`;
 if(side.length)h+=`<div class="line">Side readings:</div><div class="row"><button onclick="markCap('sw',${Math.max(...side.map(r=>r.cal))})">both sides are WALLS</button><button onclick="markCap('so',${rows.slice(1).map(r=>r.n?r.cal:9999).reduce((a,b)=>Math.min(a,b))})">both sides are OPEN</button></div>`;
 else h+=`<div class="line">Side readings:</div><div class="row"><button onclick="markCap('so',9999)">both sides are OPEN</button></div>`;
 return h+'<div id="capsug"></div>'}
function markCap(k,x){capMarks[k]=x;showCapSug()}
function showCapSug(){let h='';const m=capMarks;
 const sug=(a,b,id,lo)=>{const mid=Math.round(Math.min((a+Math.min(b,300))/2,lo));return `<button onclick="setParam('${id}',${mid})">Set ${id} = ${mid}</button>`};
 if(m.fw!==undefined&&m.fo!==undefined)h+=`<div class="line">Front wall ${m.fw.toFixed(0)} mm, open ${m.fo>=9999?'out of range':m.fo.toFixed(0)+' mm'} ${sug(m.fw,m.fo,'FRONT_WALL_THRESHOLD_MM',250)}</div>`;
 if(m.sw!==undefined&&m.so!==undefined)h+=`<div class="line">Side wall (farthest) ${m.sw.toFixed(0)} mm, open (nearest) ${m.so>=9999?'out of range':m.so.toFixed(0)+' mm'} ${sug(m.sw,m.so,'SIDE_WALL_THRESHOLD_MM',200)}</div>`;
 if(m.ft!==undefined&&m.ft<9999){const em=Math.round(m.ft+5),cw=m.fw;
  h+=`<div class="line">Nose touching: front reads ${m.ft.toFixed(0)} mm, so FRONT_EMERGENCY_MM below that only fires after contact.`+
   (cw!==undefined&&cw<9999&&em>=cw-8?` <span class="wa">Careful: ${em} is close to the centred-wall reading ${cw.toFixed(0)}; a small overshoot would end the run.</span>`:'')+
   `</div><div class="row"><button onclick="setParam('FRONT_EMERGENCY_MM',${em})">Set FRONT_EMERGENCY_MM = ${em}</button></div>`}
 const e=$('#capsug');if(e)e.innerHTML=h}
function showResult(r){if(!r||r.type==='none')return;const t=r.type,rows=r.rows||[];let box=null,h='';
 if(t==='straight'||t==='seq'){box=runTab==='walls'?'#r-corr':(runTab==='maze'?'#r-maze':'#r-straight');h=stepTable(rows)+straightSummary(rows)+turnSummary(rows)}
 else if(t==='turns'){box='#r-turns';h=stepTable(rows)+turnSummary(rows)}
 else if(t==='dir'){box='#r-dir';h=rows.length?dirDiag(rows[0]):''}
 else if(t==='enc'){box='#r-enc';const x=rows[0];if(x)h=`<div class="line">Counts L ${x.cL}, R ${x.cR}, average ${x.avg}. With the current values that is <b>${x.mm} mm</b> (target ${x.target}).</div>`}
 else if(t==='cpr'){box='#r-enc';const x=rows[0];if(x)h=`<div class="line ok">ENCODER_COUNTS_PER_REV ${x.before} -> <b>${x.after}</b> (ruler ${x.mm} mm)</div>`;loadParams()}
 else if(t==='capture'){box='#r-capture';h=capTable(rows)}
 else if(t==='maze'){box='#r-maze';const x=rows[0];if(x)h=`<div class="line">${x.moves} cells, ${x.turns} turns in ${x.s} s; ended at (${x.x},${x.y})</div>`;pollMaze()}
 else if(t==='import'){box='#r-enc';h=rows.map(x=>`<div class="small">${x.id} = ${x.v}</div>`).join('');loadParams()}
 else if(t==='defaults'||t==='saved'){loadParams();if(tab==='export')loadExport()}
 if(r.note)toast(r.note,4000);
 if(box){$(box).innerHTML=h+(r.note?`<div class="small">${esc(r.note)}</div>`:'');}}

// ---------------- trace charts ----------------
let T=null;
async function loadTrace(){try{const txt=await api('/api/trace',true);const L=txt.trim().split('\n');const hd=L.shift().split(',');
 const cols={};hd.forEach((h,i)=>cols[h]=[]);for(const line of L){const f=line.split(',');hd.forEach((h,i)=>cols[h].push(parseFloat(f[i])))}
 T=cols.t_ms&&cols.t_ms.length?cols:null;redrawCharts()}catch(e){}}
function redrawCharts(){if(!T)return;const t=T.t_ms.map(x=>x/1000),cpr=(pById('ENCODER_COUNTS_PER_REV')||{v:660}).v,dia=(pById('WHEEL_DIAMETER_MM')||{v:32}).v,k=Math.PI*dia/cpr;
 const rate=T.yaw_deg.map((y,i)=>i?(y-T.yaw_deg[i-1])/Math.max(0.001,t[i]-t[i-1]):0).map((r,i)=>T.step[i]!==T.step[i-1]?0:r);
 const dLR=T.encL.map((l,i)=>(Math.abs(l)-Math.abs(T.encR[i]))*k),lat=T.lat_mm,ang=T.ang_deg;
 const lmin=T.lmin.map(x=>x>=8190?NaN:x),rmin=T.rmin.map(x=>x>=8190?NaN:x);
 const sd=(pById('SIDE_DANGER_MM')||{v:30}).v,hs=(pById('SIDE_HARD_STOP_MM')||{v:18}).v;
 if(tab==='straight'){chart('#c-straight1',t,[['yaw deg','#45b8ff',T.yaw_deg],['L-R mm','#f2b33d',dLR]]);chart('#c-straight2',t,[['PWM L','#39c98a',T.pwmL],['PWM R','#ff5a5f',T.pwmR]])}
 if(tab==='turns'){chart('#c-turns1',t,[['yaw deg','#45b8ff',T.yaw_deg]],[[90,'90'],[-90,'-90']]);chart('#c-turns2',t,[['rate deg/s','#f2b33d',rate],['PWM L','#39c98a',T.pwmL],['PWM R','#ff5a5f',T.pwmR]],[[450,'450'],[-450,'-450']])}
 if(tab==='walls'){chart('#c-walls1',t,[['lateral mm','#45b8ff',lat],['angle deg','#f2b33d',ang]]);chart('#c-walls2',t,[['left min mm','#39c98a',lmin],['right min mm','#ff5a5f',rmin]],[[sd,'danger'],[hs,'hard stop']])}
 if(tab==='maze'){chart('#c-maze1',t,[['lateral mm','#45b8ff',lat],['yaw deg','#f2b33d',T.yaw_deg],['left min','#39c98a',lmin],['right min','#ff5a5f',rmin]])}}
function chart(sel,x,series,hl){const c=$(sel);if(!c||!c.offsetWidth)return;const dpr=window.devicePixelRatio||1,W=c.offsetWidth,H=c.offsetHeight;
 c.width=W*dpr;c.height=H*dpr;const g=c.getContext('2d');g.scale(dpr,dpr);g.clearRect(0,0,W,H);
 let lo=Infinity,hi=-Infinity;for(const s of series)for(const y of s[2])if(isFinite(y)){lo=Math.min(lo,y);hi=Math.max(hi,y)}
 for(const h of hl||[]){if(h[0]>=lo-Math.abs(hi-lo)*0.3&&h[0]<=hi+Math.abs(hi-lo)*0.3){lo=Math.min(lo,h[0]);hi=Math.max(hi,h[0])}}
 if(!isFinite(lo)){lo=-1;hi=1}if(hi-lo<1e-6){hi+=1;lo-=1}const pad=(hi-lo)*0.08;lo-=pad;hi+=pad;
 const x0=x[0]||0,x1=x[x.length-1]||1,L=40,R=8,Tp=18,B=18,px=q=>L+(q-x0)/Math.max(1e-6,x1-x0)*(W-L-R),py=q=>Tp+(hi-q)/(hi-lo)*(H-Tp-B);
 g.font='11px system-ui';g.strokeStyle='#2b323d';g.fillStyle='#98a2b0';g.lineWidth=1;
 for(let i=0;i<=4;i++){const q=lo+(hi-lo)*i/4,y=py(q);g.beginPath();g.moveTo(L,y);g.lineTo(W-R,y);g.stroke();g.fillText(q.toFixed(Math.abs(hi-lo)<10?1:0),2,y+4)}
 g.fillText(x0.toFixed(1)+' s',L,H-4);g.fillText(x1.toFixed(1)+' s',W-R-40,H-4);
 if(T&&T.step){g.strokeStyle='#39414d';g.setLineDash([3,4]);for(let i=1;i<T.step.length;i++)if(T.step[i]!==T.step[i-1]){const X=px(x[i]);g.beginPath();g.moveTo(X,Tp);g.lineTo(X,H-B);g.stroke()}g.setLineDash([])}
 for(const h of hl||[]){if(h[0]<lo||h[0]>hi)continue;g.strokeStyle='#6b4b12';g.beginPath();g.moveTo(L,py(h[0]));g.lineTo(W-R,py(h[0]));g.stroke();g.fillStyle='#f2b33d';g.fillText(h[1],W-R-60,py(h[0])-3)}
 let lx=L;for(const s of series){g.strokeStyle=s[1];g.lineWidth=1.6;g.beginPath();let pen=false;
  s[2].forEach((y,i)=>{if(!isFinite(y)){pen=false;return}const X=px(x[i]),Y=py(y);if(pen)g.lineTo(X,Y);else g.moveTo(X,Y);pen=true});g.stroke();
  g.fillStyle=s[1];g.fillText(s[0],lx,12);lx+=g.measureText(s[0]).width+14}}
window.addEventListener('resize',redrawCharts);

// ---------------- maze ----------------
let mzTimer=null;
async function pollMaze(){clearTimeout(mzTimer);if(tab!=='maze')return;
 try{const m=await api('/api/maze');drawMaze(m);$('#mzlog').textContent=m.log.slice().reverse().join('\n');
  $('#mzinfo').textContent=`${m.n||16}x${m.n||16} | at (${m.x},${m.y}) facing ${'NESW'[m.h]} | ${m.moves} cells, ${m.turns} turns | maze opens ${m.mir<0?'undecided (AUTO)':(m.mir?'LEFT (drawn mirrored)':'RIGHT')}`}catch(e){}
 mzTimer=setTimeout(pollMaze,S.busy?500:2000)}
// Draws the MAZE_SIZE grid (n x n); the outer border is always a wall and the
// goal 2x2 is where MAZE_SIZE / GOAL_* put it (gx, gy = its corner nearest the start).
function drawMaze(m){const c=$('#mazec'),dpr=window.devicePixelRatio||1,W=c.offsetWidth;if(!W)return;c.width=W*dpr;c.height=W*dpr;
 const n=m.n||16,gx=m.gx!==undefined?m.gx:n/2-1,gy=m.gy!==undefined?m.gy:n/2-1;
 const g=c.getContext('2d');g.scale(dpr,dpr);g.clearRect(0,0,W,W);const pad=10,cs=(W-2*pad)/n,mir=m.mir===1;
 const X=x=>pad+(mir?n-1-x:x)*cs,Y=y=>pad+(n-1-y)*cs;
 g.fillStyle='rgba(69,184,255,.12)';g.fillRect(X(mir?gx+1:gx),Y(gy+1),cs*2,cs*2);g.fillStyle='rgba(57,201,138,.18)';g.fillRect(X(0),Y(0),cs,cs);
 const cell=(x,y)=>parseInt(m.w.substr((y*16+x)*2,2),16);
 for(let y=0;y<n;y++)for(let x=0;x<n;x++){const b=cell(x,y);
  for(let d=0;d<4;d++){if(d===2&&y>0)continue;if(d===3&&x>0)continue;
   const edge=(d===0&&y===n-1)||(d===1&&x===n-1)||(d===2&&y===0)||(d===3&&x===0);
   const known=edge||(b&(1<<d)),wall=edge||(b&(16<<d));let dd=d;if(mir&&(d===1||d===3))dd=4-d;
   const x0=X(x),y0=Y(y),seg=[[x0,y0,x0+cs,y0],[x0+cs,y0,x0+cs,y0+cs],[x0,y0+cs,x0+cs,y0+cs],[x0,y0,x0,y0+cs]][dd];
   if(known&&!wall)continue;g.beginPath();g.moveTo(seg[0],seg[1]);g.lineTo(seg[2],seg[3]);
   if(wall){g.strokeStyle='#e7eaef';g.lineWidth=3;g.setLineDash([])}else{g.strokeStyle='#2b323d';g.lineWidth=1;g.setLineDash([2,3])}g.stroke()}}
 g.setLineDash([]);const cx=X(m.x)+cs/2,cy=Y(m.y)+cs/2;let h=m.h;if(mir&&(h===1||h===3))h=4-h;const a=[-Math.PI/2,0,Math.PI/2,Math.PI][h];
 g.fillStyle='#f2b33d';g.beginPath();g.moveTo(cx+Math.cos(a)*cs*0.38,cy+Math.sin(a)*cs*0.38);g.lineTo(cx+Math.cos(a+2.4)*cs*0.3,cy+Math.sin(a+2.4)*cs*0.3);g.lineTo(cx+Math.cos(a-2.4)*cs*0.3,cy+Math.sin(a-2.4)*cs*0.3);g.fill()}

// ---------------- export ----------------
async function loadExport(){try{$('#exp').value=await api('/api/export',true);
 const ch=P.filter(p=>Math.abs(p.v-p.d)>p.st/2).map(p=>`${p.id} ${fmt(p.d,p.st)} -> ${fmt(p.v,p.st)}`);
 $('#chg').textContent=ch.length?('Changed from the block: '+ch.join(', ')):'No changes from the TUNED VALUES block.'}catch(e){}}
async function copyExport(){await loadExport();const t=$('#exp');
 try{await navigator.clipboard.writeText(t.value);toast('Copied')}catch(e){t.focus();t.select();try{document.execCommand('copy');toast('Copied')}catch(e2){toast('Select the text and copy it by hand')}}}

// ---------------- debug log ----------------
// One request at a time: the mouse's web server serves clients one by one.
async function downloadLog(){
 try{const s=await api('/api/state'),ex=await api('/api/export',true),res=await api('/api/result'),mz=await api('/api/maze'),tr=await api('/api/trace',true);
  const t=new Date(),stamp=t.toISOString().replace(/[:T]/g,'-').slice(0,19);
  const map={x:mz.x,y:mz.y,h:mz.h,mir:mz.mir,moves:mz.moves,turns:mz.turns,n:mz.n,gx:mz.gx,gy:mz.gy,w:mz.w};
  const txt=['# micromouse log '+t.toString(),'','[state]',JSON.stringify(s),'','[values]',ex.trim(),'','[result]',JSON.stringify(res.r),
   '','[maze]',JSON.stringify(map),'','[maze log]',...mz.log,'','[trace]',tr.trim(),''].join('\n');
  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([txt],{type:'text/plain'}));a.download='micromouse-log-'+stamp+'.txt';
  document.body.appendChild(a);a.click();setTimeout(()=>{URL.revokeObjectURL(a.href);a.remove()},1000);toast('Log downloaded')}
 catch(e){toast('Log failed: no connection to the mouse')}}

buildTabs();showDelay();loadParams();poll();
</script>
</body></html>
)TBPAGE";

#endif // TUNER_PAGE_H
