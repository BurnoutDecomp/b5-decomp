"""Regression for PhysicalBodyPart::Prepare @0x82626700 clearing mbFrozen (crash parity G29-D1).

Run from the workflow checkout:
    python b5-decomp/tests/run_part_prepare_frozen.py [--pre-fix <b5 rev>]

The member-store block (from `mRigidBodyId = lPartId;` to the `mbJoinedToVehicle = false;` store)
is extracted from BrnPhysicalBodyPart.cpp (or --pre-fix <rev>) and run on a slot whose previous
occupant was frozen.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_rival_impacts import REPO, WORKFLOW, settings

TU = "src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.cpp"


def main():
    rev = sys.argv[sys.argv.index("--pre-fix") + 1] if "--pre-fix" in sys.argv else None
    if rev:
        src = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{TU}"], capture_output=True,
                             text=True, encoding="utf-8", check=True).stdout
    else:
        src = (REPO / TU).read_text(encoding="utf-8-sig")
    src = src.replace("\r\n", "\n")
    fn = src.index("void PhysicalBodyPart::Prepare(")
    a = src.index("mRigidBodyId                 = lPartId;", fn)
    m = re.compile(r"mbJoinedToVehicle\s*= false;[^\n]*\n").search(src, a)
    block = src[a:m.end()]
    with tempfile.TemporaryDirectory(prefix="brn_partprep_") as directory:
        out = Path(directory)
        (out / "extracted.inc").write_text(block, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        cmd = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + f' /I"{out}" "'
               + str(Path(__file__).with_name("PartPrepareFrozen.cpp")) + '" /Fe:regression.exe /link /OPT:REF')
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 1\n' + cmd + ' >build.log 2>&1\n'
                          'if errorlevel 1 (findstr /i /c:"error" build.log & exit /b 1)\n'
                          '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
    sys.exit(rc)


if __name__ == "__main__":
    main()
