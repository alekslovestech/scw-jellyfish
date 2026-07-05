#!/usr/bin/env python3
"""
ESP32 Fleet Dashboard

Run on your laptop. Discovers ESP32s via mDNS service _esp32art._tcp.local,
keeps status fresh, and lets you identify, rename, send values, and upload OTA firmware.
"""

from __future__ import annotations
import asyncio
import socket
import time
from contextlib import asynccontextmanager

from typing import Any

import httpx
from fastapi import FastAPI, File, Form, UploadFile
from fastapi.responses import HTMLResponse, JSONResponse
from zeroconf import ServiceBrowser, ServiceListener, Zeroconf
from device import DEVICE_TIMEOUT_SECONDS, Device

SERVICE_TYPE = "_esp32art._tcp.local."
POLL_INTERVAL_SECONDS = 2.0
REQUEST_TIMEOUT_SECONDS = 2.0

DEVICES: dict[str, Device] = {}


def decode_txt(properties: dict[bytes, bytes | None]) -> dict[str, str]:
    out: dict[str, str] = {}
    for k, v in properties.items():
        key = k.decode(errors="ignore") if isinstance(k, bytes) else str(k)
        if v is None:
            out[key] = ""
        else:
            out[key] = v.decode(errors="ignore") if isinstance(v, bytes) else str(v)
    return out


def rssi_quality(rssi: int | None) -> str:
    if rssi is None:
        return ""
    if rssi >= -55:
        return "Excellent"
    if rssi >= -65:
        return "Good"
    if rssi >= -75:
        return "Fair"
    return "Weak"

class ESPListener(ServiceListener):
    def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        self.add_service(zc, type_, name)

    def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        info = zc.get_service_info(type_, name, timeout=2000)
        if not info or not info.addresses:
            return

        ip = socket.inet_ntoa(info.addresses[0])
        txt = decode_txt(info.properties)
        device_name = txt.get("name") or name.replace(f".{SERVICE_TYPE}", "").replace(".local.", "")
        key = txt.get("chip") or txt.get("mac") or name

        d = DEVICES.get(key) or Device(key=key, name=device_name, host=info.server, ip=ip, port=info.port or 80)
        d.name = device_name
        d.host = info.server.rstrip(".")
        d.ip = ip
        d.port = info.port or 80
        d.mac = txt.get("mac", d.mac)
        d.chip = txt.get("chip", d.chip)
        d.firmware = txt.get("fw", d.firmware)
        d.ssid = txt.get("ssid", d.ssid)
        if "rssi" in txt:
            try:
                d.rssi = int(txt["rssi"])
                d.signal_quality = rssi_quality(d.rssi)
            except ValueError:
                pass
        d.online = True
        d.last_seen = time.time()
        d.status_error = ""
        DEVICES[key] = d

    def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        for d in DEVICES.values():
            if name.startswith(d.name):
                d.online = False


@asynccontextmanager
async def lifespan(app: FastAPI):
    app.state.zeroconf = Zeroconf()
    app.state.browser = ServiceBrowser(app.state.zeroconf, SERVICE_TYPE, ESPListener())
    app.state.poll_task = asyncio.create_task(poll_loop())
    try:
        yield
    finally:
        app.state.poll_task.cancel()
        try:
            await app.state.poll_task
        except asyncio.CancelledError:
            pass
        app.state.zeroconf.close()


app = FastAPI(title="ESP32 Fleet Dashboard", lifespan=lifespan)


async def poll_loop() -> None:
    async with httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS) as client:
        while True:
            await asyncio.gather(*(poll_device(client, d) for d in list(DEVICES.values())), return_exceptions=True)
            await asyncio.sleep(POLL_INTERVAL_SECONDS)


async def poll_device(client: httpx.AsyncClient, d: Device) -> None:
    try:
        # Preferred endpoint if you add the ESP32 code below.
        r = await client.get(f"{d.base_url}/status")
        if r.status_code == 404:
            # Fallback for your current code: /number already exists.
            r = await client.get(f"{d.base_url}/number")
        r.raise_for_status()
        data = r.json()
        d.online = True
        d.last_seen = time.time()
        d.status_error = ""
        d.number = data.get("number", d.number)
        d.has_number = bool(data.get("hasNumber", d.has_number))
        d.name = data.get("name", d.name)
        d.mac = data.get("mac", d.mac)
        d.chip = data.get("chip", d.chip)
        d.firmware = data.get("firmware", d.firmware)
        d.ssid = data.get("ssid", d.ssid)
        d.has_scale = bool(data.get("hasScale", d.has_scale))
        d.is_jelly = bool(data.get("isJelly", d.is_jelly))
        d.pattern = data.get("pattern", d.pattern)
        d.posX = float(data.get("posX", d.posX))
        d.posY = float(data.get("posY", d.posY))
        d.posZ = float(data.get("posZ", d.posZ))
        if data.get("rssi") is not None:
            d.rssi = int(data.get("rssi"))
            d.signal_quality = rssi_quality(d.rssi)
    except Exception as e:
        if time.time() - d.last_seen > DEVICE_TIMEOUT_SECONDS:
            d.online = False
        d.status_error = str(e)


def get_device_or_404(key: str) -> Device | JSONResponse:
    d = DEVICES.get(key)
    if not d:
        return JSONResponse({"ok": False, "error": "Unknown device"}, status_code=404)
    return d


@app.get("/", response_class=HTMLResponse)
async def index() -> str:
    return DASHBOARD_HTML


@app.get("/api/devices")
async def api_devices() -> dict[str, Any]:
    return {"devices": sorted([d.as_dict() for d in DEVICES.values()], key=lambda x: x["name"])}


@app.get("/api/device/{key}/log", response_model=None)
async def api_device_log(key: str):
    d = get_device_or_404(key)
    if isinstance(d, JSONResponse): return d
    try:
        async with httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS) as client:
            r = await client.get(f"{d.base_url}/log")
        body = r.text
        if r.headers.get("content-type", "").startswith("application/json"):
            try:
                body = r.json().get("log", body)
            except Exception:
                pass
        return {"ok": r.status_code < 300, "http": r.status_code, "name": d.name, "log": body}
    except Exception as e:
        return {"ok": False, "name": d.name, "error": str(e), "log": ""}


@app.post("/api/device/{key}/identify", response_model=None)
async def api_identify(key: str):
    d = get_device_or_404(key)
    if isinstance(d, JSONResponse): return d
    async with httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS) as client:
        r = await client.post(f"{d.base_url}/identify")
    return {"ok": r.status_code < 300, "http": r.status_code}


@app.post("/api/device/{key}/rename", response_model=None)
async def api_rename(key: str, name: str = Form(...)):
    d = get_device_or_404(key)
    if isinstance(d, JSONResponse): return d
    async with httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS) as client:
        r = await client.post(f"{d.base_url}/rename", data={"name": name})
    d.name = name
    return {"ok": r.status_code < 300, "http": r.status_code}

@app.post("/api/device/{key}/config", response_model=None)
async def api_config(
    key: str,
    hasScale: bool = Form(False),
    isJelly: bool = Form(False),
):
    d = get_device_or_404(key)
    if isinstance(d, JSONResponse):
        return d

    async with httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS) as client:
        r = await client.post(
            f"{d.base_url}/config",
            data={
                "hasScale": "1" if hasScale else "0",
                "isJelly": "1" if isJelly else "0",
            },
        )

    if r.status_code < 300:
        d.has_scale = hasScale
        d.is_jelly = isJelly

    return {"ok": r.status_code < 300, "http": r.status_code, "body": r.text[:200]}

@app.post("/api/device/{key}/pattern", response_model=None)
async def api_pattern(key: str, pattern: str = Form(...)):
    d = get_device_or_404(key)
    if isinstance(d, JSONResponse):
        return d

    try:
        async with httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS) as client:
            r = await client.post(f"{d.base_url}/pattern", data={"pattern": pattern})
    except Exception as e:
        # Device unreachable / timed out — don't 500, and leave d.pattern untouched.
        return {"ok": False, "error": str(e)}

    ok = r.status_code < 300
    if ok:
        # The device echoes its resulting pattern; trust that as the source of truth
        # rather than the value we sent.
        try:
            d.pattern = r.json().get("pattern", pattern)
        except Exception:
            d.pattern = pattern

    return {"ok": ok, "http": r.status_code, "body": r.text[:200]}


@app.post("/api/device/{key}/position", response_model=None)
async def api_position(
    key: str,
    posX: float = Form(...),
    posY: float = Form(...),
    posZ: float = Form(...),
):
    d = get_device_or_404(key)
    if isinstance(d, JSONResponse):
        return d

    async with httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS) as client:
        r = await client.post(
            f"{d.base_url}/position",
            data={
                "posX": str(posX),
                "posY": str(posY),
                "posZ": str(posZ),
            },
        )

    if r.status_code < 300:
        d.posX = posX
        d.posY = posY
        d.posZ = posZ

    return {"ok": r.status_code < 300, "http": r.status_code, "body": r.text[:200]}

@app.post("/api/device/{key}/set-number", response_model=None)
async def api_set_number(key: str, value: float = Form(...)):
    d = get_device_or_404(key)
    if isinstance(d, JSONResponse): return d
    async with httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS) as client:
        r = await client.post(f"{d.base_url}/setNumber", data={"value": str(value)})
    return {"ok": r.status_code < 300, "http": r.status_code, "body": r.text[:200]}


@app.post("/api/broadcast/set-number")
async def api_broadcast_number(value: float = Form(...)) -> dict[str, Any]:
    results = []
    async with httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS) as client:
        for d in DEVICES.values():
            try:
                r = await client.post(f"{d.base_url}/setNumber", data={"value": str(value)})
                results.append({"name": d.name, "ok": r.status_code < 300, "http": r.status_code})
            except Exception as e:
                results.append({"name": d.name, "ok": False, "error": str(e)})
    return {"results": results}


@app.post("/api/device/{key}/update", response_model=None)
async def api_update(key: str, update: UploadFile = File(...)):
    d = get_device_or_404(key)
    if isinstance(d, JSONResponse): return d
    content = await update.read()
    files = {"update": (update.filename or "firmware.bin", content, "application/octet-stream")}
    async with httpx.AsyncClient(timeout=60.0) as client:
        r = await client.post(f"{d.base_url}/update", files=files)
    return {"ok": r.status_code < 300, "http": r.status_code, "body": r.text[:500]}


@app.post("/api/broadcast/update")
async def api_broadcast_update(update: UploadFile = File(...)) -> dict[str, Any]:
    content = await update.read()
    results = []
    # Sequential OTA is safer; 16 simultaneous firmware uploads can overwhelm Wi-Fi/ESP RAM.
    async with httpx.AsyncClient(timeout=60.0) as client:
        for d in DEVICES.values():
            files = {"update": (update.filename or "firmware.bin", content, "application/octet-stream")}
            try:
                r = await client.post(f"{d.base_url}/update", files=files)
                results.append({"name": d.name, "ok": r.status_code < 300, "http": r.status_code})
            except Exception as e:
                results.append({"name": d.name, "ok": False, "error": str(e)})
    return {"results": results}


DASHBOARD_HTML = r"""
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 Fleet Dashboard</title>
  <style>
    body{font-family:system-ui,-apple-system,Segoe UI,Arial,sans-serif;margin:24px;background:#fafafa;color:#111}
    h1{margin-bottom:6px}.muted{color:#666}.bar{display:flex;gap:12px;flex-wrap:wrap;align-items:end;margin:18px 0}
    input,button{font:inherit;padding:8px 10px;border:1px solid #ccc;border-radius:8px;background:white}button{cursor:pointer}
    table{border-collapse:collapse;width:100%;background:white;border-radius:12px;overflow:hidden;box-shadow:0 1px 4px #0001}
    th,td{text-align:left;padding:10px;border-bottom:1px solid #eee;vertical-align:middle}th{background:#f0f0f0}
    .dot{display:inline-block;width:12px;height:12px;border-radius:50%;margin-right:8px}.on{background:#19a34a}.off{background:#c22}
    .actions{display:flex;gap:6px;flex-wrap:wrap}.actions form{display:inline-flex;gap:6px;align-items:center}.small{width:90px}.name{width:140px}.loc{width:72px}.locationForm{display:flex;gap:6px;align-items:end;flex-wrap:wrap}.locationForm label{font-size:12px;color:#555}.locationForm input{display:block;margin-top:2px}.log{white-space:pre-wrap;background:#111;color:#eee;padding:12px;border-radius:8px;min-height:40px}.deviceLog{max-height:420px;overflow:auto}.panel{margin-top:20px;background:white;border-radius:12px;padding:14px;box-shadow:0 1px 4px #0001}.hidden{display:none}.rowbar{display:flex;gap:10px;align-items:center;justify-content:space-between;flex-wrap:wrap}
  </style>
</head>
<body>
  <h1>ESP32 Fleet Dashboard</h1>
  <div class="muted">Auto-refreshes every 2 seconds using mDNS + HTTP.</div>

  <div class="bar">
    <form id="broadcastNumber"><label>Broadcast number<br><input name="value" type="number" step="any" value="123"></label><button>Send to all</button></form>
    <form id="broadcastUpdate"><label>OTA all, sequential<br><input name="update" type="file" accept=".bin"></label><button>Upload to all</button></form>
  </div>

  <table>
    <thead><tr><th>Status</th><th>Name</th><th>Address</th><th>Firmware</th><th>Signal</th><th>Position</th><th>Config</th><th>Last seen</th><th>Actions</th></tr></thead>
    <tbody id="rows"><tr><td colspan="9">Waiting for ESP32 devices...</td></tr></tbody>
  </table>

  <div id="deviceLogPanel" class="panel hidden">
    <div class="rowbar">
      <h3 id="deviceLogTitle" style="margin:0">Device log</h3>
      <div>
        <label><input id="autoLog" type="checkbox" checked> Auto-refresh</label>
        <button onclick="refreshDeviceLog()">Refresh log</button>
        <button onclick="closeDeviceLog()">Close</button>
      </div>
    </div>
    <div id="deviceLog" class="log deviceLog"></div>
  </div>

  <h3>Result</h3><div id="log" class="log"></div>

<script>
const rows = document.getElementById('rows');
const log = document.getElementById('log');
const deviceLogPanel = document.getElementById('deviceLogPanel');
const deviceLogTitle = document.getElementById('deviceLogTitle');
const deviceLog = document.getElementById('deviceLog');
const autoLog = document.getElementById('autoLog');
let selectedLogKey = null;
let selectedLogName = '';
const renameDrafts = new Map();

function esc(s){return String(s ?? '').replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));}

// Pattern menu — single source of truth. Add [value, label] here and the dropdown follows.
const PATTERNS = [
  ['heartbeat', 'Heartbeat'],
  ['demo', 'Demo'],
  ['ripple', 'Ripple effect'],
  ['fireSpread', 'Fire spread'],
  ['waterfall', 'Waterfall'],
  ['twoToneDiffuse', 'Two-tone diffuse'],
  ['colorwheel', 'Color wheel'],
  ['bottomfill', 'Bottom fill'],
  ['sensordemo', 'Sensor demo'],
  ['fallingRain', 'Falling rain'],
  ['movementSimulation', 'Movement simulation'],
  ['innerSpreadWave', 'Inner spread wave'],
  ['rainbowWave', 'Rainbow wave'],
];

function patternSelect(current){
  const options = PATTERNS.map(([value, label]) =>
    `<option value="${value}"${value === current ? ' selected' : ''}>${esc(label)}</option>`
  ).join('');
  return `<select name="pattern" onchange="this.form.requestSubmit()">${options}</select>`;
}

function tableHasActiveFormState(){
  // The table is rebuilt on each refresh. Avoid destroying user input while a
  // single-device form is being edited, especially file inputs, which browsers
  // do not allow JavaScript to restore after re-rendering.
  const active = document.activeElement;
  if(active && rows.contains(active) && ['INPUT','SELECT','TEXTAREA'].includes(active.tagName)) return true;
  return Array.from(rows.querySelectorAll('input[type="file"]')).some(input => input.files && input.files.length > 0);
}

rows.addEventListener('input', e => {
  if(e.target.matches('.renameInput')){
    renameDrafts.set(e.target.dataset.key, e.target.value);
  }
});
function signalHtml(d){
  if(d.rssi === null || d.rssi === undefined) return '-';
  return esc(d.rssi) + ' dBm<br><small>' + esc(d.signalQuality || '') + '</small>';
}
async function postForm(url, form){
  const body = new FormData(form);
  const r = await fetch(url, {method:'POST', body});
  const data = await r.json().catch(() => ({}));
  log.textContent = JSON.stringify(data, null, 2);

  if(form.classList.contains('renameForm') && data.ok){
    renameDrafts.delete(form.dataset.key);
  }
  if(form.classList.contains('singleUpdate') && data.ok){
    form.reset();
  }

  refresh(true);
}
function openDeviceLog(key, name){
  selectedLogKey = key;
  selectedLogName = name;
  deviceLogTitle.textContent = 'Device log: ' + name;
  deviceLogPanel.classList.remove('hidden');
  refreshDeviceLog();
}
function closeDeviceLog(){
  selectedLogKey = null;
  deviceLogPanel.classList.add('hidden');
}
async function refreshDeviceLog(){
  if(!selectedLogKey) return;
  try{
    const data = await fetch('/api/device/' + encodeURIComponent(selectedLogKey) + '/log').then(r => r.json());
    deviceLog.textContent = data.ok ? (data.log || '(empty log)') : JSON.stringify(data, null, 2);
    deviceLog.scrollTop = deviceLog.scrollHeight;
  }catch(e){
    deviceLog.textContent = String(e);
  }
}
async function refresh(force=false){
  if(!force && tableHasActiveFormState()) return;
  const data = await fetch('/api/devices').then(r => r.json());
  if(!data.devices.length){ rows.innerHTML = '<tr><td colspan="9">Waiting for ESP32 devices...</td></tr>'; return; }
  rows.innerHTML = data.devices.map(d => `
    <tr>
      <td><span class="dot ${d.online?'on':'off'}"></span>${d.online?'Online':'Offline'}${d.statusError ? '<br><small>'+esc(d.statusError)+'</small>' : ''}</td>
      <td><b>${esc(d.name)}</b><br><small>${esc(d.mac || d.chip || d.key)}</small></td>
      <td><a href="${esc(d.url)}" target="_blank">${esc(d.ip)}</a><br><small>${esc(d.host)}</small></td>
      <td>${esc(d.firmware || '-')}<br><small>${esc(d.ssid || '')}</small></td>
      <td>${signalHtml(d)}</td>
      <td>
        <form class="locationForm" onsubmit="event.preventDefault(); postForm('/api/device/${encodeURIComponent(d.key)}/position', this)">
          <label>X<input class="loc" name="posX" type="number" step="any" value="${esc(d.posX ?? 0)}"></label>
          <label>Y<input class="loc" name="posY" type="number" step="any" value="${esc(d.posY ?? 0)}"></label>
          <label>Z<input class="loc" name="posZ" type="number" step="any" value="${esc(d.posZ ?? 0)}"></label>
          <button>Save</button>
        </form>
      </td>
      <td>
        Scale: ${d.hasScale ? 'yes' : 'no'}<br>
        Jelly: ${d.isJelly ? 'yes' : 'no'}
      </td>
      <td>${esc(d.lastSeenAgo)}s</td>
      <td class="actions">
          <form onsubmit="event.preventDefault(); postForm('/api/device/${encodeURIComponent(d.key)}/config', this)">
            <label><input type="checkbox" name="hasScale" ${d.hasScale ? 'checked' : ''}> Scale</label>
            <label><input type="checkbox" name="isJelly" ${d.isJelly ? 'checked' : ''}> Jelly</label>
            <button>Save config</button>
          </form>
          <form onsubmit="event.preventDefault(); postForm('/api/device/${encodeURIComponent(d.key)}/pattern', this)">
            <label>Pattern
              ${patternSelect(d.pattern)}
            </label>
          </form>
        <button onclick="fetch('/api/device/${encodeURIComponent(d.key)}/identify',{method:'POST'}).then(r=>r.json()).then(x=>{log.textContent=JSON.stringify(x,null,2); refresh();})">Blink</button>
        <button data-key="${esc(d.key)}" data-name="${esc(d.name)}" onclick="openDeviceLog(this.dataset.key, this.dataset.name)">Log</button>
        <form class="renameForm" data-key="${esc(d.key)}" onsubmit="event.preventDefault(); postForm('/api/device/${encodeURIComponent(d.key)}/rename', this)"><input class="name renameInput" data-key="${esc(d.key)}" name="name" value="${esc(renameDrafts.get(d.key) ?? d.name)}"><button>Rename</button></form>
        <form class="singleUpdate" onsubmit="event.preventDefault(); postForm('/api/device/${encodeURIComponent(d.key)}/update', this)"><input name="update" type="file" accept=".bin"><button>Send update</button></form>
      </td>
    </tr>`).join('');
}
document.getElementById('broadcastNumber').onsubmit = e => { e.preventDefault(); postForm('/api/broadcast/set-number', e.target); };
document.getElementById('broadcastUpdate').onsubmit = e => { e.preventDefault(); postForm('/api/broadcast/update', e.target); };
refresh(); setInterval(refresh, 2000); setInterval(() => { if(selectedLogKey && autoLog.checked) refreshDeviceLog(); }, 2000);
</script>
</body>
</html>
"""

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)
