"""Check where the ICE editor's info list and logo draw their text (ARTIST 0x825346B8,
0x82534934): the production ICEWidgetInfoList.cpp and ICEWidgetLogo.cpp are compiled and
linked against a recording ICERender in IceWidgetTextPlacement.cpp.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from run_showtime_impulse import settings, REPO, WORKFLOW

WIDGETS = REPO / "src/SDKs/Packages/ICE/ICEWidget"


def main():
    sources = [Path(__file__).with_name("IceWidgetTextPlacement.cpp"),
               WIDGETS / "ICEWidgetInfoList.cpp", WIDGETS / "ICEWidgetLogo.cpp"]
    with tempfile.TemporaryDirectory(prefix="brn_ice_text_placement_") as directory:
        output = Path(directory)
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + " "
                   + " ".join(f'"{source}"' for source in sources)
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        sys.exit(subprocess.run(["cmd", "/c", str(script)], cwd=output).returncode)


if __name__ == "__main__":
    main()
