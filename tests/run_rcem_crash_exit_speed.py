"""FX-RCEM (crash-parity 2026-09-22): the crash exit's place-on-track speed.

RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents (ARTIST 0x822F3FE0) hands a still-crashing
wreck back to the road with RequestResetOnTrack; with the engine running (meEngineState == 2) the
console's speed is flt_82FAD720 (50 mph) offline or flt_82FAD8C0 (75 mph) online. Both words are
BSS written by CRT initialisers, and until 2026-09-22 the PC pinned the speed to 0.0f.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_rcem_crash_exit_speed.py [CrashExit.cpp snapshot]
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

SOURCE = REPO / "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule_CrashExit.cpp"


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else SOURCE
    source = path.read_text(encoding="utf-8-sig")
    constants = re.findall(r"^\s*const f32 KF_RESET_ON_TRACK_SPEED\w*\s*=[^;]+;", source, re.M)
    method = definition(source, "void RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents(")
    print("constants:", [c.strip() for c in constants])
    with tempfile.TemporaryDirectory(prefix="brn_rcem_crash_exit_") as directory:
        output = Path(directory)
        (output / "rcem_crash_exit_speed.inc").write_text(
            "\n".join(c.strip() for c in constants) + "\n" + method + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("RcemCrashExitSpeed.cpp")}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}"'
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
