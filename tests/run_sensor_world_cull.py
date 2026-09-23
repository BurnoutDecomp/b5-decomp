"""Regression for DeformationSensor::AddContactsToPenetrationSolver's world-contact cull
@0x825E1D20 (crash parity G25-D1).

Run from the workflow checkout:
    python b5-decomp/tests/run_sensor_world_cull.py [--pre-fix <b5 rev>]

Steps (1) partition and (2) cull are extracted verbatim from BrnDeformationSensor.cpp (or from
--pre-fix <rev>, the RED side: the invented near-point merge culls nothing here) and run over
three geometric cases from the verifier: a convex corner (console keeps 1), a concave floor+wall
(keeps 2) and the >= / < arm asymmetry.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_rival_impacts import REPO, WORKFLOW, settings

TU = "src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.cpp"


def main():
    rev = sys.argv[sys.argv.index("--pre-fix") + 1] if "--pre-fix" in sys.argv else None
    if rev:
        src = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{TU}"], capture_output=True,
                             text=True, encoding="utf-8", check=True).stdout
    else:
        src = (REPO / TU).read_text(encoding="utf-8-sig")
    src = src.replace("\r\n", "\n")
    fn = src.index("void DeformationSensor::AddContactsToPenetrationSolver(")
    start = src.index("// --- (1) partition stored contacts", fn)
    end = src.index("// --- (3) feed the WORLD contacts", fn)
    block = src[start:end]
    with tempfile.TemporaryDirectory(prefix="brn_cull_") as directory:
        out = Path(directory)
        (out / "extracted.inc").write_text(block, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        cmd = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + f' /I"{out}" "'
               + str(Path(__file__).with_name("SensorWorldCull.cpp")) + '" /Fe:regression.exe /link /OPT:REF')
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 1\n' + cmd + ' >build.log 2>&1\n'
                          'if errorlevel 1 (findstr /i /c:"error" build.log & exit /b 1)\n'
                          '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
    sys.exit(rc)


if __name__ == "__main__":
    main()
