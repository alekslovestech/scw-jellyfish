#!/usr/bin/env python3
"""Run the Jellyfish Forest dashboard and show conductor."""

import uvicorn


if __name__ == "__main__":
    uvicorn.run("dashboard.app:app", host="0.0.0.0", port=8080, reload=False)
