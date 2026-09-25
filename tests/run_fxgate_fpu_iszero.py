"""FX-GATE: compile the shipped rw::math::fpu scalar header (vendor/renderware/include/rw/math/fpu/
scalar_operation.h) with the canonical build flags and check IsZero against the console's inlined
predicate (FxGateFpuIsZero.cpp): `fcmpu x,+2^-23 ; bgt -> not zero ; fcmpu x,-2^-23 ; bge -> zero`
(TrafficLaneTruck::Update 0x82247C18..0x82247C34, and five other sites named in the .cpp), which answers
TRUE for a NaN.

usage: run_fxgate_fpu_iszero.py [--rev <b5-decomp git revision>]
    default   the working tree's scalar_operation.h
    --rev R   the header as of revision R (e.g. the fix commit's parent, to show the pre-fix header
              fails); it is placed first on the include path.
"""
from pathlib import Path
import argparse
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import settings, REPO, WORKFLOW

HEADER = "vendor/renderware/include/rw/math/fpu/scalar_operation.h"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="b5-decomp revision to take scalar_operation.h from")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="brn_fxgate_iszero_") as directory:
        output = Path(directory)
        includes = []
        if args.rev:
            header = output / "inc" / "rw" / "math" / "fpu" / "scalar_operation.h"
            header.parent.mkdir(parents=True)
            header.write_bytes(subprocess.run(["git", "-C", str(REPO), "show", f"{args.rev}:{HEADER}"],
                                              check=True, capture_output=True).stdout)
            includes.append(output / "inc")   # first, so the revision's header wins
        includes += [WORKFLOW / path for path in settings("msvc_includes.txt")]
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " "
                   + " ".join(f'/I"{path}"' for path in includes) + " "
                   + f'"{Path(__file__).with_name("FxGateFpuIsZero.cpp")}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + ' >build.log 2>&1\nif errorlevel 1 (type build.log & exit /b 1)\n'
                          + 'regression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        sys.exit(subprocess.run(["cmd", "/c", str(script)], cwd=output).returncode)


if __name__ == "__main__":
    main()
