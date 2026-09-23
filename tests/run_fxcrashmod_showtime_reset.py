"""FX-CRASHMOD (crash parity 2026-09-23), G64-D1: replay the production
CrashModule::ProcessCrashedRaceCarEvents (ARTIST 0x827CAAB8) against RaceCarCrashEvent fixtures
(tests/FxCrashmodShowtimeReset.cpp). The Showtime arm must open the record with
KF_PLAYER_SHOWTIME_CAR_RESET_SECONDS == 15.0f (flt_8300E9B0, CRT thunk 0x82C6AC28); the pre-fix
body stored 0.0f.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashmod_showtime_reset.py [--pre-fix <b5 rev>]
--pre-fix reads BrnCrashModule_RaceCarCrashes.cpp from that b5 revision instead of the working tree.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

BASE = "src/GameSource/World/CrashModule/"


def read(rel, pre_fix=False):
    if pre_fix and "--pre-fix" in sys.argv:
        rev = sys.argv[sys.argv.index("--pre-fix") + 1]
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout.replace("\r\n", "\n")
    return (REPO / rel).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def main():
    race = read(BASE + "BrnCrashModule_RaceCarCrashes.cpp", pre_fix=True)
    module = read(BASE + "BrnCrashModule.cpp")
    body = read(BASE + "BrnRaceCarCrash.cpp")
    io = read(BASE + "SharedIO/BrnCrashModuleIO_InputBuffers.cpp")
    methods = []
    constant = re.search(r"^\s*const f32 KF_PLAYER_SHOWTIME_CAR_RESET_SECONDS\s*=[^;]+;", race, re.M)
    if constant:
        methods.append(constant[0].strip())
    methods.append(definition(race, "void CrashModule::ProcessCrashedRaceCarEvents(").replace("CrashModule::", "CrashFixture::"))
    methods.append(definition(module, "    u32 CrashModule::FindCrashForRaceCar(").replace("CrashModule::", "CrashFixture::"))
    methods += ["namespace BrnWorld {",
                definition(body, "    s32 RaceCarCrash::GetOwner()"),
                definition(body, "    void RaceCarCrash::Construct("),
                "}",
                "namespace BrnWorld { namespace CrashIO {",
                "const InputBuffer_PostPhysics::VehicleOutputInterface* "
                + definition(io, "    InputBuffer_PostPhysics::GetVehicleOutputInterface()"),
                "const InputBuffer_PostPhysics::VehicleManagerOutputInterface* "
                + definition(io, "    InputBuffer_PostPhysics::GetVehicleManagerOutputInterface()"),
                "} }"]
    with tempfile.TemporaryDirectory(prefix="brn_fxcrashmod_showtime_") as directory:
        output = Path(directory)
        (output / "fxcrashmod_showtime_methods.inc").write_text("\n".join(methods) + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("FxCrashmodShowtimeReset.cpp")}"'
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
