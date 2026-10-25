"""Check real snapshot projection and semantic event routing for PC feedback."""
from pathlib import Path
import os
import subprocess
import tempfile
from fxgs_common import REPO, WORKFLOW, STRSTREAM_CPP, settings


def main():
    with tempfile.TemporaryDirectory(prefix="brn_vehicle_feedback_") as directory:
        output = Path(directory)
        executable = output / "vehicle_feedback.exe"
        includes = " ".join('/I"' + str(WORKFLOW / path) + '"' for path in settings("msvc_includes.txt"))
        command = ('cl ' + ' '.join(settings("msvc_flags.txt")) + ' ' + includes +
                   ' "' + str(Path(__file__).with_name("VehicleFeedback.cpp")) + '" "' +
                   str(REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp") + '" "' +
                   str(STRSTREAM_CPP) + '" /Fe:"' + str(executable) + '" /link /OPT:REF')
        script = output / "build.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") +
                          '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command +
                          '\nexit /b %ERRORLEVEL%\n', newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output, capture_output=True, text=True)
        if result.returncode:
            print(result.stdout, result.stderr)
            return result.returncode
        environment = os.environ.copy()
        environment.pop("NoDefaultCurrentDirectoryInExePath", None)
        return subprocess.run([str(executable)], cwd=output, env=environment, timeout=30).returncode


if __name__ == "__main__":
    raise SystemExit(main())
