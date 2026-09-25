"""FX-GATE: compile the shipped CgsSystem::Time header (src/GameShared/GameClasses/System/Timer/CgsTime.h)
with the canonical build flags and check its unordered (NaN) arms against the X360 bodies
(FxGateTimeCompare.cpp): the three `t >= 0` asserts are `fcmpu ; bge -> past` (Time(f32) @0x821F1FA0,
SetFloatVal @0x821F2140, operator=(f32) @0x8230E500), the carries of operator+ @0x8230E5E0 and
operator+= @0x8230E7B8 are `fcmpu f,1.0 ; blt -> no carry`, and the inlined operator>= / operator<=
are `li 1 ; fcmpu ; bge|ble keep` (0x823958AC, 0x82550418).

usage: run_fxgate_time_compare.py [--rev <b5-decomp git revision>]
    default   the working tree's CgsTime.h
    --rev R   the header as of revision R (placed first on the include path), to show the pre-fix
              header fails.
"""
from pathlib import Path
import argparse
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import settings, REPO, WORKFLOW

HEADER = "src/GameShared/GameClasses/System/Timer/CgsTime.h"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="b5-decomp revision to take CgsTime.h from")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="brn_fxgate_time_") as directory:
        output = Path(directory)
        includes = []
        if args.rev:
            header = output / "inc" / "GameShared" / "GameClasses" / "System" / "Timer" / "CgsTime.h"
            header.parent.mkdir(parents=True)
            header.write_bytes(subprocess.run(["git", "-C", str(REPO), "show", f"{args.rev}:{HEADER}"],
                                              check=True, capture_output=True).stdout)
            includes.append(output / "inc")   # first, so the revision's header wins
        includes += [WORKFLOW / path for path in settings("msvc_includes.txt")]
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " "
                   + " ".join(f'/I"{path}"' for path in includes) + " "
                   + f'"{Path(__file__).with_name("FxGateTimeCompare.cpp")}"'
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
