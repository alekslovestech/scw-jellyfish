from __future__ import annotations

from pathlib import Path


def test_dashboard_assets_are_split_and_referenced() -> None:
    static = Path(__file__).resolve().parents[1] / "static"
    html = (static / "index.html").read_text(encoding="utf-8")
    assert 'href="/static/app.css"' in html
    assert 'src="/static/app.js"' in html
    assert "<style>" not in html
    assert "<script>\n" not in html
    assert (static / "app.css").stat().st_size > 1000
    assert (static / "app.js").stat().st_size > 5000
