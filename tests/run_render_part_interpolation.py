"""Run from the workflow checkout: python b5-decomp/tests/run_render_part_interpolation.py.

Compile the presentation methods verbatim from production sources. Isolating these
definitions avoids linking the full game's unrelated resource/scene dependencies.
The normal build remains the integration check for the complete translation units.
"""
from pathlib import Path
import subprocess
import tempfile


REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent
CAR = REPO / "src/GameSource/World/EntityModules/RaceCarEntityModule"


def definition(path, signature):
    source = path.read_text(encoding="utf-8")
    start = source.index(signature)
    # These namespace-level methods close at column zero; nested blocks do not.
    return source[start:source.index("\n}", start) + 2]


def settings(name):
    return [line for line in (WORKFLOW / "tools/build" / name).read_text().splitlines()
            if line and not line.startswith("#")]


def main():
    with tempfile.TemporaryDirectory(prefix="brn_part_interpolation_") as directory:
        output = Path(directory)
        methods = ['#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"',
                   "namespace BrnWorld {"]
        for name in ("RestoreTickRenderPose", "LatchTickRenderPose",
                     "ApplyRenderPoseInterpolation", "ResetRenderPoseInterpolation"):
            methods.append(definition(CAR / "BrnActiveRaceCar.cpp", "void ActiveRaceCar::" + name + "("))
        methods.append(definition(CAR / "BrnActiveRaceCarRenderParams.cpp",
                                  "Matrix44Affine& ActiveRaceCar::RenderParams::GetWheelTransform("))
        methods.append("}")
        extracted = output / "methods.cpp"
        extracted.write_text("\n".join(methods), encoding="utf-8")
        sources = [Path(__file__).with_name("RenderPartInterpolation.cpp"), extracted,
                   REPO / "src/GameShared/GameClasses/System/Timer/CgsFrameInterpolation.cpp",
                   REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp"]
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + " "
                   + " ".join(f'"{path}"' for path in sources)
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
