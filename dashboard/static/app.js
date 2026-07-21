const PATTERNS=[['heartbeat','Heartbeat'],['demo','Demo rotation'],['ripple','Ripple'],['fireSpread','Fire spread'],['waterfall','Waterfall'],['twoToneDiffuse','Two-tone diffuse'],['colorwheel','Color wheel'],['bottomfill','Bottom fill'],['sensordemo','Sensor field'],['fallingRain','Falling rain'],['movementSimulation','Movement simulation'],['innerSpreadWave','Inner spread wave'],['rainbowWave','Rainbow wave'],['crazy','Crazy'],['sparkle','Sparkle'],['none','None']];
const state={devices:[],installation:null,settings:null,platforms:[],events:[],selectedKey:null,settingsLoaded:false,dialogDirty:false};
const deviceDialog=document.getElementById('deviceDialog');
const toast=document.getElementById('toast');
function esc(value){return String(value??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}
function pct(value){return Math.round((Number(value)||0)*100)+'%';}
function num(value,digits=2){return Number(value||0).toFixed(digits);}
function showToast(value){toast.textContent=typeof value==='string'?value:JSON.stringify(value,null,2);toast.classList.add('show');clearTimeout(showToast.timer);showToast.timer=setTimeout(()=>toast.classList.remove('show'),5000);}
async function request(url,options={}){const response=await fetch(url,options);const data=await response.json().catch(()=>({ok:false,error:'Invalid response'}));if(!response.ok&&!data.error)data.error=`HTTP ${response.status}`;return data;}
async function postForm(url,formOrData){const body=formOrData instanceof FormData?formOrData:new FormData(formOrData);const data=await request(url,{method:'POST',body});showToast(data);return data;}
async function saveDeviceForm(url,formOrData){const data=await postForm(url,formOrData);if(data.ok){state.dialogDirty=false;await refresh();}return data;}
function setForm(form,values){for(const [name,value] of Object.entries(values||{})){const field=form.elements.namedItem(name);if(!field)continue;if(field.type==='checkbox')field.checked=Boolean(value);else field.value=value;}}
function patternOptions(current){return PATTERNS.map(([value,label])=>`<option value="${value}"${value===current?' selected':''}>${label}</option>`).join('');}
function polarToXZ(radius,azimuthDeg){const rad=azimuthDeg*Math.PI/180;return {x:radius*Math.sin(rad),z:radius*Math.cos(rad)};}
function xzToPolar(x,z){const radius=Math.hypot(x,z);let azimuth=Math.atan2(x,z)*180/Math.PI;if(azimuth<0)azimuth+=360;return {radius,azimuth};}

async function refresh(){
  try{
    const data=await request('/api/installation');
    state.devices=data.devices||[];state.installation=data.installation||{};state.settings=data.settings||{};state.platforms=data.platforms||[];state.events=data.events||[];
    render();
  }catch(error){showToast(String(error));}
}
function render(){
  const online=state.devices.filter(d=>d.online).length;
  document.getElementById('onlineCount').textContent=online;
  document.getElementById('fleetDot').className='dot '+(online?'on':'');
  document.getElementById('fleetState').textContent=online?`${online} controller${online===1?'':'s'} online`:'No controllers online';
  const installation=state.installation||{};const expected=state.settings?.expectedPlatformCount||0;
  document.getElementById('occupiedCount').textContent=`${installation.occupiedPlatforms||0} / ${expected}`;
  document.getElementById('meanCalm').textContent=pct(installation.meanCalmness);
  document.getElementById('sceneState').textContent=installation.allCalm?'CHORUS':String(state.settings?.mode||'forest').toUpperCase();
  document.getElementById('clockBadge').textContent=`Chorus ${pct(installation.chorus)}`;
  document.getElementById('audioBadge').textContent=state.settings?.audioEnabled?'OSC audio on':'Audio bridge off';
  if(!state.settingsLoaded&&state.settings){setForm(document.getElementById('showForm'),state.settings);state.settingsLoaded=true;}
  renderMap();renderDevices();renderEvents();
  if(state.selectedKey&&deviceDialog.open&&!state.dialogDirty)renderDialog(state.devices.find(d=>d.key===state.selectedKey));
}
function renderMap(){
  const canvas=document.getElementById('map'),ctx=canvas.getContext('2d'),w=canvas.width,h=canvas.height,s=state.settings||{};
  ctx.clearRect(0,0,w,h);const grad=ctx.createRadialGradient(w/2,h/2,20,w/2,h/2,w*.7);grad.addColorStop(0,'#123044');grad.addColorStop(1,'#050b11');ctx.fillStyle=grad;ctx.fillRect(0,0,w,h);
  ctx.strokeStyle='#17384a';ctx.lineWidth=1;for(let i=1;i<10;i++){ctx.beginPath();ctx.moveTo(i*w/10,0);ctx.lineTo(i*w/10,h);ctx.stroke();ctx.beginPath();ctx.moveTo(0,i*h/10);ctx.lineTo(w,i*h/10);ctx.stroke();}
  const minX=Number(s.mapMinX??-10),maxX=Number(s.mapMaxX??10),minZ=Number(s.mapMinZ??-10),maxZ=Number(s.mapMaxZ??10);
  // +Z is North; canvas y grows downward, so North is flipped to the top of the field.
  const xy=(x,z)=>[(x-minX)/(maxX-minX)*w,(maxZ-z)/(maxZ-minZ)*h];
  ctx.save();ctx.strokeStyle='#4d99b1';ctx.fillStyle='#a8cbdc';ctx.lineWidth=2;ctx.font='bold 13px system-ui';ctx.textAlign='center';
  ctx.beginPath();ctx.moveTo(26,40);ctx.lineTo(26,14);ctx.moveTo(18,22);ctx.lineTo(26,14);ctx.lineTo(34,22);ctx.stroke();
  ctx.fillText('N',26,56);ctx.restore();
  for(const p of state.platforms){const [x,y]=xy(p.x,p.z);const radius=(state.settings?.influenceRadiusMeters||5)/(maxX-minX)*w;const halo=ctx.createRadialGradient(x,y,4,x,y,Math.max(10,radius));halo.addColorStop(0,p.occupied?(p.agitation>.2?'#ff718477':'#ffe39a77'):'#ffe39a22');halo.addColorStop(1,'transparent');ctx.fillStyle=halo;ctx.beginPath();ctx.arc(x,y,Math.max(10,radius),0,Math.PI*2);ctx.fill();ctx.fillStyle=p.occupied?'#ffe39a':'#806f43';ctx.beginPath();ctx.arc(x,y,9,0,Math.PI*2);ctx.fill();ctx.fillStyle='#071017';ctx.font='bold 11px system-ui';ctx.textAlign='center';ctx.fillText(p.id,x,y+4);const device=state.devices.find(d=>d.hasScale&&Number(d.id)===Number(p.id));ctx.fillStyle='#e8d9ad';ctx.font='11px system-ui';ctx.textAlign='left';ctx.fillText(device?device.name:`Platform ${p.id}`,x+13,y+4);}
  for(const d of state.devices.filter(d=>d.isJelly)){const [x,y]=xy(d.posX,d.posZ);ctx.shadowColor='#62e4ff';ctx.shadowBlur=d.online?16:0;ctx.fillStyle=d.online?'#62e4ff':'#42606b';ctx.beginPath();ctx.arc(x,y,d.isBig?9:6,0,Math.PI*2);ctx.fill();ctx.shadowBlur=0;ctx.fillStyle='#ccefff';ctx.font='11px system-ui';ctx.textAlign='left';ctx.fillText(d.name,x+12,y+4);}
}
function roles(d){const list=[];if(d.isJelly)list.push(d.isBig?'big jelly':'jelly');if(d.hasScale)list.push('platform');if(d.localShowOverride)list.push(`${d.localOverrideMode||'local'} override`);if(!list.length)list.push('controller');return list.map(x=>`<span class="role">${esc(x)}</span>`).join('');}
function renderDevices(){const grid=document.getElementById('deviceGrid');if(!state.devices.length){grid.innerHTML='<div class="small">Waiting for mDNS discovery...</div>';return;}grid.innerHTML=state.devices.map(d=>{const sensor=d.sensor||{},field=d.field||{};return `<article class="device-card ${d.online?'':'offline'}" data-key="${esc(d.key)}"><div class="device-head"><div><div class="device-name">${esc(d.name)}</div><div class="small">ID ${d.id||'—'} · ${esc(d.ip)}</div></div><span class="badge"><i class="dot ${d.online?'on':''}"></i>${d.online?'online':'offline'}</span></div><div class="roles">${roles(d)}</div><div class="device-metrics"><div class="metric"><b>${esc(d.scene||d.pattern)}</b><span>scene</span></div><div class="metric"><b>${d.hasScale?pct(sensor.calmness):pct(field.harmony)}</b><span>${d.hasScale?'calm':'harmony'}</span></div><div class="metric"><b>${d.hasScale?pct(sensor.agitation):(d.rssi??'—')}</b><span>${d.hasScale?'agitation':'RSSI dBm'}</span></div></div></article>`}).join('');grid.querySelectorAll('.device-card').forEach(card=>card.onclick=()=>openDevice(card.dataset.key));}
function renderEvents(){const box=document.getElementById('events');if(!state.events.length){box.innerHTML='<div class="small">No activation events yet.</div>';return;}box.innerHTML=state.events.slice(0,8).map(e=>`<div class="event"><b>Platform ${e.platformId||'test'}</b> at (${num(e.x,1)}, ${num(e.z,1)})<div class="small">${num(e.weightKg,1)} kg · gains ${(e.speakerGains||[]).map(v=>num(v,2)).join(' / ')}</div></div>`).join('');}
function openDevice(key){state.selectedKey=key;state.dialogDirty=false;const d=state.devices.find(x=>x.key===key);renderDialog(d);deviceDialog.showModal();}
function slider(name,label,value,max=1,step=.01){return `<label>${label}<input name="${name}" type="range" min="0" max="${max}" step="${step}" value="${esc(value)}"><span class="small" data-value-for="${name}">${num(value)}</span></label>`;}
function hueToHex(hue){const h=((Number(hue)||0)%1+1)%1;const hue2rgb=(p,q,t)=>{if(t<0)t+=1;if(t>1)t-=1;if(t<1/6)return p+(q-p)*6*t;if(t<1/2)return q;if(t<2/3)return p+(q-p)*(2/3-t)*6;return p;};const r=hue2rgb(0,1,h+1/3),g=hue2rgb(0,1,h),b=hue2rgb(0,1,h-1/3);const toHex=v=>Math.round(v*255).toString(16).padStart(2,'0');return `#${toHex(r)}${toHex(g)}${toHex(b)}`;}
function hexToHue(hex){const r=parseInt(hex.slice(1,3),16)/255,g=parseInt(hex.slice(3,5),16)/255,b=parseInt(hex.slice(5,7),16)/255;const max=Math.max(r,g,b),min=Math.min(r,g,b);if(max===min)return 0;const d=max-min;let h;if(max===r)h=(g-b)/d+(g<b?6:0);else if(max===g)h=(b-r)/d+2;else h=(r-g)/d+4;return h/6;}
function colorPicker(name,label,hex){return `<label class="color-field"><input name="${name}" type="color" value="${hex}"><span>${label}</span></label>`;}
function sensorTuningPanel(d){
  if(!d.hasScale)return '';
  const t={
    calibrationFactor:-14850,occupancyOnKg:18,occupancyOffKg:10,smoothing:.2,
    noiseFloorKg:.015,movementScaleKg:.2,stillnessThreshold:.12,
    calmBuildSeconds:35,calmRiseSeconds:8,calmFallSeconds:1.5,sampleIntervalMs:80,
    ...(d.sensorTuning||{})
  };
  return `<div class="subpanel"><h3>Platform sensing and calmness</h3>
    <form data-submit="sensor">
      <div class="field-grid four">
        <label>Calibration factor<input name="calibrationFactor" type="number" step="any" value="${esc(t.calibrationFactor)}"></label>
        <label>Occupied above (kg)<input name="occupancyOnKg" type="number" min="0" step="0.1" value="${esc(t.occupancyOnKg)}"></label>
        <label>Empty below (kg)<input name="occupancyOffKg" type="number" min="0" step="0.1" value="${esc(t.occupancyOffKg)}"></label>
        <label>Sample interval (ms)<input name="sampleIntervalMs" type="number" min="20" max="1000" value="${esc(t.sampleIntervalMs)}"></label>
        <label>Smoothing<input name="smoothing" type="number" min="0.01" max="1" step="0.01" value="${esc(t.smoothing)}"></label>
        <label>Noise floor (kg/sample)<input name="noiseFloorKg" type="number" min="0" step="0.001" value="${esc(t.noiseFloorKg)}"></label>
        <label>Movement scale (kg/sample)<input name="movementScaleKg" type="number" min="0.001" step="0.005" value="${esc(t.movementScaleKg)}"></label>
        <label>Stillness threshold<input name="stillnessThreshold" type="number" min="0" max="1" step="0.01" value="${esc(t.stillnessThreshold)}"></label>
        <label>Calm build (s)<input name="calmBuildSeconds" type="number" min="1" step="1" value="${esc(t.calmBuildSeconds)}"></label>
        <label>Calm rise filter (s)<input name="calmRiseSeconds" type="number" min="0.1" step="0.1" value="${esc(t.calmRiseSeconds)}"></label>
        <label>Calm fall filter (s)<input name="calmFallSeconds" type="number" min="0.1" step="0.1" value="${esc(t.calmFallSeconds)}"></label>
      </div>
      <div class="button-row"><button type="button" data-action="tareSensor">Tare now</button></div>
      <div class="small" style="margin-top:8px">The separate occupied/empty thresholds prevent chatter. Calmness builds only while occupied and below the stillness threshold.</div>
    </form>
  </div>`;
}
function renderDialog(d){
  if(!d)return;
  document.getElementById('dialogTitle').textContent=d.name;
  const p={brightness:.72,speed:.45,scale:1,density:.45,hue:.52,hue2:.72,contrast:.65,sparkle:.1,...d.patternParameters};
  const hueHex=hueToHex(p.hue),hue2Hex=hueToHex(p.hue2);
  const sensor=d.sensor||{},field=d.field||{};
  const polar=xzToPolar(Number(d.posX)||0,Number(d.posZ)||0);
  document.getElementById('dialogBody').innerHTML=`
    <div class="subpanel">
      <div class="section-title"><h3>Live state</h3><span class="badge">${esc(d.firmware||'unknown firmware')}</span></div>
      <div class="field-grid four">
        <div><b>${d.online?'Online':'Offline'}</b><div class="small">${esc(d.statusError||d.signalQuality||'')}</div></div>
        <div><b>${d.rssi??'—'} dBm</b><div class="small">Wi-Fi</div></div>
        <div><b>${pct(sensor.calmness??field.harmony)}</b><div class="small">Calm / harmony</div></div>
        <div><b>${pct(sensor.agitation??field.agitation)}</b><div class="small">Agitation</div></div>
      </div>
      ${d.hasScale?`<div class="divider"></div><div class="field-grid four"><div><b>${num(sensor.weightKg,1)} kg</b><div class="small">Filtered load</div></div><div><b>${sensor.occupied?'Occupied':'Empty'}</b><div class="small">Occupancy</div></div><div><b>${esc(sensor.status||'unknown')}</b><div class="small">HX711</div></div><div><b>${d.clockSynchronized?'Synced':'Free-running'}</b><div class="small">Show clock</div></div></div>`:''}
    </div>
    <div class="subpanel"><h3>Identity and role</h3>
      <form class="deviceForm" data-path="config">
        <div class="field-grid four">
          <label>Permanent ID<input name="deviceId" type="number" min="1" max="32" value="${d.id||''}"></label>
          <label>Name<input name="name" value="${esc(d.name)}"></label>
          <label class="check"><input name="hasScale" type="checkbox" ${d.hasScale?'checked':''}>Platform scale</label>
          <label class="check"><input name="isJelly" type="checkbox" ${d.isJelly?'checked':''}>Jellyfish</label>
          <label class="check"><input name="isBig" type="checkbox" ${d.isBig?'checked':''}>Big geometry</label>
          <label>Power limit (mA)<input name="powerLimitMilliAmps" type="number" min="500" max="60000" value="${d.powerLimitMilliAmps||12000}"></label>
        </div>
        <div class="button-row"><button type="button" data-action="identify">Identify</button><button type="button" data-action="tare" ${d.hasScale?'':'disabled'}>Tare scale</button></div>
      </form>
    </div>
    <div class="subpanel"><h3>Position</h3>
      <form data-submit="position"><div class="field-grid four">
        <label>Radius (m)<input name="radius" type="number" min="0" step="any" value="${num(polar.radius,3)}"></label>
        <label>Azimuth° (0=N/+Z, 90=E/+X)<input name="azimuth" type="number" step="any" value="${num(polar.azimuth,2)}"></label>
        <label>Y (height)<input name="posY" type="number" step="any" value="${d.posY}"></label>
        <label>Y rotation°<input name="rotationY" type="number" step="any" value="${d.rotationY}"></label>
      </div>
      <div class="small" style="margin-top:8px" data-derived-xz>Derived: X ${num(d.posX,3)} · Z ${num(d.posZ,3)}</div>
      <input type="hidden" name="posX" value="${d.posX}"><input type="hidden" name="posZ" value="${d.posZ}">
      </form>
    </div>
    ${sensorTuningPanel(d)}
    <div class="subpanel"><h3>Fallback pattern and parameters</h3>
      <form data-submit="pattern">
        <div class="field-grid">
          <label>Pattern<select name="pattern">${patternOptions(d.pattern)}</select></label>
          <label>Controller override<select name="mode"><option value="follow" ${!d.localShowOverride?'selected':''}>Follow global conductor</option><option value="auto" ${d.localShowOverride&&d.localOverrideMode==='auto'?'selected':''}>Local auto</option><option value="fallback" ${d.localShowOverride&&d.localOverrideMode==='fallback'?'selected':''}>Local fallback pattern</option><option value="forest" ${d.localShowOverride&&d.localOverrideMode==='forest'?'selected':''}>Local ambient forest</option><option value="chorus" ${d.localShowOverride&&d.localOverrideMode==='chorus'?'selected':''}>Local chorus</option><option value="blackout" ${d.localShowOverride&&d.localOverrideMode==='blackout'?'selected':''}>Local blackout</option></select></label>
        </div>
        <div class="field-grid four" style="margin-top:14px">${slider('brightness','Brightness',p.brightness)}${slider('speed','Speed',p.speed)}${slider('scale','Scale',p.scale,2,.01)}${slider('density','Density',p.density)}${colorPicker('hue','Primary color',hueHex)}${colorPicker('hue2','Secondary color',hue2Hex)}${slider('contrast','Contrast',p.contrast)}${slider('sparkle','Sparkle',p.sparkle)}</div>
      </form>
    </div>
    <div class="subpanel"><h3>Maintenance</h3>
      <form data-submit="ota"><div class="field-grid"><label>Firmware .bin<input name="update" type="file" accept=".bin"></label></div><div class="button-row"><button>Upload OTA</button><button type="button" data-action="loadLog">Load log</button></div></form>
      <div id="deviceLog" class="log">Log not loaded.</div>
    </div>`;
  wireDialog(d);
}
function wireDialog(d){
  const body=document.getElementById('dialogBody');
  body.oninput=event=>{if(event.target.matches('input,select,textarea'))state.dialogDirty=true;};
  body.onchange=event=>{if(event.target.matches('input,select,textarea'))state.dialogDirty=true;};
  body.querySelectorAll('input[type=range]').forEach(input=>{
    input.oninput=()=>body.querySelector(`[data-value-for="${input.name}"]`).textContent=num(input.value);
  });
  const identityForm=body.querySelector('[data-path=config]');
  body.querySelector('[data-action=identify]').onclick=()=>postForm(`/api/device/${encodeURIComponent(d.key)}/identify`,new FormData());
  body.querySelector('[data-action=tare]').onclick=()=>saveDeviceForm(`/api/device/${encodeURIComponent(d.key)}/tare`,new FormData());
  const posForm=body.querySelector('[data-submit=position]');
  posForm.onsubmit=e=>e.preventDefault();
  const posXHidden=posForm.elements.posX,posZHidden=posForm.elements.posZ;
  const radiusInput=posForm.elements.radius,azimuthInput=posForm.elements.azimuth;
  const derivedXZ=posForm.querySelector('[data-derived-xz]');
  const syncFromPolar=()=>{
    const {x,z}=polarToXZ(Number(radiusInput.value)||0,Number(azimuthInput.value)||0);
    posXHidden.value=x;posZHidden.value=z;
    derivedXZ.textContent=`Derived: X ${num(x,3)} · Z ${num(z,3)}`;
  };
  radiusInput.oninput=syncFromPolar;
  azimuthInput.oninput=syncFromPolar;
  const sensorForm=body.querySelector('[data-submit=sensor]');
  if(sensorForm){
    sensorForm.onsubmit=e=>e.preventDefault();
    sensorForm.querySelector('[data-action=tareSensor]').onclick=()=>saveDeviceForm(`/api/device/${encodeURIComponent(d.key)}/tare`,new FormData());
  }
  const pf=body.querySelector('[data-submit=pattern]');
  document.getElementById('dialogSaveAll').onclick=async()=>{
    const rn=new FormData();rn.append('name',identityForm.name.value);
    const renameResult=await request(`/api/device/${encodeURIComponent(d.key)}/rename`,{method:'POST',body:rn});
    if(!renameResult.ok){showToast(renameResult);return;}
    const requested=Number(identityForm.deviceId.value);
    if(requested&&requested!==Number(d.id)){
      const id=new FormData();id.append('deviceId',String(requested));
      const idResult=await request(`/api/device/${encodeURIComponent(d.key)}/device-id`,{method:'POST',body:id});
      if(!idResult.ok){showToast(idResult);return;}
    }
    const configFd=new FormData();
    for(const n of ['hasScale','isJelly','isBig'])if(identityForm[n].checked)configFd.append(n,'true');
    configFd.append('powerLimitMilliAmps',identityForm.powerLimitMilliAmps.value);
    const configResult=await request(`/api/device/${encodeURIComponent(d.key)}/config`,{method:'POST',body:configFd});
    if(!configResult.ok){showToast(configResult);return;}
    const posResult=await request(`/api/device/${encodeURIComponent(d.key)}/position`,{method:'POST',body:new FormData(posForm)});
    if(!posResult.ok){showToast(posResult);return;}
    if(sensorForm){
      const sensorResult=await request(`/api/device/${encodeURIComponent(d.key)}/sensor`,{method:'POST',body:new FormData(sensorForm)});
      if(!sensorResult.ok){showToast(sensorResult);return;}
    }
    const patternFd=new FormData();patternFd.append('pattern',pf.pattern.value);
    const patternResult=await request(`/api/device/${encodeURIComponent(d.key)}/pattern`,{method:'POST',body:patternFd});
    if(!patternResult.ok){showToast(patternResult);return;}
    const paramsFd=new FormData();
    for(const n of ['brightness','speed','scale','density','contrast','sparkle'])paramsFd.append(n,pf[n].value);
    paramsFd.append('hue',hexToHue(pf.hue.value));paramsFd.append('hue2',hexToHue(pf.hue2.value));
    const paramsResult=await request(`/api/device/${encodeURIComponent(d.key)}/pattern-parameters`,{method:'POST',body:paramsFd});
    if(!paramsResult.ok){showToast(paramsResult);return;}
    const modeFd=new FormData();const override=pf.mode.value!=='follow';
    modeFd.append('localOverride',override?'true':'false');
    modeFd.append('mode',override?pf.mode.value:(d.localOverrideMode||'fallback'));
    modeFd.append('masterBrightness',String(d.masterBrightness??1));
    const modeResult=await request(`/api/device/${encodeURIComponent(d.key)}/show`,{method:'POST',body:modeFd});
    if(!modeResult.ok){showToast(modeResult);return;}
    showToast('All values saved.');
    state.dialogDirty=false;await refresh();
  };
  body.querySelector('[data-submit=ota]').onsubmit=async e=>{
    e.preventDefault();await saveDeviceForm(`/api/device/${encodeURIComponent(d.key)}/update`,e.target);
  };
  body.querySelector('[data-action=loadLog]').onclick=async()=>{
    const data=await request(`/api/device/${encodeURIComponent(d.key)}/log`);
    body.querySelector('#deviceLog').textContent=data.log||JSON.stringify(data,null,2);
  };
}

deviceDialog.addEventListener('close',()=>{state.dialogDirty=false;state.selectedKey=null;});
document.getElementById('showForm').onsubmit=async e=>{e.preventDefault();const fd=new FormData(e.target);fd.set('audioEnabled',e.target.audioEnabled.checked?'true':'false');await postForm('/api/installation/settings',fd);state.settingsLoaded=false;refresh();};
document.getElementById('waveForm').onsubmit=async e=>{e.preventDefault();await postForm('/api/installation/test-wave',e.target);refresh();};
document.getElementById('broadcastPatternButton').onclick=async()=>{const current=prompt('Fallback pattern name:', 'waterfall');if(!current)return;const fd=new FormData();fd.append('pattern',current);await postForm('/api/broadcast/pattern',fd);refresh();};
document.getElementById('broadcastUpdateForm').onsubmit=async e=>{e.preventDefault();if(!e.target.update.files.length){showToast('Choose a firmware .bin first.');return;}await postForm('/api/broadcast/update',e.target);e.target.reset();};
refresh();setInterval(()=>{const active=document.activeElement;const editing=active&&['INPUT','SELECT','TEXTAREA'].includes(active.tagName);const fileChosen=[...document.querySelectorAll('input[type=file]')].some(input=>input.files&&input.files.length);if(editing||fileChosen)return;refresh();},2000);
