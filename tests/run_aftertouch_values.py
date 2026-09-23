"""Regression for BrnPlayerDriverControls::GetAftertouchValues @0x825B2E88 (crash parity G47-D1).

Run from the workflow checkout:
    python b5-decomp/tests/run_aftertouch_values.py [--pre-fix <b5 rev>]

The console leaf selects the aftertouch Y formula on the controls' own mbIsSteeringWheel
(`lbz r11,0x41(r3)`); the tree branched on the unread trailing bool instead. Links the shipped
BrnPlayerDriverControls.cpp (or that file at --pre-fix <rev>, the RED side).
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_rival_impacts import REPO, WORKFLOW, settings

TU = "src/GameSource/Physics/VehicleManager/SharedIO/BrnPlayerDriverControls.cpp"


def main():
    rev = sys.argv[sys.argv.index("--pre-fix") + 1] if "--pre-fix" in sys.argv else None
    with tempfile.TemporaryDirectory(prefix="brn_aftertouch_") as directory:
        out = Path(directory)
        if rev:
            text = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{TU}"], capture_output=True,
                                  text=True, encoding="utf-8", check=True).stdout
            tu = out / "BrnPlayerDriverControls.cpp"
            tu.write_text(text, encoding="utf-8")
        else:
            tu = REPO / TU
        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        cmd = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + " "
               + f'"{Path(__file__).with_name("AftertouchValues.cpp")}" "{tu}"'
               + " /Fe:regression.exe /link /OPT:REF")
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 1\n' + cmd + ' >build.log 2>&1\n'
                          'if errorlevel 1 (findstr /i /c:"error" build.log & exit /b 1)\n'
                          '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
    sys.exit(rc)


if __name__ == "__main__":
    main()
