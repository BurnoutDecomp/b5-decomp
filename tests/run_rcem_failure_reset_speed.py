"""FX-RCEM (crash-parity 2026-09-22): the FAILURE-arm speed cap of
RaceCarEntityModule::ProcessResetOnTrackResultQueue (ARTIST 0x822F4580).

flt_82FAD610 is BSS (reads 0.0 in the image) and its writer is the CRT initialiser at
0x82C4BB10 (0.44704 * 10.0 == 10 mph); until 2026-09-22 the PC pinned it to 0.0f, so every car
the AI could not place came back at rest.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_rcem_failure_reset_speed.py [ResetPump.cpp snapshot]
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

SOURCE = REPO / "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule_ResetPump.cpp"


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else SOURCE
    source = path.read_text(encoding="utf-8-sig")
    constant = re.search(r"const f32 KF_FAILURE_RESET_SPEED_CAP\s*=[^;]+;", source).group(0)
    queue = definition(source, "void RaceCarEntityModule::ProcessResetOnTrackResultQueue(")
    statement = re.search(r"lfAppliedSpeed\s*=\s*\(\s*lrResult\.GetResetSpeed\(\)[^;]+;", queue).group(0)
    print("constant: ", constant)
    print("statement:", " ".join(statement.split()))
    body = (constant + "\n"
            "f32 FailureArmSpeed(const ResetOnTrackResult& lrResult)\n{\n    f32 lfAppliedSpeed;\n    "
            + statement + "\n    return lfAppliedSpeed;\n}\n")
    with tempfile.TemporaryDirectory(prefix="brn_rcem_failure_speed_") as directory:
        output = Path(directory)
        (output / "rcem_failure_reset_speed.inc").write_text(body, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("RcemFailureResetSpeed.cpp")}"'
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
