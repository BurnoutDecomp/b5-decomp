"""FX-NETCRASH (crash parity 2026-09-24), G64-D2 / G65-D2 part 2: replay the production
CrashModule::HandleNetworkCrashingTraffic (ARTIST 0x827CB788) against the real crash-side
NetworkInputInterface, OutputBuffer_PreScene queues and TrafficCrash / Set / FastBitArray containers
(tests/FxNetcrashHandleTraffic.cpp), and check structurally that PreSceneUpdate calls
HandleNetworkCrashingTraffic(lpInput, lpOutput) inside `if (mbIsOnlineGameMode)`, after
ClearUpRecycledTraffic and BEFORE ResetCrashedNetworkRaceCars (0x827D3B4C..0x827D3B74).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxnetcrash_handle_traffic.py [--pre-fix <b5 rev>]
--pre-fix reads BrnCrashModule_RaceCarCrashes.cpp and BrnTrafficCrash.cpp from that b5 revision; a
missing body is replayed EMPTY (the pre-fix PC ran nothing: the call was parked).
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


def code_only(text):
    return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"', ' ', text)


def body_or_stub(source, signature, stub, rename=None):
    try:
        text = definition(source, signature)
        print("found  ", signature.strip())
    except ValueError:
        print("MISSING", signature.strip(), "(replayed empty)")
        return stub
    return text.replace(*rename) if rename else text


def main():
    race = read(BASE + "BrnCrashModule_RaceCarCrashes.cpp", pre_fix=True)
    traffic = read(BASE + "BrnTrafficCrash.cpp", pre_fix=True)
    traffic_crashes = read(BASE + "BrnCrashModule_TrafficCrashes.cpp")
    racecar = read(BASE + "BrnRaceCarCrash.cpp")
    io = read(BASE + "SharedIO/BrnCrashModuleIO_InputBuffers.cpp")
    out = read(BASE + "SharedIO/BrnCrashModuleIO_OutputBuffer_PreScene.cpp")
    failures = []

    methods = []
    constant = re.search(r"^\s*const f32 KF_NETWORK_CRASH_TIMEOUT\s*=[^;]+;", race, re.M)
    methods.append(constant[0].strip() if constant else "const f32 KF_NETWORK_CRASH_TIMEOUT = 20.0f;")
    diag_max = re.search(r"^\s*const s32 KI_NETCRASH_DIAG_MAX_LINES\s*=[^;]+;", race, re.M)
    if diag_max:
        methods.append(definition(race, "    bool NetCrashDiagEnabled()"))
        methods.append(diag_max[0].strip())
    methods.append("namespace BrnWorld {")
    methods.append(body_or_stub(race, "void RaceCarCrash::ResetNetworkTimeout()",
                                "void RaceCarCrash::ResetNetworkTimeout() {}"))
    methods.append(body_or_stub(race, "void TrafficCrash::ResetNetworkTimeout()",
                                "void TrafficCrash::ResetNetworkTimeout() {}"))
    methods.append(definition(racecar, "    s32 RaceCarCrash::GetOwner()"))
    methods.append(definition(traffic, "    void TrafficCrash::Construct("))
    methods.append(body_or_stub(traffic, "    void TrafficCrash::ConfirmNetworkOwner(",
                                "void TrafficCrash::ConfirmNetworkOwner(EActiveRaceCarIndex) {}"))
    methods.append(body_or_stub(traffic, "    void TrafficCrash::SetNetworkVehicleClearedUp(",
                                "void TrafficCrash::SetNetworkVehicleClearedUp() {}"))
    methods.append("}")
    methods.append(body_or_stub(race, "void CrashModule::HandleNetworkCrashingTraffic(",
                                "void CrashFixture::HandleNetworkCrashingTraffic(const CrashIO::InputBuffer_PreScene*, "
                                "CrashIO::OutputBuffer_PreScene*) {}",
                                ("CrashModule::", "CrashFixture::")))
    methods.append(definition(race, "void CrashModule::OnContactFromNetworkPlayer(").replace("CrashModule::", "CrashFixture::"))
    methods.append(definition(traffic_crashes, "    u32 CrashModule::FindCrashForTrafficVehicle(").replace("CrashModule::", "CrashFixture::"))
    methods += ["namespace BrnWorld { namespace CrashIO {",
                "const NetworkInputInterface* "
                + definition(io, "    InputBuffer_PreScene::GetNetworkInputInterface()"),
                "TrafficOutputInterface* "
                + definition(out, "    OutputBuffer_PreScene::GetTrafficOutputInterface()         "),
                "OutputBuffer_PreScene::VehicleInputInterface* "
                + definition(out, "    OutputBuffer_PreScene::GetVehicleInputInterface()         "),
                "} }"]

    # Structural: console PreSceneUpdate 0x827D3B48 ClearUpRecycledTraffic ; 0x827D3B4C lbz 0x1529 ;
    # 0x827D3B64 bl HandleNetworkCrashingTraffic (r4 = lpInput, r5 = lpOutput) ; 0x827D3B74 bl
    # ResetCrashedNetworkRaceCars ; 0x827D3B78 lbz 0x152B (mbClearUpEnabled).
    prescene = code_only(definition(race, "void CrashModule::PreSceneUpdate("))
    recycled = prescene.find("ClearUpRecycledTraffic(")
    online = re.search(r"if\s*\(\s*mbIsOnlineGameMode\s*\)\s*\{", prescene)
    call = re.search(r"HandleNetworkCrashingTraffic\s*\(\s*lpInput\s*,\s*lpOutput\s*\)", prescene)
    reset = re.search(r"ResetCrashedNetworkRaceCars\s*\(\s*lpInput\s*,\s*lpOutput\s*\)", prescene)
    cleanup = re.search(r"if\s*\(\s*mbClearUpEnabled\s*\)", prescene)
    if not (online and call and reset and cleanup
            and 0 <= recycled < online.start() < call.start() < reset.start() < cleanup.start()):
        failures.append("PreSceneUpdate must call HandleNetworkCrashingTraffic(lpInput, lpOutput) inside "
                        "`if (mbIsOnlineGameMode)`, after ClearUpRecycledTraffic and before "
                        "ResetCrashedNetworkRaceCars (console 0x827D3B4C..0x827D3B74)")
    if re.search(r"network arm PARK", race):
        failures.append("PreSceneUpdate still carries the '[crash-exit] CrashModule network arm PARK' one-shot")

    with tempfile.TemporaryDirectory(prefix="brn_fxnetcrash_handle_traffic_") as directory:
        output = Path(directory)
        (output / "fxnetcrash_handle_traffic_methods.inc").write_text("\n".join(methods) + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("FxNetcrashHandleTraffic.cpp")}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp"}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}"'
                   + f' "{REPO / "src/GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.cpp"}"'
                   + f' "{REPO / "src/GameSource/World/CrashModule/SharedIO/NetworkInputInterface.cpp"}"'
                   + f' "{REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp"}"'
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
