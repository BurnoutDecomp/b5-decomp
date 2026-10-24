"""Run the production world-grid sampler; --pre-fix REV checks a historical body."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

from run_showtime_impulse import REPO, WORKFLOW, settings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pre-fix")
    args = parser.parse_args()
    relative = "src/GameShared/GameClasses/World/CgsWorldMap2D.cpp"
    source = (REPO / relative).read_text(encoding="utf-8")
    if args.pre_fix:
        source = subprocess.run(["git", "-C", str(REPO), "show", f"{args.pre_fix}:{relative}"],
                                check=True, capture_output=True, text=True, encoding="utf-8").stdout

    with tempfile.TemporaryDirectory(prefix="brn_world_map_2d_") as directory:
        output = Path(directory)
        production = output / "methods.cpp"
        production.write_text(source, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' "{Path(__file__).with_name("WorldMap2D.cpp")}" "{production}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + ' >build.log 2>&1\nif errorlevel 1 (type build.log & exit /b 1)\n'
                          + 'regression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        env = dict(os.environ)
        env.pop("NoDefaultCurrentDirectoryInExePath", None)
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output, env=env)
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
