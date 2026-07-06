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
from pathlib import Path

from typing import Any

import httpx
from fastapi import FastAPI, File, Form, UploadFile
from fastapi.responses import FileResponse, HTMLResponse, JSONResponse
from zeroconf import ServiceBrowser, ServiceListener, Zeroconf
from device import DEVICE_TIMEOUT_SECONDS, Device

STATIC_DIR = Path(__file__).parent / "static"

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
async def index() -> FileResponse:
    return FileResponse(STATIC_DIR / "index.html")


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


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)
