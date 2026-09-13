"""Compile the production impact classifiers verbatim with observed-output test doubles.
Run from the workflow checkout: python b5-decomp/tests/run_rival_impacts.py.
The full exe build separately checks the real dependency and type integration.
"""
from pathlib import Path
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent
SOURCE = REPO / "src/GameSource/Physics/VehicleManager/BrnVehicleManager.cpp"

def definition(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n    }", start) + 6]

def settings(name):
    return [line for line in (WORKFLOW / "tools/build" / name).read_text().splitlines()
            if line and not line.startswith("#")]

def main():
    source = SOURCE.read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix="brn_rival_impacts_") as directory:
        output = Path(directory)
        # Prefix contains production vector helpers and the threshold constants.
        methods = [source[:source.index("    // [td-contact] / [td-crash]")]]
        start = source.index("    static const f32 KF_SPEED_UNIT_SCALE")
        end = source.index("    // EntityId packing helper", start)
        methods.append(source[start:end])
        methods.append(definition(source, "    static inline EntityId MakeRaceCarEntityId("))
        for name in ("CheckForShuntAndNudge", "CheckForSlamAndTradingPaint", "CheckForHeadToHead",
                     "CheckForStationaryTargetTakedown", "CheckForPlayerSlammingAIIntoAI",
                     "CheckForHittingAlreadyCrashingCar"):
            methods.append(definition(source, "    bool VehicleManager::" + name + "("))
        methods.append("} }")
        sensor = (REPO / "src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.cpp").read_text(encoding="utf-8")
        start = sensor.index("\tvoid DeformationSensor::OutputContactSpy(")
        end = sensor.index("\n\t}", start) + 3
        methods.append('#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h"\n'
                       '#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"\n'
                       '#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"\n'
                       'namespace BrnPhysics { namespace Deformation {\n' + sensor[start:end] + '\n} }')
        extracted = output / "methods.cpp"
        extracted.write_text("\n".join(methods), encoding="utf-8")
        sources = [Path(__file__).with_name("RivalImpacts.cpp"), extracted]
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
