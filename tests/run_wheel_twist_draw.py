"""Regression for DeformableObject::UpdateWheels' twist-limit draw @0x82626580 (crash parity G19-D2).

Run from the workflow checkout:
    python b5-decomp/tests/run_wheel_twist_draw.py [--pre-fix <b5 rev>]

The console inlines RandomVecFloat (vector-slot ring draw); the tree called RandomFloat (scalar
cursor), which reads a different slot for cursors 1-3 and 5-7. The shipped `lfTwistLimit =`
statement and KVF_MAX_TWIST_ANGLE are extracted from BrnDeformableObject_Update.cpp (or from
--pre-fix <rev>) and run against the real CgsRandom.cpp.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_rival_impacts import REPO, WORKFLOW, settings

TU = "src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject_Update.cpp"


def main():
    rev = sys.argv[sys.argv.index("--pre-fix") + 1] if "--pre-fix" in sys.argv else None
    if rev:
        src = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{TU}"], capture_output=True,
                             text=True, encoding="utf-8", check=True).stdout
    else:
        src = (REPO / TU).read_text(encoding="utf-8-sig")
    const = re.search(r"static const VecFloat KVF_MAX_TWIST_ANGLE\s*=[^;]+;", src).group(0)
    stmt = re.search(r"const f32 lfTwistLimit = [^;]+;", src).group(0)
    inc = (const + "\n"
           "f32 TwistLimit(CgsNumeric::Random* lpRandom)\n{\n    " + stmt + "\n    return lfTwistLimit;\n}\n")
    with tempfile.TemporaryDirectory(prefix="brn_twist_") as directory:
        out = Path(directory)
        (out / "extracted.inc").write_text(inc, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        sources = [Path(__file__).with_name("WheelTwistDraw.cpp"),
                   REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"]
        cmd = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + f' /I"{out}" '
               + " ".join(f'"{s}"' for s in sources) + " /Fe:regression.exe /link /OPT:REF")
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 1\n' + cmd + ' >build.log 2>&1\n'
                          'if errorlevel 1 (findstr /i /c:"error" build.log & exit /b 1)\n'
                          '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
    sys.exit(rc)


if __name__ == "__main__":
    main()
