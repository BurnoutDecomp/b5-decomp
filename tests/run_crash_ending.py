"""Exercise the production crash-ending producer and game-state relay with real queues.
--baseline-producer/--baseline-relay reproduce each former missing link.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW


def main():
    base = REPO / "src/GameSource/World/CrashModule"
    source = (base / "BrnCrashModule_RaceCarCrashes.cpp").read_text(encoding="utf-8-sig")
    producer = definition(source, "void CrashModule::PostPhysicsUpdate(").replace("CrashModule::", "CrashFixture::")
    if "--baseline-producer" in sys.argv:
        a = producer.index("            // ARTIST 827D3CD0")
        b = producer.index("            mbNeedToSendEndingMessage = false;", a)
        producer = producer[:a] + producer[b:]
    source = (REPO / "src/GameSource/GameState/GameStateModule_gUI_00.cpp").read_text(encoding="utf-8-sig")
    relay = definition(source, "void GameStateModule::ProcessGameEventsVehicleImpactBringUp(").replace("GameStateModule::", "GameStateFixture::")
    if "--baseline-relay" in sys.argv:
        a = relay.index("        else if (liType == GameStateModuleIO::E_EVENT_PLAYER_CRASH_ENDING)")
        b = relay.index("        const CgsModule::Event* lpCurrent", a)
        relay = relay[:a] + relay[b:]
    source = (base / "SharedIO/BrnCrashModuleIO_OutputBuffer_PostPhysics.cpp").read_text(encoding="utf-8-sig")
    getter = definition(source, "    OutputBuffer_PostPhysics::GameEventQueue* OutputBuffer_PostPhysics::GetGameEventQueue()")
    methods = producer + relay + "\nnamespace BrnWorld { namespace CrashIO {\n" + getter + "\n}}"
    io = (base / "SharedIO/BrnCrashModuleIO_InputBuffers.cpp").read_text(encoding="utf-8-sig")
    methods += "\nnamespace BrnWorld { namespace CrashIO {\nconst TrafficInputInterface* " + definition(io, "    InputBuffer_PostPhysics::GetTrafficInputInterface()")
    methods += "const InputBuffer_PostPhysics::VehicleManagerOutputInterface* " + definition(io, "    InputBuffer_PostPhysics::GetVehicleManagerOutputInterface()") + "\n}}"
    with tempfile.TemporaryDirectory(prefix="brn_crash_ending_") as directory:
        output = Path(directory)
        (output / "crash_ending_methods.inc").write_text(methods, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        sources = [Path(__file__).with_name("CrashEnding.cpp"),
                   REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp",
                   REPO / "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp",
                   REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"]
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" ' + " ".join(f'"{s}"' for s in sources)
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
