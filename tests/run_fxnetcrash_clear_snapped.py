"""FX-NETCRASH (crash parity 2026-09-24), G45-D1 / G34-D2: replay the production
PhysicalTrafficManager::ClearSnappedNetworkTrafficContacts (ARTIST 0x825F37F0) with the production
GetPhysicsEntityId (0x825B4980) against real traffic drivers and a recording deformation manager
(tests/FxNetcrashClearSnapped.cpp), and check structurally that
VehicleManager::ClearSnappedNetworkCarContacts (0x8261A8D0) tail-calls
mPhysicalTrafficManager.ClearSnappedNetworkTrafficContacts(lpDeformationManager) after its race-car walk
(0x8261AC1C..0x8261AC28, reached from every exit of the walk).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxnetcrash_clear_snapped.py [--pre-fix <b5 rev>]
--pre-fix reads BrnPhysicalTrafficManager_TrafficEvents.cpp and BrnVehicleManager_CrashState.cpp from
that b5 revision; a missing body is replayed EMPTY (the pre-fix PC ran nothing).
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

BASE = "src/GameSource/Physics/VehicleManager/"


def read(rel, pre_fix=False):
    if pre_fix and "--pre-fix" in sys.argv:
        rev = sys.argv[sys.argv.index("--pre-fix") + 1]
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout.replace("\r\n", "\n")
    return (REPO / rel).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def code_only(text):
    return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"', ' ', text)


def main():
    events = read(BASE + "BrnPhysicalTrafficManager_TrafficEvents.cpp", pre_fix=True)
    crash_state = read(BASE + "BrnVehicleManager_CrashState.cpp", pre_fix=True)
    manager = read(BASE + "BrnPhysicalTrafficManager.cpp")
    failures = []

    methods = []
    diag_max = re.search(r"^\s*const s32 KI_NETCRASH_DIAG_MAX_LINES\s*=[^;]+;", events, re.M)
    if diag_max:
        methods += ["namespace {", definition(events, "    bool NetCrashDiagEnabled()"), diag_max[0].strip(), "}"]
    methods.append("namespace BrnPhysics { namespace Vehicle {")
    methods.append(definition(manager, "EntityId PhysicalTrafficManager::GetPhysicsEntityId(")
                   .replace("PhysicalTrafficManager::", "TrafficFixture::"))
    try:
        body = definition(events, "void PhysicalTrafficManager::ClearSnappedNetworkTrafficContacts(")
        body = (body.replace("PhysicalTrafficManager::", "TrafficFixture::")
                    .replace("Deformation::DeformationManager* lpDeformationManager",
                             "FakeDeformationManager* lpDeformationManager"))
        print("found   PhysicalTrafficManager::ClearSnappedNetworkTrafficContacts")
    except ValueError:
        body = "void TrafficFixture::ClearSnappedNetworkTrafficContacts(FakeDeformationManager*) {}"
        print("MISSING PhysicalTrafficManager::ClearSnappedNetworkTrafficContacts (replayed empty)")
    methods += [body, "} }"]

    # Structural: 0x8261AC1C (every exit of the race-car walk) addis r3,r31,1 ; lwz r4,arg ;
    # addi r3,r3,-0x5120 ; 0x8261AC28 bl ClearSnappedNetworkTrafficContacts.
    caller = code_only(definition(crash_state, "    void VehicleManager::ClearSnappedNetworkCarContacts("))
    loop = re.search(r"for\s*\(\s*s32\s+liRaceCar\s*=", caller)
    call = re.search(r"mPhysicalTrafficManager\s*\.\s*ClearSnappedNetworkTrafficContacts\s*\(\s*lpDeformationManager\s*\)\s*;",
                     caller)
    if not (loop and call and call.start() > loop.start()):
        failures.append("ClearSnappedNetworkCarContacts must tail-call "
                        "mPhysicalTrafficManager.ClearSnappedNetworkTrafficContacts(lpDeformationManager) after the "
                        "race-car walk (console 0x8261AC1C..0x8261AC28)")
    else:
        # The call is the last statement: after it only the closing brace of the function.
        tail = caller[call.end():].strip()
        if tail != "}":
            failures.append("the ClearSnappedNetworkTrafficContacts call must be the function's last statement")
    header = read(BASE + "BrnPhysicalTrafficManager.h", pre_fix=True)
    if not re.search(r"void\s+ClearSnappedNetworkTrafficContacts\s*\(\s*Deformation::DeformationManager\s*\*", header):
        failures.append("BrnPhysicalTrafficManager.h must declare "
                        "void ClearSnappedNetworkTrafficContacts(Deformation::DeformationManager*) (DWARF h:391)")

    with tempfile.TemporaryDirectory(prefix="brn_fxnetcrash_clear_snapped_") as directory:
        output = Path(directory)
        (output / "fxnetcrash_clear_snapped_methods.inc").write_text("\n".join(methods) + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("FxNetcrashClearSnapped.cpp")}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + ' >build.log 2>&1\nif errorlevel 1 (type build.log & exit /b 1)\n'
                          + 'regression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=output).returncode
    for failure in failures:
        print("FAIL:", failure)
    print(f"structural: {len(failures)} failures; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
