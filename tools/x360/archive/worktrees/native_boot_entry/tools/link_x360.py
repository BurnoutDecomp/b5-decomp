#!/usr/bin/env python3
"""Link native Xbox 360 objects and, after a complete link, build an XEX.

Inputs are build_x360.py compile reports, not a directory glob: old failed
objects cannot silently re-enter the link. Partial links are diagnostic only.
"""

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MACHINE = 0x01F2
ENTRY_SOURCE = "src/GameSource/Main/BrnMainX360.cpp"
# Original XDK 6534.4 TriangleHLSL2005.vcproj, Release|Xbox 360.
SDK_LIBRARIES = ["d3d9.lib", "d3dx9.lib", "xgraphics.lib", "xapilib.lib",
                 "xaudio.lib", "x3daudio.lib", "xboxkrnl.lib", "xbdm.lib"]
ASSETS = {
    "BrnGame::gpLoadingScreenTextureArrow": (0x4000, "affee0686027b6428434336ac8f19d1fc5745fc6f2c26dc0d2f9422ec94efe57"),
    "BrnGame::gpLoadingScreenTextureCar": (0x200000, "90512933f117408cac32d23b4ef426d0a6590b420c2a314c128ad54ded7a7338"),
    "BrnGame::gpLoadingScreenTextureText": (0x40000, "4333d59da3d710079f208cf18f6f640483caf3633bc1b6c2abb2f900c7ec47dc"),
    "BrnGame::gpLoadingScreenTextureBox": (0x4000, "b3fdb4214c53995d75952f09b80eab93cc40389b29ea1876482e3919c0635ff2"),
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def coff(path):
    """Read ordinary PPC COFF sections and external symbol definitions."""
    data = path.read_bytes()
    if len(data) < 20:
        raise ValueError("Truncated COFF object: " + str(path))
    machine, count, _, symbols_at, symbols_count, optional, _ = struct.unpack_from("<HHIIIHH", data)
    if machine != MACHINE or optional:
        raise ValueError("Expected native PPC COFF 01F2: " + str(path))
    sections = []
    for i in range(count):
        offset = 20 + 40 * i
        name, _, _, size, raw_at, _, _, _, _, flags = struct.unpack_from("<8sIIIIIIHHI", data, offset)
        if raw_at and raw_at + size > len(data):
            raise ValueError("Truncated COFF section: " + str(path))
        sections.append({"name": name.rstrip(b"\0").decode("ascii"),
                         "data": data[raw_at:raw_at + size] if raw_at else b"",
                         "size": size, "flags": flags})
    strings_at = symbols_at + symbols_count * 18
    symbols = {}
    index = 0
    while index < symbols_count:
        offset = symbols_at + index * 18
        name, value, section, typ, storage, auxiliary = struct.unpack_from("<8sIhHBB", data, offset)
        if name[:4] == b"\0\0\0\0":
            start = strings_at + struct.unpack_from("<I", name, 4)[0]
            end = data.index(b"\0", start)
            name = data[start:end]
        name = name.rstrip(b"\0").decode("ascii", errors="replace")
        if storage == 2 and section > 0:
            symbols[name] = (section - 1, value)
        index += 1 + auxiliary
    return data, sections, symbols


def run(command, env, cwd, log, timeout):
    try:
        result = subprocess.run(command, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                                capture_output=True, timeout=timeout,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        code, output = result.returncode, result.stdout + result.stderr
    except subprocess.TimeoutExpired as error:
        code = -1
        output = (error.stdout or b"") + (error.stderr or b"")
        output += ("\nTimed out after %d seconds\n" % timeout).encode()
    log.write_bytes(output)
    return {"command": command, "returncode": code, "log": str(log),
            "diagnostics": output.decode(errors="replace")}


def build_assets(manifest, sdk, output, env, timeout):
    records = json.loads(manifest.read_text(encoding="utf-8"))
    if not isinstance(records, list) or {r["symbol"] for r in records} != set(ASSETS) or len(records) != 4:
        raise ValueError("Asset manifest must contain the four original loading-screen arrays")
    results = []
    for record in records:
        symbol = record["symbol"]
        size, expected = ASSETS[symbol]
        path = Path(record["path"])
        if not path.is_absolute():
            path = manifest.parent / path
        data = path.read_bytes()
        if (len(data), sha256(data)) != (size, expected) or record.get("sha256") != expected:
            raise ValueError("Original ARTIST asset size/hash mismatch: " + symbol)
        name = symbol.split("::")[-1]
        source, obj, log = (output / (name + ext) for ext in (".cpp", ".obj", ".log"))
        # Real mutable array definitions. In particular, Car is reused as a
        # LinearMalloc backing store after texture upload by the original game.
        with source.open("w", encoding="ascii", newline="\n") as stream:
            stream.write("namespace BrnGame { unsigned char %s[%d] = {\n" % (name, size))
            for offset in range(0, size, 64):
                stream.write(",".join("0x%02x" % byte for byte in data[offset:offset + 64]) + ",\n")
            stream.write("}; }\n")
        obj.unlink(missing_ok=True)
        result = run([str(sdk / "bin/win32/cl.exe"), "/nologo", "/c", "/X", "/Zl",
                      "/D_XBOX", "/Fo" + str(obj), str(source)], env, output, log, timeout)
        if result["returncode"]:
            raise ValueError("Native asset compilation failed; see " + str(log))
        raw, sections, symbols = coff(obj)
        # Use the native compiler's mangling, then verify the defined bytes at
        # the actual symbol's section offset, not merely the generated C++ text.
        matches = [item for key, item in symbols.items() if key.startswith("?" + name + "@BrnGame@@")]
        if len(matches) != 1:
            raise ValueError("Missing/ambiguous native asset symbol: " + symbol)
        section, offset = matches[0]
        section = sections[section]
        if not section["flags"] & 0x80000000 or section["data"][offset:offset + size] != data:
            raise ValueError("Native asset object bytes or writable section differ: " + symbol)
        result.pop("diagnostics")
        results.append(dict(result, symbol=symbol, size=size, sha256=expected,
                            input=str(path.resolve()), source=str(source), object=str(obj),
                            object_sha256=sha256(raw), writable=True))
    return results


def reuse_assets(path):
    report = json.loads(path.read_text(encoding="utf-8"))
    assets = report.get("assets", [])
    if report.get("target") != "xbox360-ppc-be" or len(assets) != 4 or {a["symbol"] for a in assets} != set(ASSETS):
        raise ValueError("Expected a native report containing all four verified assets")
    for asset in assets:
        symbol = asset["symbol"]
        size, expected = ASSETS[symbol]
        data, sections, symbols = coff(Path(asset["object"]))
        if sha256(data) != asset["object_sha256"]:
            raise ValueError("Previously verified asset object changed: " + symbol)
        name = symbol.split("::")[-1]
        matches = [item for key, item in symbols.items() if key.startswith("?" + name + "@BrnGame@@")]
        if len(matches) != 1:
            raise ValueError("Missing/ambiguous asset symbol: " + symbol)
        section, offset = matches[0]
        section = sections[section]
        if not section["flags"] & 0x80000000 or sha256(section["data"][offset:offset + size]) != expected:
            raise ValueError("Previously verified asset bytes or writability changed: " + symbol)
    return assets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--linker", type=Path, help="Prepared portable native link.exe")
    parser.add_argument("--report", type=Path, action="append", default=[],
                        help="Compile report; repeat in oldest-to-newest order for source overrides")
    parser.add_argument("--out", type=Path, default=ROOT / "build/x360/link")
    assets = parser.add_mutually_exclusive_group()
    assets.add_argument("--assets", type=Path, help="Manifest of the four original ARTIST loading textures")
    assets.add_argument("--asset-report", type=Path, help="Reuse and reverify native asset objects from an earlier report")
    parser.add_argument("--assets-only", action="store_true", help="Compile and verify only embedded asset objects")
    parser.add_argument("--diagnose-partial", action="store_true",
                        help="Run the real linker on available objects; never produce an XEX")
    parser.add_argument("--exclude-source", action="append", default=[],
                        help="Exact report source to exclude (for example standalone Lua tools); recorded in output")
    parser.add_argument("--library", action="append", default=[], help="Additional native .lib in SDK lib/xbox")
    parser.add_argument("--xex-config", type=Path, help="Explicit ImageXEX title configuration; required to build an XEX")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    if os.name != "nt" or args.timeout <= 0:
        parser.error("Requires Windows and a positive timeout")
    if args.assets_only and not (args.assets or args.asset_report):
        parser.error("--assets-only requires --assets or --asset-report")
    if not args.assets_only and not args.report:
        parser.error("Pass at least one --report")
    sdk, output = args.sdk.resolve(), args.out.resolve()
    linker = (args.linker or sdk / "bin/win32/link.exe").resolve()
    if not (sdk / "bin/win32/cl.exe").is_file() or not (sdk / "lib/xbox/xboxkrnl.lib").is_file():
        parser.error("SDK must contain the native compiler and Xbox libraries")
    output.mkdir(parents=True, exist_ok=True)
    exe, xex = output / "Burnout_X360.exe", output / "Burnout_X360.xex"
    if not args.assets_only:
        for stale in (exe, xex):
            stale.unlink(missing_ok=True)
    ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    env = dict(os.environ)
    env.update(XEDK=str(sdk), LIB=str(sdk / "lib/xbox"), INCLUDE=str(sdk / "include/xbox"))
    env["PATH"] = os.pathsep.join([str(linker.parent), str(sdk / "bin/win32"),
                                    str(Path(os.environ["SystemRoot"]) / "System32")])
    for name in ("CL", "_CL_", "LINK", "_LINK_"):
        env.pop(name, None)
    report = {"stage": "native-link", "target": "xbox360-ppc-be", "state": "running",
              "sdk": str(sdk), "linker": str(linker), "input_reports": [], "assets": [],
              "complete_game": False, "boot_verified": False}
    report_path = output / "link-results.json"
    try:
        if args.assets:
            report["assets"] = build_assets(args.assets.resolve(), sdk, output, env, args.timeout)
        elif args.asset_report:
            report["assets"] = reuse_assets(args.asset_report.resolve())
            report["asset_report"] = str(args.asset_report.resolve())
        if args.assets_only:
            report["state"] = "assets-verified"
            return 0
        rows = {}
        for path in args.report:
            path = path.resolve()
            data = path.read_bytes()
            compiled = json.loads(data)
            if compiled.get("target") != "xbox360-ppc-be" or compiled.get("state") != "complete":
                raise ValueError("Not a completed native compile report: " + str(path))
            report["input_reports"].append({"path": str(path), "sha256": sha256(data)})
            for row in compiled["results"]:
                source = row["source"].replace("\\", "/")
                if "pc" in [part.lower() for part in Path(source).parts]:
                    raise ValueError("PC source in native report: " + source)
                rows[source] = row
        report["excluded_sources"] = []
        for source in args.exclude_source:
            source = source.replace("\\", "/")
            if source == ENTRY_SOURCE or source not in rows:
                raise ValueError("Cannot exclude native main or an unreported source: " + source)
            rows.pop(source)
            report["excluded_sources"].append(source)
        failures = [name for name, row in rows.items() if not row.get("passed")]
        report["compile_failures"] = failures
        report["source_count"] = len(rows)
        report["objects"] = []
        entry_found = False
        for source, row in rows.items():
            if not row.get("passed"):
                continue
            obj = Path(row["object"])
            if not obj.is_absolute():
                raise ValueError("Object paths in reports must be absolute: " + str(obj))
            data, _, symbols = coff(obj)
            if "main" in symbols:
                if source != ENTRY_SOURCE or entry_found:
                    raise ValueError("Unexpected/duplicate native main definition: " + source)
                entry_found = True
            report["objects"].append({"source": source, "path": str(obj), "sha256": sha256(data)})
        report["native_entry_present"] = entry_found
        report["partial"] = bool(failures or not entry_found or args.diagnose_partial)
        if report["partial"] and not args.diagnose_partial:
            report["state"] = "blocked-compile"
            return 1
        if not linker.is_file():
            raise ValueError("Prepared native linker does not exist: " + str(linker))
        report["linker_sha256"] = sha256(linker.read_bytes())
        libraries = list(dict.fromkeys(SDK_LIBRARIES + args.library))
        for name in libraries:
            if Path(name).name != name or not (sdk / "lib/xbox" / name).is_file():
                raise ValueError("Expected a native SDK library filename: " + name)
        arguments = ["/NOLOGO", "/MACHINE:PPCBE", "/SUBSYSTEM:XBOX", "/ENTRY:mainCRTStartup",
                     "/INCREMENTAL:NO", "/OUT:" + str(exe), "/MAP:" + str(output / "Burnout_X360.map"),
                     "/LIBPATH:" + str(sdk / "lib/xbox")]
        arguments += [r["path"] for r in report["objects"]]
        arguments += [r["object"] for r in report["assets"]]
        arguments += libraries
        response = output / "link.rsp"
        response.write_text("\n".join('"' + a + '"' for a in arguments) + "\n", encoding="utf-8")
        report["link"] = run([str(linker), "@" + str(response)], env, output, output / "link.log", args.timeout)
        diagnostics = report["link"].pop("diagnostics")
        report["diagnostic_counts"] = {code: len(re.findall(r"\b" + code + r"\b", diagnostics))
                                       for code in ("LNK2001", "LNK2019", "LNK2005", "LNK1169", "LNK1120")}
        if report["link"]["returncode"] or not exe.is_file():
            report["state"] = "link-failed"
            return 1
        report["pe"] = {"path": str(exe), "sha256": sha256(exe.read_bytes())}
        if report["partial"]:
            report["state"] = "partial-link-only"
            return 1
        if not args.xex_config:
            report["state"] = "linked-pe-only"
            return 0
        config = args.xex_config.resolve()
        imagexex = sdk / "bin/win32/imagexex.exe"
        if not config.is_file() or not imagexex.is_file():
            raise ValueError("ImageXEX or explicit title configuration is missing")
        report["image"] = run([str(imagexex), "/IN:" + str(exe), "/OUT:" + str(xex),
                               "/CONFIG:" + str(config)], env, output, output / "imagexex.log", args.timeout)
        report["image"].pop("diagnostics")
        report["xex_config"] = {"path": str(config), "sha256": sha256(config.read_bytes())}
        if report["image"]["returncode"] or not xex.is_file() or xex.read_bytes()[:4] != b"XEX2":
            report["state"] = "image-failed"
            return 1
        report["xex"] = {"path": str(xex), "sha256": sha256(xex.read_bytes())}
        report["state"] = "xex-built-unverified"
        return 0
    except (OSError, ValueError, KeyError, struct.error) as error:
        report["state"] = "input-or-tool-error"
        report["error"] = str(error)
        return 1
    finally:
        report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print("%s; %s" % (report["state"], report_path))


if __name__ == "__main__":
    sys.exit(main())
