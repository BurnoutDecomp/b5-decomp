"""Check production crash actions and cleanup with real event queues.
--baseline replays the former parked HandleGameActions path.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW


def main():
    base = REPO / "src/GameSource/World/CrashModule"
    source = (base / "BrnCrashModule.cpp").read_text(encoding="utf-8-sig")
    methods = "\n".join(definition(source, signature).replace("CrashModule::", "CrashFixture::") for signature in (
        "    void CrashModule::HandleGameActions(", "    void CrashModule::ForceClearupAllCrashes(",
        "    void CrashModule::OnNetworkPlayerDisconnected(", "    void CrashModule::OnTrafficCarRemovedFromCrash(",
        "    u32 CrashModule::FindCrashForRaceCar("))
    traffic_module = (base / "BrnCrashModule_TrafficCrashes.cpp").read_text(encoding="utf-8-sig")
    for name in ("FindCrashForTrafficVehicle", "AddCrashingTrafficVehicle", "ProcessSlammedTrafficEvents", "HandleNewCrashingTraffic", "HandleRecoveredSlammedTraffic", "HandleCleanedUpTrafficEvents", "ClearUpRecycledTraffic"):
        prefix = "    u32 " if name == "FindCrashForTrafficVehicle" else "    void "
        methods += "\n" + definition(traffic_module, prefix + "CrashModule::" + name + "(").replace("CrashModule::", "CrashFixture::")
    if "--baseline-traffic-lifecycle" in sys.argv:
        methods = methods.replace(definition(methods, "    void CrashFixture::HandleNewCrashingTraffic("), "void CrashFixture::HandleNewCrashingTraffic(const CrashIO::InputBuffer_PostPhysics*) {}")
    if "--baseline" in sys.argv:
        methods = methods.replace(definition(methods, "    void CrashFixture::HandleGameActions("),
            "void CrashFixture::HandleGameActions(const CrashIO::InputBuffer_PreScene*, CrashIO::OutputBuffer_PreScene*) {}")
    race = (base / "BrnCrashModule_RaceCarCrashes.cpp").read_text(encoding="utf-8-sig")
    methods += "\n" + definition(race, "void CrashModule::ResetRaceCarFromCrashIndex(").replace("CrashModule::", "CrashFixture::")
    tick = definition(race, "void CrashModule::TickCrashes(").replace("CrashModule::", "CrashFixture::")
    if "--baseline-traffic-tick" in sys.argv:
        tick = tick.replace("mTrafficCrashes.GetItem(luCrash).Tick(lfTimeStep);", "(void)luCrash;")
    methods += "\n" + tick
    cleanup = definition(race, "void CrashModule::ClearupCrashes(").replace("CrashModule::", "CrashFixture::")
    if "--baseline-proximity" in sys.argv:
        cleanup = cleanup.replace("lbClearUp = !(rw::math::vpu::Dot(lSeparation, lSeparation) < 1600.0f &&", "lbClearUp = true || !(rw::math::vpu::Dot(lSeparation, lSeparation) < 1600.0f &&")
    methods += "\n" + cleanup
    body = (base / "BrnRaceCarCrash.cpp").read_text(encoding="utf-8-sig")
    traffic = (base / "BrnTrafficCrash.cpp").read_text(encoding="utf-8-sig")
    methods += "\nnamespace BrnWorld {\n" + definition(body, "    s32 RaceCarCrash::GetOwner()")
    methods += definition(body, "    void RaceCarCrash::SetSecondsBeforeCleanup(")
    methods += definition(body, "    void RaceCarCrash::Tick(")
    methods += definition(traffic, "    void TrafficCrash::Construct(")
    methods += definition(traffic, "    void TrafficCrash::Tick(")
    methods += definition(traffic, "    void TrafficCrash::MarkVehicleAsOnscreen()")
    methods += definition(traffic, "    void TrafficCrash::OnOwnerDisconnected()") + "\n}"
    io = (base / "SharedIO/BrnCrashModuleIO_InputBuffers.cpp").read_text(encoding="utf-8-sig")
    out = (base / "SharedIO/BrnCrashModuleIO_OutputBuffer_PreScene.cpp").read_text(encoding="utf-8-sig")
    methods += "\nnamespace BrnWorld { namespace CrashIO {\n"
    methods += definition(io, "    void InputBuffer_PreScene::SetGameActionQueue(")
    methods += definition(io, "    const InputBuffer_PreScene::GameActionQueue* InputBuffer_PreScene::GetGameActionQueue()")
    methods += "\nTrafficOutputInterface* " + definition(out, "    OutputBuffer_PreScene::GetTrafficOutputInterface()         ")
    methods += "\nRaceCarOutputInterface* " + definition(out, "    OutputBuffer_PreScene::GetRaceCarOutputInterface()         ") + "\n}}"
    methods += "\nnamespace BrnWorld { namespace CrashIO {\nconst RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* " + definition(io, "    InputBuffer_PreScene::GetActiveRaceCarInterface()") + "\n}}"
    methods += "\nnamespace BrnWorld { namespace CrashIO {\nconst CgsSystem::TimerStatusInterface* " + definition(io, "    InputBuffer_PreScene::GetTimerStatusInterface()") + "\n}}"
    methods += "\nnamespace BrnWorld { namespace CrashIO {\nconst TrafficInputInterface* " + definition(io, "    InputBuffer_PostPhysics::GetTrafficInputInterface()")
    methods += "const InputBuffer_PostPhysics::VehicleManagerOutputInterface* " + definition(io, "    InputBuffer_PostPhysics::GetVehicleManagerOutputInterface()") + "\n}}"
    active = (REPO / "src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRCEntityActiveRaceCarOutputInterface.cpp").read_text(encoding="utf-8-sig")
    methods += "\nnamespace BrnWorld { namespace RaceCarEntityModuleIO {\n"
    for signature in ("EActiveRaceCarIndex RCEntityActiveRaceCarOutputInterface::GetPlayerActiveRaceCarIndex(", "bool RCEntityActiveRaceCarOutputInterface::IsCarInShowtime(", "bool RCEntityActiveRaceCarOutputInterface::IsPlayerCarActive(", "bool RCEntityActiveRaceCarOutputInterface::IsRaceCarRival(", "bool RCEntityActiveRaceCarOutputInterface::IsRaceCarNetwork(", "Vector3 RCEntityActiveRaceCarOutputInterface::GetPlayerPosition(", "Vector3 RCEntityActiveRaceCarOutputInterface::GetPlayerDirection("):
        methods += definition(active, signature)
    methods += "const RCEntityActiveRaceCarOutputInterface::RaceCarState* " + definition(active, "RCEntityActiveRaceCarOutputInterface::GetRaceCarState(") + "\n}}"
    actions = (REPO / "src/GameSource/GameState/BrnGameActions.cpp").read_text(encoding="utf-8-sig")
    methods += "\nnamespace BrnGameState { namespace GameStateModuleIO {\n" + definition(actions, "const GameModeParams* PrepareForModeAction::GetGameModeParams()") + "\n}}"
    with tempfile.TemporaryDirectory(prefix="brn_crash_actions_") as directory:
        output = Path(directory)
        (output / "crash_actions_methods.inc").write_text(methods, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("CrashActions.cpp")}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}"'
                   + f' "{REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp"}"'
                   + ' /Fe:regression.exe /link /OPT:REF')
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
