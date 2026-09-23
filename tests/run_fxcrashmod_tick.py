"""FX-CRASHMOD (crash parity 2026-09-23), G63-D1: replay the production RaceCarCrash::Tick
(ARTIST 0x827BF0B8) against RaceCarState fixtures (tests/FxCrashmodTick.cpp). The console's
"still moving" test reads RaceCarState+0x340 == mAngularVelocity (0x827BF20C) against 1.5f
(flt_820CA5C4); the pre-fix body read mLinearVelocity (+0x330).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashmod_tick.py [--pre-fix <b5 rev>]
--pre-fix reads BrnRaceCarCrash.cpp from that b5 revision instead of the working tree.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

BODY = "src/GameSource/World/CrashModule/BrnRaceCarCrash.cpp"
ACTIVE = "src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRCEntityActiveRaceCarOutputInterface.cpp"


def read(rel, pre_fix=True):
    if pre_fix and "--pre-fix" in sys.argv:
        rev = sys.argv[sys.argv.index("--pre-fix") + 1]
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout.replace("\r\n", "\n")
    return (REPO / rel).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def main():
    body = read(BODY)
    active = read(ACTIVE, pre_fix=False)
    methods = ['#include "GameShared/GameClasses/Core/CgsAssert.h"',
               "namespace BrnWorld {",
               definition(body, "    void RaceCarCrash::SetSecondsBeforeCleanup("),
               definition(body, "    void RaceCarCrash::Tick("),
               "}",
               "namespace BrnWorld { namespace RaceCarEntityModuleIO {",
               definition(active, "EActiveRaceCarIndex RCEntityActiveRaceCarOutputInterface::GetPlayerActiveRaceCarIndex("),
               "const RCEntityActiveRaceCarOutputInterface::RaceCarState* "
               + definition(active, "RCEntityActiveRaceCarOutputInterface::GetRaceCarState("),
               "} }"]
    with tempfile.TemporaryDirectory(prefix="brn_fxcrashmod_tick_") as directory:
        output = Path(directory)
        (output / "fxcrashmod_tick_methods.inc").write_text("\n".join(methods) + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("FxCrashmodTick.cpp")}"'
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
