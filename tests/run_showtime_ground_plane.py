"""Regression for RaceCarPhysics::UpdateAftertouch @0x8262EBE8 and UpdateTargetAssist @0x8261FF50
(crash parity G37-D2 / G37-D3): the console works in the GROUND PLANE -- it zeroes lane y of the
camera X/Z axes and of (target - car) before normalising / measuring.

Run from the workflow checkout:
    python b5-decomp/tests/run_showtime_ground_plane.py [--pre-fix <b5 rev>]

Both shipped blocks are extracted from RaceCarPhysics.cpp (or from --pre-fix <rev>, the RED side)
and wrapped in two free functions over test doubles.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_rival_impacts import REPO, WORKFLOW, settings

TU = "src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.cpp"


def main():
    rev = sys.argv[sys.argv.index("--pre-fix") + 1] if "--pre-fix" in sys.argv else None
    if rev:
        src = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{TU}"], capture_output=True,
                             text=True, encoding="utf-8", check=True).stdout
    else:
        src = (REPO / TU).read_text(encoding="utf-8-sig")
    src = src.replace("\r\n", "\n")

    # --- camera block: from the first statement touching lpCameraMatrix->xAxis to the line that
    #     normalises camera Z (both spellings).
    aft = src[src.index("void RaceCarPhysics::UpdateAftertouch("):]
    first = min(i for i in (aft.find("CGS_ASSERT(vpu::MagnitudeSquared(lpCameraMatrix->xAxis)"),
                            aft.find("Vector3 lvCameraX = lpCameraMatrix->xAxis;")) if i >= 0)
    last = re.search(r"\n[^\n]*lvCameraZ = vpu::Normalize\([^\n]*\n", aft[first:])
    camera = aft[first:first + last.end()]

    # --- target loop: the candidate for-loop of UpdateTargetAssist.
    uta = src[src.index("void RaceCarPhysics::UpdateTargetAssist("):]
    lstart = uta.index("for (s32 liT = 0; liT < MS.miNumTargets; ++liT)")
    lend = uta.index("MS.miCurrentTargetId =", lstart)
    loop = uta[lstart:lend]
    consts = [m.group(0) for m in re.finditer(r"^\s*static const f32 KF_TARGET_(SCORE_GATE|STICKINESS)\s*=[^;]+;", src, re.M)]

    inc = "\n".join(consts) + "\n"
    inc += ("void CameraAxes(const Matrix44Affine* lpCameraMatrix, Vector3& lrX, Vector3& lrZ)\n{\n"
            + camera + "\n    lrX = lvCameraX; lrZ = lvCameraZ;\n}\n")
    inc += ("s32 PickTarget(const Vector3 lvPosition, const Vector3 lvAimDirection)\n{\n"
            "    s32 liBest = -1; f32 lfBestWeight = 3.4028235e38f;\n" + loop + "\n    return liBest;\n}\n")

    with tempfile.TemporaryDirectory(prefix="brn_stground_") as directory:
        out = Path(directory)
        (out / "extracted.inc").write_text(inc, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        cmd = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + f' /I"{out}" "'
               + str(Path(__file__).with_name("ShowtimeGroundPlane.cpp")) + '" /Fe:regression.exe /link /OPT:REF')
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 1\n' + cmd + ' >build.log 2>&1\n'
                          'if errorlevel 1 (findstr /i /c:"error" build.log & exit /b 1)\n'
                          '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
    sys.exit(rc)


if __name__ == "__main__":
    main()
