#!/usr/bin/env python3
"""Compile reconstructed source with the original Xbox 360 SDK, without installing it."""

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCES = [
    "src/GameShared/GameClasses/Containers/CgsHash.cpp",
    "src/GameShared/GameClasses/Containers/CgsHash12.cpp",
    "src/GameShared/GameClasses/Containers/CgsHash16.cpp",
    "src/GameShared/GameClasses/Core/CgsStringUtils.cpp",
    "src/GameShared/GameClasses/System/Timer/CgsTimeUtils.cpp",
    "src/GameShared/GameClasses/Module/CgsIOBufferStack.cpp",
]
INCLUDES = [
    "src",
    "vendor/EABase/include/Common",
    "vendor/EASTL/include",
    "vendor/EAThread/include",
    "vendor/renderware/include",
    "vendor/PPMalloc/include",
    "vendor/coreallocator/include",
    "vendor/dirtysdk/include",
    "vendor/zlib/include",
    "vendor/lua/src",
    "src/GameShared/GameClasses/Development",
]
FLAGS = [
    "/nologo", "/c", "/X", "/O2", "/Oi", "/Ob2", "/GR-", "/GS-",
    "/EHs-c-", "/Gy", "/FAcs", "/D_XBOX", "/DD_PLATFORM_X360=1",
    "/DCGS_PLATFORM_X360=1",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, help="XDK root containing bin/win32/cl.exe; defaults to XEDK")
    parser.add_argument("--source", action="append", help="Repository-relative .cpp/.c path; repeat to compile several")
    parser.add_argument("--out", type=Path, default=ROOT / "build/x360/objects")
    parser.add_argument("--compiler-arg", action="append", default=[])
    parser.add_argument("--timeout", type=int, default=60, help="Seconds per translation unit")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("The SDK's Windows compiler must run on Windows")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")

    sdk = args.sdk or (Path(os.environ["XEDK"]) if os.environ.get("XEDK") else None)
    if sdk is None:
        parser.error("Pass --sdk or set XEDK to the extracted SDK root")
    sdk = sdk.resolve()
    compiler = sdk / "bin/win32/cl.exe"
    if not compiler.is_file() or not (sdk / "include/xbox/xtl.h").is_file():
        parser.error("SDK must contain bin/win32/cl.exe and include/xbox/xtl.h")

    sources = []
    for name in args.source or DEFAULT_SOURCES:
        source = (ROOT / name).resolve()
        try:
            relative = source.relative_to(ROOT)
        except ValueError:
            parser.error("Sources must be inside this repository")
        if any(part.lower() == "pc" for part in relative.parts):
            parser.error("PC sources are excluded from this branch")
        if source.suffix.lower() not in (".cpp", ".c") or not source.is_file():
            parser.error("Source does not exist or is not C/C++: " + name)
        sources.append(relative)

    output = (ROOT / args.out).resolve()
    output.mkdir(parents=True, exist_ok=True)
    # Child tools stay unattended; no installation, registry or global env changes.
    ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    env = dict(os.environ)
    env["XEDK"] = str(sdk)
    env["PATH"] = str(sdk / "bin/win32") + os.pathsep + str(Path(os.environ["SystemRoot"]) / "System32")
    env["INCLUDE"] = str(sdk / "include/xbox")
    env["LIB"] = str(sdk / "lib/xbox")
    for name in ("CL", "_CL_", "LINK", "_LINK_"):
        env.pop(name, None)
    includes = [ROOT / name for name in INCLUDES if (ROOT / name).is_dir()]
    includes.append(sdk / "include/xbox")
    flags = FLAGS + ["/I" + str(path) for path in includes] + args.compiler_arg
    report = {"stage": "compile-only", "target": "xbox360-ppc-be", "sdk": str(sdk), "results": []}

    for source in sources:
        key = source.stem + "-" + hashlib.sha256(source.as_posix().lower().encode()).hexdigest()[:8]
        obj, listing, log = (output / (key + ext) for ext in (".obj", ".asm", ".log"))
        obj.unlink(missing_ok=True)
        listing.unlink(missing_ok=True)
        command = [str(compiler)] + flags + ["/Fo" + str(obj), "/Fa" + str(listing), str(source)]
        try:
            result = subprocess.run(command, cwd=ROOT, env=env, stdin=subprocess.DEVNULL,
                                    capture_output=True, text=True, errors="replace",
                                    timeout=args.timeout, creationflags=subprocess.CREATE_NO_WINDOW)
            returncode, diagnostics = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            returncode = -1
            diagnostics = "Compiler timed out after %d seconds\n" % args.timeout
            for text in (error.stdout, error.stderr):
                if text:
                    diagnostics += text.decode(errors="replace") if isinstance(text, bytes) else text
        machine = struct.unpack("<H", obj.read_bytes()[:2])[0] if obj.is_file() and obj.stat().st_size >= 2 else None
        passed = returncode == 0 and machine == 0x01F2 and listing.is_file()
        if returncode == 0 and not passed:
            diagnostics += "Expected a PowerPC big-endian COFF object (machine 0x01F2) and assembly listing\n"
        log.write_text(diagnostics, encoding="utf-8")
        report["results"].append({"source": source.as_posix(), "passed": passed,
                                  "returncode": returncode, "machine": machine,
                                  "object": str(obj), "assembly": str(listing), "log": str(log),
                                  "command": command})
        print(("PASS " if passed else "FAIL ") + source.as_posix(), flush=True)
        if not passed:
            print("\n".join(diagnostics.splitlines()[:14]), flush=True)

    report["passed"] = sum(row["passed"] for row in report["results"])
    report["failed"] = len(sources) - report["passed"]
    report_path = output / "compile-results.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("Native objects: %d passed, %d failed; %s" % (report["passed"], report["failed"], report_path))
    return 0 if report["failed"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
