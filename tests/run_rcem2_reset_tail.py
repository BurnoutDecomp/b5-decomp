"""FX-RCEM2 (crash-parity 2026-09-23): replay the production tail of
RaceCarEntityModule::ResetActiveRaceCar's live-car arm (ARTIST 0x822F4B2C..0x822F4C40) --
G67-D4 (mabResetThisFrame.SetBit) and G67-D5 (a deformation reset clears the glass render state)
-- against fixture cars (tests/Rcem2ResetTail.cpp).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_rcem2_reset_tail.py [--pre-fix <b5 rev> | <snapshot.cpp>]
The extracted text runs from the `lpVehicleInputInterface->ResetRaceCar(` statement to the arm's
first `return;`, so the pre-fix source (which had neither leg) still compiles and reports
per-check failures.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

REL = "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp"


def read_source():
    if "--pre-fix" in sys.argv:
        rev = sys.argv[sys.argv.index("--pre-fix") + 1]
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{REL}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    path = Path(args[0]) if args else REPO / REL
    return path.read_text(encoding="utf-8-sig")


def main():
    source = read_source().replace("\r\n", "\n")
    body = definition(source, "void RaceCarEntityModule::ResetActiveRaceCar(")
    start = re.search(r"^[ \t]*lpVehicleInputInterface->ResetRaceCar\(", body, re.M)
    if start is None:
        print("FAIL: no ResetRaceCar call in ResetActiveRaceCar")
        sys.exit(1)
    end = body.index("return;", start.start()) + len("return;")
    tail = body[start.start():end]
    with tempfile.TemporaryDirectory(prefix="brn_rcem2_reset_tail_") as directory:
        output = Path(directory)
        (output / "rcem2_reset_tail.inc").write_text(tail + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("Rcem2ResetTail.cpp")}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + ' >build.log 2>&1\nif errorlevel 1 (type build.log & exit /b 1)\n'
                          + 'regression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output)
        sys.exit(result.returncode)


if __name__ == "__main__":
    main()
