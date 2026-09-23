"""FX-FX (crash parity 2026-09-23): BrnEffects::BrnCrashTriangleCache regressions.

Compiles tests/FxFxCrashTriangleCache.cpp against the PRODUCTION BrnCrashTriangleCache.cpp/.h --
the working tree, or any b5 revision with --rev (the RED side of a fix is `--rev <fix commit>~1`)
-- plus src/SharedClasses/World/BrnCollisionTag.cpp, with the canonical MSVC flags, and runs it.
The revision's header and .cpp are staged in a scratch include root that shadows src/, so the
whole shipped TU is what gets tested. A revision that cannot build the harness is a failure.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfx_crash_triangle_cache.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import subprocess
import sys
import tempfile

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent
HEADER = "src/GameSource/Effects/BrnCrashTriangleCache.h"
SOURCE = "src/GameSource/Effects/BrnCrashTriangleCache.cpp"
COLLISION_TAG = REPO / "src/SharedClasses/World/BrnCollisionTag.cpp"
HARNESS = Path(__file__).with_name("FxFxCrashTriangleCache.cpp")


def read(rev, relative):
    if rev is None:
        return (REPO / relative).read_text(encoding="utf-8-sig")
    result = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{relative}"],
                            capture_output=True, text=True, encoding="utf-8", check=True)
    text = result.stdout
    return text[1:] if text.startswith("﻿") else text


def settings(name):
    return [line for line in (WORKFLOW / "tools/build" / name).read_text().splitlines()
            if line and not line.startswith("#")]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="b5 revision to test instead of the working tree")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="brn_fxfx_tricache_") as directory:
        out = Path(directory)
        root = out / "override"
        (root / "GameSource/Effects").mkdir(parents=True)
        (root / "GameSource/Effects/BrnCrashTriangleCache.h").write_text(read(args.rev, HEADER), encoding="utf-8")
        staged = root / "GameSource/Effects/BrnCrashTriangleCache.cpp"
        staged.write_text(read(args.rev, SOURCE), encoding="utf-8")

        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt"))
                   + " /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public"
                   + f' /I"{root}" ' + includes
                   + f' "{HARNESS}" "{staged}" "{COLLISION_TAG}" /Fe:regression.exe /link /OPT:REF')
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 3\n' + command + ' >build.log 2>&1\n'
                          'if errorlevel 1 (findstr /i /c:" error " build.log & exit /b 2)\n'
                          '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
    label = args.rev or "working tree"
    if rc == 2:
        print(f"RED ({label}): the harness does not build against this revision")
    elif rc == 3:
        print("msvc_env.bat could not locate MSVC")
    sys.exit(rc)


if __name__ == "__main__":
    main()
