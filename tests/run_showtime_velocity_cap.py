"""Regression for RaceCarPhysics::CapShowtimeVelocities @0x825D7600 (crash parity G37-D1).

Run from the workflow checkout:
    python b5-decomp/tests/run_showtime_velocity_cap.py [--pre-fix <b5 rev>]

The console rebuilds the capped linear velocity and then, when the car was DESCENDING
(0 > vel.y), puts the original vertical speed back (0x825D7908 vcmpgtfp. ; 0x825D791C vrlimi128
mask 4). The tree stored the rebuilt vector whole, so a falling Showtime car was slowed to the cap
and hung in the air. The shipped body and its five cap constants are extracted from
RaceCarPhysics.cpp (or from --pre-fix <rev>, the RED side).
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_rival_impacts import REPO, WORKFLOW, settings
from run_showtime_impulse import definition

TU = "src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.cpp"


def main():
    rev = sys.argv[sys.argv.index("--pre-fix") + 1] if "--pre-fix" in sys.argv else None
    if rev:
        src = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{TU}"], capture_output=True,
                             text=True, encoding="utf-8", check=True).stdout
    else:
        src = (REPO / TU).read_text(encoding="utf-8-sig")
    src = src.replace("\r\n", "\n")
    consts = [m.group(0) for m in re.finditer(r"^\s*static const f32 KF_CAP_[A-Z_]+\s*=[^;]+;", src, re.M)]
    body = definition(src, "    void RaceCarPhysics::CapShowtimeVelocities()")
    with tempfile.TemporaryDirectory(prefix="brn_stcap_") as directory:
        out = Path(directory)
        (out / "extracted.inc").write_text("\n".join(consts) + "\n" + body + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        cmd = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + f' /I"{out}" "'
               + str(Path(__file__).with_name("ShowtimeVelocityCap.cpp")) + '" /Fe:regression.exe /link /OPT:REF')
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 1\n' + cmd + ' >build.log 2>&1\n'
                          'if errorlevel 1 (findstr /i /c:"error" build.log & exit /b 1)\n'
                          '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
    sys.exit(rc)


if __name__ == "__main__":
    main()
