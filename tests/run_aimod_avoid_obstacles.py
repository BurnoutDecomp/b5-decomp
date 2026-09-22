"""FX-AIMOD G08-D1: replay the production ResetOnTrackManager::AvoidObstacles sweep.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aimod_avoid_obstacles.py [AvoidObstacles.cpp]
Optional argument: a pre-fix snapshot of BrnResetOnTrackManager_AvoidObstacles.cpp (the RED side).
"""
from pathlib import Path
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

AVOID = REPO / "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_AvoidObstacles.cpp"
MANAGER = REPO / "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.cpp"


def main():
    avoid = (Path(sys.argv[1]) if len(sys.argv) > 1 else AVOID).read_text(encoding="utf-8-sig")
    manager = MANAGER.read_text(encoding="utf-8-sig")
    anonymous = definition(avoid, "namespace\n{")
    chunks = ["namespace BrnAI {", anonymous,
              definition(manager, "    AICar* ResetOnTrackManager::GetAICar("),
              definition(avoid, "bool ResetOnTrackManager::TestRecentResets("),
              definition(avoid, "bool ResetOnTrackManager::AvoidObstacles("),
              "}"]
    with tempfile.TemporaryDirectory(prefix="brn_aimod_avoid_") as directory:
        output = Path(directory)
        (output / "restored_methods.inc").write_text("\n".join(chunks), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        sources = [Path(__file__).with_name("AIModAvoidObstacles.cpp"),
                   REPO / "src/GameSource/Math/BrnMathUtils.cpp",
                   REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"]
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" ' + " ".join(f'"{s}"' for s in sources)
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output)
        sys.exit(result.returncode)


if __name__ == "__main__":
    main()
