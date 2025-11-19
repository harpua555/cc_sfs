#!/usr/bin/env python3
"""
Build the Web UI, sync it into the data/ filesystem image, and flash firmware + filesystem
to an ESP32 using PlatformIO.

Usage (run from repo root):

  python tools/build_and_flash.py

Options:
  --env ENV            PlatformIO env to use (default: esp32-s3-dev)
  --skip-npm-install   Skip `npm install` (assumes webui/node_modules already present)
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from typing import List, Optional


def run(cmd: List[str], cwd: Optional[str] = None) -> None:
    print(f"> {' '.join(cmd)} (cwd={cwd or os.getcwd()})")
    subprocess.run(cmd, cwd=cwd, check=True)


def ensure_executable(name: str) -> None:
    if shutil.which(name) is None:
        print(f"ERROR: `{name}` is not on PATH.")
        if name == "npm":
            print("Please install Node.js (which includes npm) and try again.")
        elif name in ("python", "python3"):
            print("Please ensure Python is installed and available as `python`.")
        else:
            print(f"Install `{name}` or update your PATH.")
        sys.exit(1)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Build WebUI + firmware and flash to ESP32 via PlatformIO."
    )
    parser.add_argument(
        "--env",
        default="esp32-s3-dev",
        help="PlatformIO environment to use (default: esp32-s3-dev)",
    )
    parser.add_argument(
        "--skip-npm-install",
        action="store_true",
        help="Skip `npm install` in webui/ (assumes dependencies already installed).",
    )
    args = parser.parse_args()

    # Resolve paths relative to this file (tools/ -> repo root)
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(tools_dir)
    webui_dir = os.path.join(repo_root, "webui")

    # Build WebUI and sync into data/
    npm_cmd = "npm.cmd" if os.name == "nt" else "npm"
    ensure_executable(npm_cmd)

    if not args.skip_npm_install:
        run([npm_cmd, "install"], cwd=webui_dir)

    run([npm_cmd, "run", "build"], cwd=webui_dir)

    # Flash firmware + filesystem using PlatformIO CLI (pio or platformio)
    pio_cmd = shutil.which("pio") or shutil.which("platformio")
    if not pio_cmd:
        print(
            "ERROR: Neither `pio` nor `platformio` is on PATH.\n"
            "Install PlatformIO Core (pip install platformio) "
            "and/or add its Scripts directory to PATH."
        )
        sys.exit(1)

    # Filesystem upload
    run([pio_cmd, "run", "-e", args.env, "-t", "uploadfs"], cwd=repo_root)

    # Firmware upload (will build firmware first if needed)
    run([pio_cmd, "run", "-e", args.env, "-t", "upload"], cwd=repo_root)

    print("\nAll done. Firmware and filesystem have been flashed.")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        print(f"\nCommand failed with exit code {exc.returncode}: {exc.cmd}")
        sys.exit(exc.returncode)
