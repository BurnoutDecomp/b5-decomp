#!/usr/bin/env python3
"""Bind the extracted XDK linker to its bundled CRT, without installing the SDK."""

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
RUNTIME_FILES = (
    "Microsoft.VC80.CRT.manifest", "msvcr80.dll", "msvcp80.dll",
    "Microsoft.VC80.ATL.manifest", "atl80.dll",
)
TOOL_DLLS = ("pgodb80.dll", "mspdb80.dll", "mspdbcore.dll", "msdisXXX.dll", "msobj80.dll")


def find_manifest_tool():
    installed = shutil.which("mt.exe")
    if installed:
        return Path(installed)
    kits = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Windows Kits/10/bin"
    candidates = list(kits.glob("*/x64/mt.exe"))
    return max(candidates, key=lambda p: tuple(int(n) for n in p.parents[1].name.split("."))) if candidates else None


def prepare_linker(sdk, output, manifest_tool):
    source = sdk / "bin/win32"
    manifest = source / "cl.exe.manifest"
    ET.fromstring(manifest.read_bytes().lstrip())
    output.mkdir(parents=True, exist_ok=True)
    linker = output / "link.exe"
    if linker.resolve() == (source / "link.exe").resolve():
        raise ValueError("Use a separate output directory; the original SDK must stay unchanged")
    shutil.copyfile(source / "link.exe", linker)
    for name in RUNTIME_FILES + TOOL_DLLS:
        shutil.copyfile(source / name, output / name)
    # SDK 6534.4's linker has an empty embedded manifest. Its unbound CRT
    # opens an R6034 dialog during DLL startup. The compiler's working manifest
    # supplies the same runtime identities to a local copy of the linker.
    result = subprocess.run(
        [str(manifest_tool), "-nologo", "-manifest", str(manifest),
         "-outputresource:" + str(linker) + ";#1"],
        stdin=subprocess.DEVNULL, capture_output=True, text=True,
        timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    report = {
        "original": str(source / "link.exe"),
        "original_sha256": hashlib.sha256((source / "link.exe").read_bytes()).hexdigest(),
        "linker": str(linker), "manifest": str(manifest),
        "prepared_sha256": hashlib.sha256(linker.read_bytes()).hexdigest(),
        "sdk_files_modified": False,
    }
    (output / "linker-host.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return linker


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=ROOT / "build/x360/tool-host")
    parser.add_argument("--manifest-tool", type=Path, help="Windows SDK mt.exe (auto-detected when omitted)")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("This tool requires Windows")
    sdk, output = args.sdk.resolve(), args.out.resolve()
    for name in ("link.exe", "cl.exe.manifest") + RUNTIME_FILES + TOOL_DLLS:
        if not (sdk / "bin/win32" / name).is_file():
            parser.error("Missing SDK file: " + name)
    manifest_tool = args.manifest_tool or find_manifest_tool()
    if manifest_tool is None or not manifest_tool.is_file():
        parser.error("Pass --manifest-tool pointing to Windows SDK mt.exe")
    ctypes.windll.kernel32.SetErrorMode(0x8003)
    linker = prepare_linker(sdk, output, manifest_tool)
    print(linker)
    return 0


if __name__ == "__main__":
    sys.exit(main())
