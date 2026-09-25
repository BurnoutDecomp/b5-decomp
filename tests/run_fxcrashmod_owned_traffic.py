"""FX-CRASHMOD (crash parity 2026-09-23), G64-D3 / G65-D3: replay the production
CrashModule::GenerateOwnedTrafficUpdates (ARTIST 0x827C53F0) against a real physical-traffic-state
queue and the real NetworkOutputInterface (tests/FxCrashmodOwnedTraffic.cpp), and check structurally
that PostPhysicsUpdate calls GenerateOwnedTrafficUpdates(lpInput, lpOutput) under mbIsOnlineGameMode,
after HandleCleanedUpTrafficEvents and before the mbNeedToSendEndingMessage post
(0x827D3CA4..0x827D3CC4).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashmod_owned_traffic.py [--pre-fix <b5 rev>]
--pre-fix reads BrnCrashModule_RaceCarCrashes.cpp from that b5 revision; a missing body is replayed
EMPTY (the pre-fix PC ran nothing).
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

BASE = "src/GameSource/World/CrashModule/"
EMPTY = ("void CrashFixture::GenerateOwnedTrafficUpdates(const CrashIO::InputBuffer_PostPhysics*, "
         "CrashIO::OutputBuffer_PostPhysics*) {}")


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
    traffic = read(BASE + "BrnTrafficCrash.cpp")
    io = read(BASE + "SharedIO/BrnCrashModuleIO_InputBuffers.cpp")
    post = read(BASE + "SharedIO/BrnCrashModuleIO_OutputBuffer_PostPhysics.cpp")
    network = read(BASE + "SharedIO/NetworkOutputInterface.cpp")
    failures = []

    try:
        body = definition(race, "void CrashModule::GenerateOwnedTrafficUpdates(").replace("CrashModule::", "CrashFixture::")
        print("found   CrashModule::GenerateOwnedTrafficUpdates")
    except ValueError:
        body = EMPTY
        print("MISSING CrashModule::GenerateOwnedTrafficUpdates (replayed empty)")
    # crash parity FX-NETCRASH: the body carries a capped [netcrash] PC witness (BRN_NETCRASH_DIAG,
    # silent here -- gpDebugPrint is null); its file-local switch comes with it.
    helpers = []
    if "NetCrashDiagEnabled(" in body:
        helpers = [definition(race, "    bool NetCrashDiagEnabled()"),
                   re.search(r"^\s*const s32 KI_NETCRASH_DIAG_MAX_LINES\s*=[^;]+;", race, re.M)[0].strip()]
    methods = helpers + [body,
               definition(module, "    bool CrashModule::WillTrafficVehicleBeRecycledNextFrame(").replace("CrashModule::", "CrashFixture::"),
               "namespace BrnWorld {",
               definition(traffic, "    void TrafficCrash::Construct("),
               "namespace CrashIO {",
               "const InputBuffer_PostPhysics::VehicleOutputInterface* "
               + definition(io, "    InputBuffer_PostPhysics::GetVehicleOutputInterface()"),
               definition(post, "    NetworkOutputInterface* OutputBuffer_PostPhysics::GetNetworkOutputInterface()"),
               re.search(r"^const u32 KU_MAX_TOTAL_TRAFFIC = 600;", network, re.M)[0],
               definition(network, "void NetworkOutputInterface::Construct()"),
               definition(network, "void NetworkOutputInterface::Clear()"),
               definition(network, "void NetworkOutputInterface::AddOwnedTrafficUpdate("),
               "const NetworkOutputInterface::CrashingTrafficUpdateQueue* "
               + definition(network, "NetworkOutputInterface::GetCrashingTrafficUpdateQueue() const"),
               "} }"]

    # Structural: console PostPhysicsUpdate 0x827D3CA4 HandleCleanedUpTrafficEvents ; 0x827D3CA8
    # lbz 0x1529 ; 0x827D3CC0 bl GenerateOwnedTrafficUpdates(r4 = in, r5 = out) ; 0x827D3CC4 lbz 0x152E.
    postphysics = code_only(definition(race, "void CrashModule::PostPhysicsUpdate("))
    cleaned = postphysics.find("HandleCleanedUpTrafficEvents(")
    online = re.search(r"if\s*\(\s*mbIsOnlineGameMode\s*\)\s*\{?", postphysics)
    call = re.search(r"GenerateOwnedTrafficUpdates\s*\(\s*lpInput\s*,\s*lpOutput\s*\)", postphysics)
    ending = re.search(r"if\s*\(\s*mbNeedToSendEndingMessage\s*\)", postphysics)
    if not (online and call and ending and 0 <= cleaned < online.start() < call.start() < ending.start()):
        failures.append("PostPhysicsUpdate must call GenerateOwnedTrafficUpdates(lpInput, lpOutput) inside "
                        "`if (mbIsOnlineGameMode)`, after HandleCleanedUpTrafficEvents and before the "
                        "mbNeedToSendEndingMessage post (console 0x827D3CA8..0x827D3CC4)")

    with tempfile.TemporaryDirectory(prefix="brn_fxcrashmod_owned_traffic_") as directory:
        output = Path(directory)
        (output / "fxcrashmod_owned_traffic_methods.inc").write_text("\n".join(methods) + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("FxCrashmodOwnedTraffic.cpp")}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp"}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}"'
                   + f' "{REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp"}"'
                   + f' "{REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnPhysicalTrafficState.cpp"}"'
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
