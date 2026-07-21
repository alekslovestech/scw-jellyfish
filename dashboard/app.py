#!/usr/bin/env python3
"""Jellyfish Forest fleet dashboard, show conductor, and OTA manager."""

from __future__ import annotations

import asyncio
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Any

from fastapi import FastAPI, File, Form, UploadFile
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles

from .conductor import ShowConductor
from .device import Device
from .fleet import FleetManager

BASE_DIR = Path(__file__).resolve().parent
STATIC_DIR = BASE_DIR / "static"
SETTINGS_PATH = BASE_DIR / "show_settings.json"

conductor = ShowConductor(SETTINGS_PATH)
fleet = FleetManager(status_sink=conductor.ingest_device_status)


@asynccontextmanager
async def lifespan(app: FastAPI):
    del app
    await conductor.start()
    await fleet.start()
    try:
        yield
    finally:
        await fleet.stop()
        await conductor.stop()


app = FastAPI(title="Jellyfish Forest Dashboard", lifespan=lifespan)
app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")


def device_or_error(key: str) -> Device | JSONResponse:
    device = fleet.registry.get(key)
    if not device:
        return JSONResponse(
            {"ok": False, "error": "Unknown device"}, status_code=404
        )
    return device


async def proxy_form(
    key: str, path: str, data: dict[str, str]
) -> Any:
    device = device_or_error(key)
    if isinstance(device, JSONResponse):
        return device
    return await fleet.request(device, "POST", path, data=data)


async def persist_global_settings() -> list[dict[str, Any]]:
    """Persist conductor settings to online controllers for dashboard-free fallback."""
    settings = conductor.settings
    show_data = {
        "mode": settings.mode,
        "masterBrightness": str(settings.master_brightness),
    }
    interaction_data = {
        "expectedPlatformCount": str(settings.expected_platform_count),
        "influenceRadiusMeters": str(settings.influence_radius_meters),
        "calmThreshold": str(settings.calm_threshold),
        "calmReleaseThreshold": str(settings.calm_release_threshold),
        "maxAgitationForChorus": str(settings.max_agitation_for_chorus),
        "allCalmHoldSeconds": str(settings.all_calm_hold_seconds),
        "chorusFadeSeconds": str(settings.chorus_fade_seconds),
        "activationWaveSpeedMetersPerSecond": str(
            settings.activation_wave_speed_mps
        ),
        "activationWaveWidthMeters": str(settings.activation_wave_width_meters),
        "activationWaveDurationSeconds": str(
            settings.activation_wave_duration_seconds
        ),
    }

    async def persist_device(device: Device) -> dict[str, Any]:
        show_result, interaction_result = await asyncio.gather(
            fleet.request(device, "POST", "/show", data=show_data),
            fleet.request(device, "POST", "/interaction", data=interaction_data),
        )
        return {
            "key": device.key,
            "name": device.name,
            "ok": bool(show_result.get("ok") and interaction_result.get("ok")),
            "show": show_result,
            "interaction": interaction_result,
        }

    return await asyncio.gather(
        *(persist_device(device) for device in fleet.registry.values() if device.is_online())
    )


@app.get("/")
async def index() -> FileResponse:
    return FileResponse(STATIC_DIR / "index.html")


@app.get("/api/devices")
async def api_devices() -> dict[str, Any]:
    return {"devices": fleet.registry.sorted_dicts()}


@app.get("/api/installation")
async def api_installation() -> dict[str, Any]:
    status = conductor.status()
    status["devices"] = fleet.registry.sorted_dicts()
    return status


@app.post("/api/installation/settings")
async def api_installation_settings(
    mode: str | None = Form(None),
    masterBrightness: float | None = Form(None),
    expectedPlatformCount: int | None = Form(None),
    influenceRadiusMeters: float | None = Form(None),
    calmThreshold: float | None = Form(None),
    calmReleaseThreshold: float | None = Form(None),
    maxAgitationForChorus: float | None = Form(None),
    allCalmHoldSeconds: float | None = Form(None),
    chorusFadeSeconds: float | None = Form(None),
    activationWaveSpeedMetersPerSecond: float | None = Form(None),
    activationWaveWidthMeters: float | None = Form(None),
    activationWaveDurationSeconds: float | None = Form(None),
    mapMinX: float | None = Form(None),
    mapMaxX: float | None = Form(None),
    mapMinZ: float | None = Form(None),
    mapMaxZ: float | None = Form(None),
    audioEnabled: bool | None = Form(None),
    audioHost: str | None = Form(None),
    audioPort: int | None = Form(None),
) -> dict[str, Any]:
    raw = {
        "mode": mode,
        "masterBrightness": masterBrightness,
        "expectedPlatformCount": expectedPlatformCount,
        "influenceRadiusMeters": influenceRadiusMeters,
        "calmThreshold": calmThreshold,
        "calmReleaseThreshold": calmReleaseThreshold,
        "maxAgitationForChorus": maxAgitationForChorus,
        "allCalmHoldSeconds": allCalmHoldSeconds,
        "chorusFadeSeconds": chorusFadeSeconds,
        "activationWaveSpeedMetersPerSecond": activationWaveSpeedMetersPerSecond,
        "activationWaveWidthMeters": activationWaveWidthMeters,
        "activationWaveDurationSeconds": activationWaveDurationSeconds,
        "mapMinX": mapMinX,
        "mapMaxX": mapMaxX,
        "mapMinZ": mapMinZ,
        "mapMaxZ": mapMaxZ,
        "audioEnabled": audioEnabled,
        "audioHost": audioHost,
        "audioPort": audioPort,
    }
    updates = {key: value for key, value in raw.items() if value is not None}
    settings = conductor.update_settings(updates)
    device_results = await persist_global_settings()
    return {
        "ok": True,
        "settings": settings.public_dict(),
        "deviceResults": device_results,
    }


@app.post("/api/installation/test-wave")
async def api_test_wave(
    x: float = Form(0.0),
    y: float = Form(0.0),
    z: float = Form(0.0),
    platformId: int = Form(0),
) -> dict[str, Any]:
    return {"ok": True, "event": conductor.trigger_wave(x, y, z, platformId)}


@app.get("/api/device/{key}/log")
async def api_device_log(key: str) -> Any:
    device = device_or_error(key)
    if isinstance(device, JSONResponse):
        return device
    result = await fleet.request(device, "GET", "/log")
    body = result.get("body")
    if isinstance(body, dict):
        result["log"] = body.get("log", "")
    else:
        result["log"] = body or ""
    return result


@app.post("/api/device/{key}/identify")
async def api_identify(key: str) -> Any:
    return await proxy_form(key, "/identify", {})


@app.post("/api/device/{key}/rename")
async def api_rename(
    key: str, name: str = Form(...)
) -> Any:
    result = await proxy_form(key, "/rename", {"name": name})
    device = fleet.registry.get(key)
    if device and isinstance(result, dict) and result.get("ok"):
        device.name = name
    return result


@app.post("/api/device/{key}/device-id")
async def api_device_id(
    key: str, deviceId: int = Form(...)
) -> Any:
    device = device_or_error(key)
    if isinstance(device, JSONResponse):
        return device
    if not 1 <= deviceId <= 32:
        return JSONResponse(
            {"ok": False, "error": "ID must be between 1 and 32"},
            status_code=400,
        )
    conflict = next(
        (
            other
            for other in fleet.registry.values()
            if other is not device and other.device_id == deviceId
        ),
        None,
    )
    if conflict:
        return JSONResponse(
            {
                "ok": False,
                "error": f"ID {deviceId} is already assigned to {conflict.name}",
            },
            status_code=409,
        )
    result = await fleet.request(
        device, "POST", "/device-id", data={"id": str(deviceId)}
    )
    if result.get("ok"):
        device.device_id = deviceId
        device.online = False
        device.status_error = "Rebooting after device ID change"
    return result


@app.post("/api/device/{key}/config")
async def api_config(
    key: str,
    hasScale: bool = Form(False),
    isJelly: bool = Form(False),
    isBig: bool = Form(False),
    powerLimitMilliAmps: int = Form(12000),
) -> Any:
    return await proxy_form(
        key,
        "/config",
        {
            "hasScale": "1" if hasScale else "0",
            "isJelly": "1" if isJelly else "0",
            "isBig": "1" if isBig else "0",
            "powerLimitMilliAmps": str(powerLimitMilliAmps),
        },
    )


@app.post("/api/device/{key}/position")
async def api_position(
    key: str,
    posX: float = Form(...),
    posY: float = Form(...),
    posZ: float = Form(...),
    rotationY: float = Form(...),
) -> Any:
    return await proxy_form(
        key,
        "/position",
        {
            "posX": str(posX),
            "posY": str(posY),
            "posZ": str(posZ),
            "rotationY": str(rotationY),
        },
    )


@app.post("/api/device/{key}/pattern")
async def api_pattern(
    key: str, pattern: str = Form(...)
) -> Any:
    return await proxy_form(key, "/pattern", {"pattern": pattern})


@app.post("/api/device/{key}/pattern-parameters")
async def api_pattern_parameters(
    key: str,
    brightness: float = Form(...),
    speed: float = Form(...),
    scale: float = Form(...),
    density: float = Form(...),
    hue: float = Form(...),
    hue2: float = Form(...),
    contrast: float = Form(...),
    sparkle: float = Form(...),
) -> Any:
    return await proxy_form(
        key,
        "/pattern-parameters",
        {
            "brightness": str(brightness),
            "speed": str(speed),
            "scale": str(scale),
            "density": str(density),
            "hue": str(hue),
            "hue2": str(hue2),
            "contrast": str(contrast),
            "sparkle": str(sparkle),
        },
    )


@app.post("/api/device/{key}/show")
async def api_device_show(
    key: str,
    mode: str = Form(...),
    masterBrightness: float = Form(1.0),
    localOverride: bool | None = Form(None),
) -> Any:
    data = {"mode": mode, "masterBrightness": str(masterBrightness)}
    if localOverride is not None:
        data["localOverride"] = "1" if localOverride else "0"
    return await proxy_form(key, "/show", data)


@app.post("/api/device/{key}/sensor")
async def api_sensor(
    key: str,
    calibrationFactor: float | None = Form(None),
    occupancyOnKg: float | None = Form(None),
    occupancyOffKg: float | None = Form(None),
    smoothing: float | None = Form(None),
    noiseFloorKg: float | None = Form(None),
    movementScaleKg: float | None = Form(None),
    stillnessThreshold: float | None = Form(None),
    calmBuildSeconds: float | None = Form(None),
    calmRiseSeconds: float | None = Form(None),
    calmFallSeconds: float | None = Form(None),
    sampleIntervalMs: int | None = Form(None),
) -> Any:
    values = {
        "calibrationFactor": calibrationFactor,
        "occupancyOnKg": occupancyOnKg,
        "occupancyOffKg": occupancyOffKg,
        "smoothing": smoothing,
        "noiseFloorKg": noiseFloorKg,
        "movementScaleKg": movementScaleKg,
        "stillnessThreshold": stillnessThreshold,
        "calmBuildSeconds": calmBuildSeconds,
        "calmRiseSeconds": calmRiseSeconds,
        "calmFallSeconds": calmFallSeconds,
        "sampleIntervalMs": sampleIntervalMs,
    }
    return await proxy_form(
        key,
        "/sensor",
        {name: str(value) for name, value in values.items() if value is not None},
    )


@app.post("/api/device/{key}/interaction")
async def api_interaction(
    key: str,
    expectedPlatformCount: int | None = Form(None),
    influenceRadiusMeters: float | None = Form(None),
    calmThreshold: float | None = Form(None),
    calmReleaseThreshold: float | None = Form(None),
    maxAgitationForChorus: float | None = Form(None),
    allCalmHoldSeconds: float | None = Form(None),
    chorusFadeSeconds: float | None = Form(None),
    activationWaveSpeedMetersPerSecond: float | None = Form(None),
    activationWaveWidthMeters: float | None = Form(None),
    activationWaveDurationSeconds: float | None = Form(None),
) -> Any:
    values = {
        "expectedPlatformCount": expectedPlatformCount,
        "influenceRadiusMeters": influenceRadiusMeters,
        "calmThreshold": calmThreshold,
        "calmReleaseThreshold": calmReleaseThreshold,
        "maxAgitationForChorus": maxAgitationForChorus,
        "allCalmHoldSeconds": allCalmHoldSeconds,
        "chorusFadeSeconds": chorusFadeSeconds,
        "activationWaveSpeedMetersPerSecond": activationWaveSpeedMetersPerSecond,
        "activationWaveWidthMeters": activationWaveWidthMeters,
        "activationWaveDurationSeconds": activationWaveDurationSeconds,
    }
    return await proxy_form(
        key,
        "/interaction",
        {name: str(value) for name, value in values.items() if value is not None},
    )


@app.post("/api/device/{key}/tare")
async def api_tare(key: str) -> Any:
    return await proxy_form(key, "/tare", {})


@app.post("/api/device/{key}/set-number")
async def api_set_number(
    key: str, value: float = Form(...)
) -> Any:
    return await proxy_form(key, "/setNumber", {"value": str(value)})


@app.post("/api/broadcast/pattern")
async def api_broadcast_pattern(pattern: str = Form(...)) -> dict[str, Any]:
    tasks = [
        fleet.request(device, "POST", "/pattern", data={"pattern": pattern})
        for device in fleet.registry.values()
        if device.is_online() and device.is_jelly
    ]
    return {"results": await asyncio.gather(*tasks)}


@app.post("/api/broadcast/set-number")
async def api_broadcast_number(value: float = Form(...)) -> dict[str, Any]:
    tasks = [
        fleet.request(device, "POST", "/setNumber", data={"value": str(value)})
        for device in fleet.registry.values()
        if device.is_online()
    ]
    return {"results": await asyncio.gather(*tasks)}


@app.post("/api/device/{key}/update")
async def api_update(
    key: str, update: UploadFile = File(...)
) -> Any:
    device = device_or_error(key)
    if isinstance(device, JSONResponse):
        return device
    content = await update.read()
    files = {
        "update": (
            update.filename or "firmware.bin",
            content,
            "application/octet-stream",
        )
    }
    return await fleet.request(
        device, "POST", "/update", files=files, timeout=90.0
    )


@app.post("/api/broadcast/update")
async def api_broadcast_update(update: UploadFile = File(...)) -> dict[str, Any]:
    content = await update.read()
    results: list[dict[str, Any]] = []
    for device in fleet.registry.values():
        if not device.is_online():
            continue
        files = {
            "update": (
                update.filename or "firmware.bin",
                content,
                "application/octet-stream",
            )
        }
        results.append(
            await fleet.request(
                device, "POST", "/update", files=files, timeout=90.0
            )
        )
    return {"results": results}


if __name__ == "__main__":
    import uvicorn

    uvicorn.run("dashboard.app:app", host="0.0.0.0", port=8080, reload=False)
