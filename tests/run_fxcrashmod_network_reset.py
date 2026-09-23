"""FX-CRASHMOD (crash parity 2026-09-23), G64-D4 / G65-D1 / G65-D2: replay the production
CrashModule::ResetCrashedNetworkRaceCars (ARTIST 0x827CE6E0), CrashModule::OnContactFromNetworkPlayer
(0x827C62E8) and the RaceCarCrash / TrafficCrash::ResetNetworkTimeout bodies against a real
driver-update queue (tests/FxCrashmodNetworkReset.cpp), and check structurally that PreSceneUpdate
calls ResetCrashedNetworkRaceCars(lpInput, lpOutput) under mbIsOnlineGameMode, between
ClearUpRecycledTraffic and the mbClearUpEnabled arm (0x827D3B4C..0x827D3B74).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashmod_network_reset.py [--pre-fix <b5 rev>]
--pre-fix reads BrnCrashModule_RaceCarCrashes.cpp from that b5 revision. A function it does not define
is replayed as an EMPTY body (the pre-fix PC ran nothing); the IO accessors always come from the tree.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

BASE = "src/GameSource/World/CrashModule/"

EMPTY = {
    "void CrashModule::ResetCrashedNetworkRaceCars(":
        "void CrashFixture::ResetCrashedNetworkRaceCars(const CrashIO::InputBuffer_PreScene*, CrashIO::OutputBuffer_PreScene*) {}",
    "void CrashModule::OnContactFromNetworkPlayer(":
        "void CrashFixture::OnContactFromNetworkPlayer(EActiveRaceCarIndex) {}",
}


def read(rel, pre_fix=False):
    if pre_fix and "--pre-fix" in sys.argv:
        rev = sys.argv[sys.argv.index("--pre-fix") + 1]
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout.replace("\r\n", "\n")
    return (REPO / rel).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def code_only(text):
    return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"', ' ', text)


def main():
    race = read(BASE + "BrnCrashModule_RaceCarCrashes.cpp", pre_fix=True)
    module = read(BASE + "BrnCrashModule.cpp")
    body = read(BASE + "BrnRaceCarCrash.cpp")
    traffic = read(BASE + "BrnTrafficCrash.cpp")
    io = read(BASE + "SharedIO/BrnCrashModuleIO_InputBuffers.cpp")
    out = read(BASE + "SharedIO/BrnCrashModuleIO_OutputBuffer_PreScene.cpp")
    failures = []

    methods = []
    constant = re.search(r"^\s*const f32 KF_NETWORK_CRASH_TIMEOUT\s*=[^;]+;", race, re.M)
    methods.append(constant[0].strip() if constant else "const f32 KF_NETWORK_CRASH_TIMEOUT = 20.0f;")
    methods.append("namespace BrnWorld {")
    for signature, stub in (("void RaceCarCrash::ResetNetworkTimeout()", "void RaceCarCrash::ResetNetworkTimeout() {}"),
                            ("void TrafficCrash::ResetNetworkTimeout()", "void TrafficCrash::ResetNetworkTimeout() {}")):
        try:
            methods.append(definition(race, signature))
            print("found  ", signature)
        except ValueError:
            methods.append(stub)
            print("MISSING", signature, "(replayed empty)")
    methods += [definition(body, "    s32 RaceCarCrash::GetOwner()"),
                definition(traffic, "    void TrafficCrash::Construct("), "}"]
    for signature, stub in EMPTY.items():
        try:
            methods.append(definition(race, signature).replace("CrashModule::", "CrashFixture::"))
            print("found  ", signature)
        except ValueError:
            methods.append(stub)
            print("MISSING", signature, "(replayed empty)")
    methods.append(definition(race, "void CrashModule::ResetRaceCarFromCrashIndex(").replace("CrashModule::", "CrashFixture::"))
    methods.append(definition(module, "    u32 CrashModule::FindCrashForRaceCar(").replace("CrashModule::", "CrashFixture::"))
    methods += ["namespace BrnWorld { namespace CrashIO {",
                "const RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* "
                + definition(io, "    InputBuffer_PreScene::GetActiveRaceCarInterface()"),
                "const InputBuffer_PreScene::VehicleDriverInterface* "
                + definition(io, "    InputBuffer_PreScene::GetVehicleDriverInterface()"),
                definition(io, "    void InputBuffer_PreScene::SetVehicleDriverInterface("),
                "RaceCarOutputInterface* "
                + definition(out, "    OutputBuffer_PreScene::GetRaceCarOutputInterface()         "),
                "} }"]

    # Structural: console PreSceneUpdate 0x827D3B48 ClearUpRecycledTraffic ; 0x827D3B4C lbz 0x1529 ;
    # 0x827D3B74 bl ResetCrashedNetworkRaceCars (r4 = lpInput, r5 = lpOutput) ; 0x827D3B78 lbz 0x152B.
    prescene = code_only(definition(race, "void CrashModule::PreSceneUpdate("))
    recycled = prescene.find("ClearUpRecycledTraffic(")
    online = re.search(r"if\s*\(\s*mbIsOnlineGameMode\s*\)\s*\{", prescene)
    call = re.search(r"ResetCrashedNetworkRaceCars\s*\(\s*lpInput\s*,\s*lpOutput\s*\)", prescene)
    cleanup = re.search(r"if\s*\(\s*mbClearUpEnabled\s*\)", prescene)
    if not (online and call and cleanup and 0 <= recycled < online.start() < call.start() < cleanup.start()):
        failures.append("PreSceneUpdate must call ResetCrashedNetworkRaceCars(lpInput, lpOutput) inside "
                        "`if (mbIsOnlineGameMode)`, after ClearUpRecycledTraffic and before the "
                        "mbClearUpEnabled arm (console 0x827D3B4C..0x827D3B78)")

    with tempfile.TemporaryDirectory(prefix="brn_fxcrashmod_network_reset_") as directory:
        output = Path(directory)
        (output / "fxcrashmod_network_reset_methods.inc").write_text("\n".join(methods) + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("FxCrashmodNetworkReset.cpp")}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}"'
                   + f' "{REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp"}"'
                   + f' "{REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.cpp"}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Module/VariableEventQueue_5040_16.cpp"}"'
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
