"""Run production active-car output Clear and model/rival getters. Optional source snapshot."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_render_part_interpolation import CAR, REPO, WORKFLOW, definition, settings


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else CAR / "SharedIO/BrnRCEntityActiveRaceCarOutputInterface.cpp"
    with tempfile.TemporaryDirectory(prefix="brn_active_clear_") as directory:
        output = Path(directory)
        methods = [definition(path, prefix + "RCEntityActiveRaceCarOutputInterface::" + name + "(")
                   for prefix, name in (("void ", "Clear"), ("CgsID ", "GetCarModelId"), ("CgsID ", "GetRivalId"))]
        (output / "active_output_clear_methods.inc").write_text("\n".join(methods), encoding="utf-8")
        sources = [Path(__file__).with_name("ActiveRaceCarOutputClear.cpp"),
                   REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp"]
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" ' + " ".join(f'"{path}"' for path in sources)
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
