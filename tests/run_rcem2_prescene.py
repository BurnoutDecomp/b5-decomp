"""FX-RCEM2 (crash-parity 2026-09-23): replay the production ActiveRaceCar::Update_PreScene
(ARTIST 0x822EAE08), RaceCarEntityModule::UpdateRaceCars_PreScene (0x822F5578) and
HandleGameActions case 29 (0x8230C770) against fixtures (tests/Rcem2PreScene.cpp) -- G61-D2
(mbCrashedIntoWater cleared every PreScene), G61-D3 (the online state arms) and G68-D5 (the
intro countdown that releases the rivals). Structural checks: PreSceneUpdate calls
UpdateRaceCars_PreScene in its un-paused arm before UpdateInAndOutOfRangeCars (0x8230E288), and
Construct seeds mfResetOnWaterHeight = 1.75f (0x822FE034) and mfIntroTimer = -1.0f (0x822FE2B0).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_rcem2_prescene.py [--pre-fix <b5 rev>]
A function the source does not define is replayed as an EMPTY body (the pre-fix PC ran nothing),
and a missing case arm as `default: break;`.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW
from run_rcem2_game_actions import switch_arm, code_mask

DIR = "src/GameSource/World/EntityModules/RaceCarEntityModule/"
MODULE = DIR + "BrnRaceCarEntityModule.cpp"
ACTIVE = DIR + "BrnActiveRaceCar.cpp"

EMPTY_UPDATE = ("void ActiveRaceCar::Update_PreScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*, "
                "BrnPhysics::Vehicle::VehicleInputInterface*, BrnAI::AIModuleIO::RaceCarAIInterface*) {}\n")
EMPTY_RACECARS = ("void RaceCarEntityModule::UpdateRaceCars_PreScene("
                  "RaceCarEntityModuleIO::OutputBuffer_PreScene*) {}\n")


def read(rel):
    if "--pre-fix" in sys.argv:
        rev = sys.argv[sys.argv.index("--pre-fix") + 1]
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout.replace("\r\n", "\n")
    return (REPO / rel).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def main():
    module = read(MODULE)
    active = read(ACTIVE)
    failures = []

    try:
        update = definition(active, "void ActiveRaceCar::Update_PreScene(")
        print("found   ActiveRaceCar::Update_PreScene")
    except ValueError:
        update = EMPTY_UPDATE
        print("MISSING ActiveRaceCar::Update_PreScene (replayed empty)")
    try:
        racecars = definition(module, "void RaceCarEntityModule::UpdateRaceCars_PreScene(")
        print("found   RaceCarEntityModule::UpdateRaceCars_PreScene")
    except ValueError:
        racecars = EMPTY_RACECARS
        print("MISSING RaceCarEntityModule::UpdateRaceCars_PreScene (replayed empty)")
    handler = definition(module, "void RaceCarEntityModule::HandleGameActions(")
    arm = switch_arm(handler, r"BrnGameState::GameStateModuleIO::E_ACTION_START_MODE_INTRO")
    print(("found   " if arm else "MISSING ") + "arm E_ACTION_START_MODE_INTRO")
    actions = ("void RaceCarEntityModule::Dispatch(s32 liType, const CgsModule::Event* lpEvent, "
               "OutputFixture* lpOutput)\n{\n    (void)lpEvent; (void)lpOutput;\n    switch (liType)\n    {\n"
               + (arm or "") + "\n    default:\n        break;\n    }\n}\n")

    # Structural: the console's call site 0x8230E288 -- un-paused arm, before the range pass.
    prescene = code_mask(definition(module, "void RaceCarEntityModule::PreSceneUpdate("))
    call = re.search(r"if\s*\(\s*\(\s*lUpdateSet\s*&\s*1\s*\)\s*==\s*0\s*\)\s*\{?\s*UpdateRaceCars_PreScene\s*\(\s*lpOutput\s*\)", prescene)
    rng = prescene.find("UpdateInAndOutOfRangeCars(")
    if not call or rng < 0 or call.start() > rng:
        failures.append("PreSceneUpdate must call UpdateRaceCars_PreScene(lpOutput) under !(lUpdateSet & 1) "
                        "before UpdateInAndOutOfRangeCars (console bl @0x8230E288)")
    construct = code_mask(definition(module, "void RaceCarEntityModule::Construct()"))
    if not re.search(r"mfResetOnWaterHeight\s*=\s*1\.75f\s*;", construct):
        failures.append("G61-D2 Construct must seed mfResetOnWaterHeight = 1.75f (flt_82004F68 @0x822FE034)")
    if not re.search(r"mfIntroTimer\s*=\s*-1\.0f\s*;", construct):
        failures.append("G68-D5 Construct must seed mfIntroTimer = -1.0f (flt_820037C8 @0x822FE2B0)")

    with tempfile.TemporaryDirectory(prefix="brn_rcem2_prescene_") as directory:
        output = Path(directory)
        (output / "rcem2_update_prescene.inc").write_text(update + "\n", encoding="utf-8")
        (output / "rcem2_update_racecars_prescene.inc").write_text(racecars + "\n", encoding="utf-8")
        (output / "rcem2_prescene_actions.inc").write_text(actions, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("Rcem2PreScene.cpp")}"'
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
