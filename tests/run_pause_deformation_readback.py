"""Exercise the production pause gate and deformation L3/L4/L6 with real render data.

Optional argument supplies a pre-fix BrnRaceCarEntityModule.cpp snapshot. The
publisher omits unrelated L1 vehicle state, glass and locators; their dependencies
are covered by the canonical TU/shipping build, not this focused regression.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_render_part_interpolation import CAR, REPO, WORKFLOW, definition, settings


def section(source, first, last):
    start = source.index(first)
    return source[start:source.index(last, start)]


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else CAR / "BrnRaceCarEntityModule.cpp"
    source = path.read_text(encoding="utf-8-sig")
    post = source[source.index("void RaceCarEntityModule::PostPhysicsUpdate("):]
    readback = source[source.index("void RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics("):]
    gate = section(post, "    // ⭐⭐ THE PHYSICS READBACK,", "    // ⭐⭐ THE CAR'S PER-FRAME SCENE PUBLISH,")
    legs = (section(readback, "    // ---- L3 :", "    // ---- L5 :")
            + section(readback, "    // ---- L6 :", "\n}"))
    with tempfile.TemporaryDirectory(prefix="brn_pause_readback_") as directory:
        output = Path(directory)
        methods = ["void PhysicsReadbackFixture::TickReadback(PhysicsReadbackInput* lpInput, bool lbSimPaused) {",
                   gate, "}",
                   "void PhysicsReadbackFixture::ReadUpdatedActiveRaceCarDataFromPhysics(PhysicsReadbackInput* lpInput) {",
                   "++readbacks;",
                   "for (auto& lrCar : cars) lrCar.GetRenderParams()->GetDetachedPartQueue().Clear();",
                   "const auto* lpDeformationEM = &lpInput->deformation;", legs, "}"]
        (output / "pause_readback_methods.inc").write_text("\n".join(methods), encoding="utf-8")
        active = ['#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"',
                  '#include "GameShared/GameClasses/Development/Log/CgsLog.h"',
                  "#include <cstdlib>", "#include <cstring>", "namespace BrnWorld {"]
        for name in ("RestoreTickRenderPose", "LatchTickRenderPose", "ApplyRenderPoseInterpolation",
                     "ResetRenderPoseInterpolation", "UpdateWheelPhysicsState"):
            active.append(definition(CAR / "BrnActiveRaceCar.cpp", "void ActiveRaceCar::" + name + "("))
        active.append(definition(CAR / "BrnActiveRaceCar.cpp", "bool ActiveRaceCar::IsActive("))
        active.append(definition(CAR / "BrnActiveRaceCarRenderParams.cpp",
                                 "Matrix44Affine& ActiveRaceCar::RenderParams::GetWheelTransform("))
        active.append("}")
        extracted = output / "active.cpp"
        extracted.write_text("\n".join(active), encoding="utf-8")
        sources = [Path(__file__).with_name("PauseDeformationReadback.cpp"), extracted,
                   REPO / "src/GameShared/GameClasses/System/Timer/CgsFrameInterpolation.cpp",
                   REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp",
                   REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"]
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
