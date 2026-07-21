from __future__ import annotations

import importlib
import sys
import types


def test_fastapi_routes_can_be_constructed_without_live_mdns() -> None:
    if "zeroconf" not in sys.modules:
        module = types.ModuleType("zeroconf")
        module.ServiceListener = type("ServiceListener", (), {})
        module.ServiceBrowser = type("ServiceBrowser", (), {})
        module.Zeroconf = type("Zeroconf", (), {})
        sys.modules["zeroconf"] = module
    app_module = importlib.import_module("dashboard.app")
    api_paths = {route.path for route in app_module.app.routes}
    assert "/api/installation" in api_paths
    assert "/api/device/{key}/pattern-parameters" in api_paths
    assert "/api/broadcast/update" in api_paths
