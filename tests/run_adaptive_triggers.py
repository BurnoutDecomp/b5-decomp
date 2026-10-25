"""Compile and execute the production trigger resolver and packet encoder."""
from pathlib import Path
import os
import subprocess
import tempfile
from fxgs_common import REPO, WORKFLOW, settings


def main():
    with tempfile.TemporaryDirectory(prefix="brn_adaptive_") as directory:
        output = Path(directory)
        executable = output / "adaptive.exe"
        command = ["cl", *settings("msvc_flags.txt"), '/I"' + str(REPO / "src") + '"',
                   '"' + str(Path(__file__).with_name("AdaptiveTriggers.cpp")) + '"',
                   '/Fe:"' + str(executable) + '"']
        script = output / "build.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") +
                          '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + " ".join(command) +
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
