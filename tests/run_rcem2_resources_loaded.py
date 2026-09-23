"""FX-RCEM2 (crash-parity 2026-09-23, G61-D6): replay the production ActiveRaceCar::OnResourcesLoaded
(ARTIST 0x822EB168) on a fixture car (tests/Rcem2ResourcesLoaded.cpp): ResetVerletOffsets must run
(0x822EB404) after the state/handle stores and before the detached-part queue Construct (0x822EB40C).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_rcem2_resources_loaded.py [--pre-fix <b5 rev>]
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

REL = "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.cpp"


def read(rel):
    if "--pre-fix" in sys.argv:
        rev = sys.argv[sys.argv.index("--pre-fix") + 1]
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout.replace("\r\n", "\n")
    return (REPO / rel).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def main():
    body = definition(read(REL), "void ActiveRaceCar::OnResourcesLoaded(")
    with tempfile.TemporaryDirectory(prefix="brn_rcem2_resources_loaded_") as directory:
        output = Path(directory)
        (output / "rcem2_resources_loaded.inc").write_text(body + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("Rcem2ResourcesLoaded.cpp")}"'
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
