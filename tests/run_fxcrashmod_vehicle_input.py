"""FX-CRASHMOD (crash parity 2026-09-23), G63-D2: replay the production
CrashIO::OutputBuffer_PreScene::Construct (ARTIST 0x827CE9E8) over a 0xCD-poisoned buffer
(tests/FxCrashmodVehicleInput.cpp). The console constructs the embedded
BrnPhysics::Vehicle::VehicleInputInterface at 0x827CEA20; the pre-fix body did not (the member was a
1-byte placeholder). Also appends the constructed crash-side interface into a physics-side one with
the production VehicleInputInterface::Append (0x823C87C0), the bridge's merge.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashmod_vehicle_input.py [--pre-fix <b5 rev>]
--pre-fix reads BrnCrashModuleIO_OutputBuffer_PreScene.cpp from that b5 revision (its Construct body
is replayed against the current header).
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

OUT = "src/GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO_OutputBuffer_PreScene.cpp"
TRAFFIC = "src/GameSource/World/CrashModule/SharedIO/TrafficInputInterface.cpp"
VEHICLE = "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.cpp"


def read(rel, pre_fix=False):
    if pre_fix and "--pre-fix" in sys.argv:
        rev = sys.argv[sys.argv.index("--pre-fix") + 1]
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout.replace("\r\n", "\n")
    return (REPO / rel).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def main():
    out = read(OUT, pre_fix=True)
    traffic = read(TRAFFIC)
    vehicle = read(VEHICLE)
    methods = ["namespace BrnWorld { namespace CrashIO {",
               definition(out, "    void OutputBuffer_PreScene::Construct()"),
               definition(traffic, "void TrafficOutputInterface::Construct()"),
               "} }",
               "namespace BrnPhysics { namespace Vehicle {",
               definition(vehicle, "    void VehicleInputInterface::Construct()"),
               definition(vehicle, "    void VehicleInputInterface::Append("),
               "} }"]
    with tempfile.TemporaryDirectory(prefix="brn_fxcrashmod_vehicle_input_") as directory:
        output = Path(directory)
        (output / "fxcrashmod_vehicle_input_methods.inc").write_text("\n".join(methods) + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("FxCrashmodVehicleInput.cpp")}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp"}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}"'
                   + f' "{REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp"}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + ' >build.log 2>&1\nif errorlevel 1 (type build.log & exit /b 1)\n'
                          + 'regression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=output).returncode
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
