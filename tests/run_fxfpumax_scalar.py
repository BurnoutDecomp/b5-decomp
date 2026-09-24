"""FX-FPUMAX: compile the shipped rw::math::fpu scalar header (vendor/renderware/include/rw/math/fpu/
scalar_operation.h) with the canonical build flags and check Min / Max / Clamp against an fsel oracle
(FxFpumaxScalar.cpp): rwmath 1.02.00 scalar.h's float/double forms are `fsel(a - b, b, a)` /
`fsel(a - b, a, b)` and Clamp = Min(max, Max(min, value)); the console inlines exactly that
(0x822B21FC, 0x8271F574).

usage: run_fxfpumax_scalar.py [--rev <b5-decomp git revision>]
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
    with tempfile.TemporaryDirectory(prefix="brn_fxfpumax_") as directory:
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
                   + f'"{Path(__file__).with_name("FxFpumaxScalar.cpp")}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        sys.exit(subprocess.run(["cmd", "/c", str(script)], cwd=output).returncode)


if __name__ == "__main__":
    main()
