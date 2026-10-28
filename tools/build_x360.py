#!/usr/bin/env python3
"""Compile reconstructed source with the original Xbox 360 SDK, without installing it."""

import argparse
from concurrent.futures import ThreadPoolExecutor
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
    "src/SDKs/EATech",
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
    "/DCGS_PLATFORM_X360=1", "/FItypes.hpp",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, help="XDK root containing bin/win32/cl.exe; defaults to XEDK")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--source", action="append", help="Repository-relative .cpp/.c path; repeat to compile several")
    selection.add_argument("--all", action="store_true", help="Compile all tracked game and reconstructed middleware sources")
    parser.add_argument("--out", type=Path, default=ROOT / "build/x360/objects")
    parser.add_argument("--compiler-arg", action="append", default=[])
    parser.add_argument("--timeout", type=int, default=60, help="Seconds per translation unit")
    parser.add_argument("--jobs", type=int, default=1, help="Concurrent compiler processes")
    parser.add_argument("--quiet", action="store_true", help="Print progress totals instead of per-file diagnostics")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("The SDK's Windows compiler must run on Windows")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if args.jobs <= 0:
        parser.error("--jobs must be positive")

    sdk = args.sdk or (Path(os.environ["XEDK"]) if os.environ.get("XEDK") else None)
    if sdk is None:
        parser.error("Pass --sdk or set XEDK to the extracted SDK root")
    sdk = sdk.resolve()
    compiler = sdk / "bin/win32/cl.exe"
    if not compiler.is_file() or not (sdk / "include/xbox/xtl.h").is_file():
        parser.error("SDK must contain bin/win32/cl.exe and include/xbox/xtl.h")

    names = args.source or DEFAULT_SOURCES
    if args.all:
        names = subprocess.check_output(
            ["git", "ls-files", "*.cpp", "*.c"], cwd=ROOT, text=True).splitlines()
        names = [name for name in names if name.startswith(("src/", "vendor/"))
                 and not any(part.lower() == "pc" for part in Path(name).parts)]
        # PS3-named paths can contain ARTIST functions attributed through DecFIGS.
        # Compile them too; filenames alone are not platform evidence.
    sources = []
    for name in dict.fromkeys(names):
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
    report = {"stage": "compile-only", "state": "running", "target": "xbox360-ppc-be",
              "sdk": str(sdk), "requested": len(sources), "results": []}
    report_path = output / "compile-results.json"

    def compile_source(source):
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
        return {"source": source.as_posix(), "passed": passed,
                "returncode": returncode, "machine": machine,
                "object": str(obj), "assembly": str(listing), "log": str(log),
                "command": command}, diagnostics

    def save_report():
        report["passed"] = sum(row["passed"] for row in report["results"])
        report["failed"] = len(report["results"]) - report["passed"]
        temporary = report_path.with_suffix(".json.tmp")
        temporary.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        temporary.replace(report_path)

    save_report()
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for row, diagnostics in pool.map(compile_source, sources):
            report["results"].append(row)
            if not args.quiet:
                print(("PASS " if row["passed"] else "FAIL ") + row["source"], flush=True)
                if not row["passed"]:
                    print("\n".join(diagnostics.splitlines()[:14]), flush=True)
            if len(report["results"]) % 100 == 0:
                save_report()
                print("%d/%d compiled: %d passed, %d failed" %
                      (len(report["results"]), len(sources), report["passed"], report["failed"]), flush=True)
    report["state"] = "complete"
    save_report()
    print("Native objects: %d passed, %d failed; %s" % (report["passed"], report["failed"], report_path))
    return 0 if report["failed"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
