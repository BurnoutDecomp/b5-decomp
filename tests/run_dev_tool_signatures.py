"""Pin the ARTIST-attested parameter lists of six dev-tool functions (the ICE camera editor
and the environment manager's light-setup tool blend) against the production headers.
Compile-only: a failing static_assert in DevToolSignatures.cpp is the RED verdict.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from run_showtime_impulse import settings, WORKFLOW


def main():
    with tempfile.TemporaryDirectory(prefix="brn_dev_tool_signatures_") as directory:
        output = Path(directory)
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' "{Path(__file__).with_name("DevToolSignatures.cpp")}"'
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output)
        if result.returncode != 0:
            print("FAIL dev-tool signatures: see the static_assert messages above")
        sys.exit(result.returncode)


if __name__ == "__main__":
    main()
