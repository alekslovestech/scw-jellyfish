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
from dataclasses import dataclass, field
from typing import Any

import httpx
from fastapi import FastAPI, File, Form, UploadFile
from fastapi.responses import HTMLResponse, JSONResponse
from zeroconf import ServiceBrowser, ServiceListener, Zeroconf

SERVICE_TYPE = "_esp32art._tcp.local."
POLL_INTERVAL_SECONDS = 2.0
DEVICE_TIMEOUT_SECONDS = 8.0
REQUEST_TIMEOUT_SECONDS = 2.0

app = FastAPI(title="ESP32 Fleet Dashboard")


@dataclass
class Device:
    key: str
    name: str
    host: str
    ip: str
    port: int = 80
    mac: str = ""
    chip: str = ""
    firmware: str = ""
    ssid: str = ""
    rssi: int | None = None
    signal_quality: int | None = None
    last_seen: float = field(default_factory=time.time)
    online: bool = False
    number: float | None = None
    has_number: bool = False
    status_error: str = ""

    @property
    def base_url(self) -> str:
        # Use IP for actual requests because .local resolution can vary per OS.
        return f"http://{self.ip}:{self.port}"

    def as_dict(self) -> dict[str, Any]:
        return {
            "key": self.key,
            "name": self.name,
            "host": self.host,
            "ip": self.ip,
            "port": self.port,
            "url": self.base_url,
            "mac": self.mac,
            "chip": self.chip,
            "firmware": self.firmware,
            "ssid": self.ssid,
            "rssi": self.rssi,
            "signalQuality": self.signal_quality,
            "lastSeenAgo": round(time.time() - self.last_seen, 1),
            "online": self.online and (time.time() - self.last_seen < DEVICE_TIMEOUT_SECONDS),
            "number": self.number,
            "hasNumber": self.has_number,
            "statusError": self.status_error,
        }


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


def rssi_to_quality(rssi: int | None) -> int | None:
    if rssi is None:
        return None
    # Simple Wi-Fi quality estimate: -50 dBm or better is excellent, -90 dBm is unusable.
    if rssi <= -90:
        return 0
    if rssi >= -50:
        return 100
    return max(0, min(100, 2 * (rssi + 100)))

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
                d.signal_quality = rssi_to_quality(d.rssi)
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


@app.on_event("startup")
async def startup() -> None:
    app.state.zeroconf = Zeroconf()
    app.state.browser = ServiceBrowser(app.state.zeroconf, SERVICE_TYPE, ESPListener())
    asyncio.create_task(poll_loop())


@app.on_event("shutdown")
async def shutdown() -> None:
    app.state.zeroconf.close()


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
        raw_rssi = data.get("rssi", data.get("wifiRssi"))
        if raw_rssi is not None:
            try:
                d.rssi = int(raw_rssi)
                d.signal_quality = rssi_to_quality(d.rssi)
            except (TypeError, ValueError):
                pass
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


@app.post("/api/device/{key}/identify", response_model=None)
async def api_identify(key: str) -> dict[str, Any] | JSONResponse:
    d = get_device_or_404(key)
    if isinstance(d, JSONResponse): return d
    async with httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS) as client:
        r = await client.post(f"{d.base_url}/identify")
    return {"ok": r.status_code < 300, "http": r.status_code}


@app.post("/api/device/{key}/rename", response_model=None)
async def api_rename(key: str, name: str = Form(...)) -> dict[str, Any] | JSONResponse:
    d = get_device_or_404(key)
    if isinstance(d, JSONResponse): return d
    async with httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS) as client:
        r = await client.post(f"{d.base_url}/rename", data={"name": name})
    d.name = name
    return {"ok": r.status_code < 300, "http": r.status_code}


@app.post("/api/device/{key}/set-number", response_model=None)
async def api_set_number(key: str, value: float = Form(...)) -> dict[str, Any] | JSONResponse:
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


@app.get("/api/device/{key}/log", response_model=None)
async def api_device_log(key: str) -> dict[str, Any] | JSONResponse:
    d = get_device_or_404(key)
    if isinstance(d, JSONResponse): return d
    async with httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS) as client:
        try:
            r = await client.get(f"{d.base_url}/log")
            return {"ok": r.status_code < 300, "http": r.status_code, "device": d.name, "log": r.text}
        except Exception as e:
            return JSONResponse({"ok": False, "error": str(e), "device": d.name}, status_code=502)


@app.post("/api/device/{key}/update", response_model=None)
async def api_update(key: str, update: UploadFile = File(...)) -> dict[str, Any] | JSONResponse:
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
    .actions{display:flex;gap:6px;flex-wrap:wrap}.actions form{display:inline-flex;gap:6px;align-items:center}.small{width:90px}.name{width:140px}.log{white-space:pre-wrap;background:#111;color:#eee;padding:12px;border-radius:8px;min-height:40px}
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
    <thead><tr><th>Status</th><th>Name</th><th>Address</th><th>Firmware</th><th>Wi-Fi</th><th>Number</th><th>Last seen</th><th>Actions</th></tr></thead>
    <tbody id="rows"><tr><td colspan="8">Waiting for ESP32 devices...</td></tr></tbody>
  </table>

  <h3>Result</h3><div id="log" class="log"></div>

<script>
const rows = document.getElementById('rows');
const log = document.getElementById('log');
function esc(s){return String(s ?? '').replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));}
function signalText(d){
  if(d.rssi === null || d.rssi === undefined) return '-';
  const q = d.signalQuality ?? '';
  return `${d.rssi} dBm${q !== '' ? ' / '+q+'%' : ''}`;
}
async function viewLog(key, name){
  log.textContent = `Loading log from ${name}...`;
  const r = await fetch(`/api/device/${encodeURIComponent(key)}/log`);
  const data = await r.json().catch(() => ({}));
  if(data.ok){ log.textContent = `--- ${data.device} log ---\n` + data.log; }
  else { log.textContent = JSON.stringify(data, null, 2); }
}
async function postForm(url, form){
  const r = await fetch(url, {method:'POST', body:new FormData(form)});
  const data = await r.json().catch(() => ({}));
  log.textContent = JSON.stringify(data, null, 2);
  refresh();
}
async function refresh(){
  const data = await fetch('/api/devices').then(r => r.json());
  if(!data.devices.length){ rows.innerHTML = '<tr><td colspan="8">Waiting for ESP32 devices...</td></tr>'; return; }
  rows.innerHTML = data.devices.map(d => `
    <tr>
      <td><span class="dot ${d.online?'on':'off'}"></span>${d.online?'Online':'Offline'}${d.statusError ? '<br><small>'+esc(d.statusError)+'</small>' : ''}</td>
      <td><b>${esc(d.name)}</b><br><small>${esc(d.mac || d.chip || d.key)}</small></td>
      <td><a href="${esc(d.url)}" target="_blank">${esc(d.ip)}</a><br><small>${esc(d.host)}</small></td>
      <td>${esc(d.firmware || '-')}</td>
      <td>${esc(d.ssid || '-')}<br><small>${esc(signalText(d))}</small></td>
      <td>${d.hasNumber ? esc(d.number) : '-'}</td>
      <td>${esc(d.lastSeenAgo)}s</td>
      <td class="actions">
        <button onclick="fetch('/api/device/${encodeURIComponent(d.key)}/identify',{method:'POST'}).then(r=>r.json()).then(x=>{log.textContent=JSON.stringify(x,null,2); refresh();})">Blink</button>
        <button data-key="${esc(d.key)}" data-name="${esc(d.name)}" onclick="viewLog(this.dataset.key, this.dataset.name)">Log</button>
        <form onsubmit="event.preventDefault(); postForm('/api/device/${encodeURIComponent(d.key)}/set-number', this)"><input class="small" name="value" type="number" step="any" value="123"><button>Send</button></form>
        <form onsubmit="event.preventDefault(); postForm('/api/device/${encodeURIComponent(d.key)}/rename', this)"><input class="name" name="name" value="${esc(d.name)}"><button>Rename</button></form>
        <form onsubmit="event.preventDefault(); postForm('/api/device/${encodeURIComponent(d.key)}/update', this)"><input name="update" type="file" accept=".bin"><button>OTA</button></form>
      </td>
    </tr>`).join('');
}
document.getElementById('broadcastNumber').onsubmit = e => { e.preventDefault(); postForm('/api/broadcast/set-number', e.target); };
document.getElementById('broadcastUpdate').onsubmit = e => { e.preventDefault(); postForm('/api/broadcast/update', e.target); };
refresh(); setInterval(refresh, 2000);
</script>
</body>
</html>
"""

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)
