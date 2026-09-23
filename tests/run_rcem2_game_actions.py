"""FX-RCEM2 (crash-parity 2026-09-23): replay the production arms of
RaceCarEntityModule::HandleGameActions (ARTIST 0x8230BE08) for G68-D3 (case 7's drive-thru speeds),
G68-D4 (case 35), G68-D6 (case 205 + the HandlePrepareForModeAction stash), G68-D7 (case 97) and
G68-D8 (cases 128/140/144/201/273) against fixture race cars (tests/Rcem2GameActions.cpp).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_rcem2_game_actions.py [--pre-fix <b5 rev>]
An arm the source does not have is replayed as the console's `default: break;`, so the pre-fix
source reports per-check failures instead of failing to extract.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

DIR = "src/GameSource/World/EntityModules/RaceCarEntityModule/"
MODULE = DIR + "BrnRaceCarEntityModule.cpp"
ARMING = DIR + "BrnRaceCarEntityModule_ModeArming.cpp"
BOOST = DIR + "Boost/BrnBoostManager.h"
LABELS = [r"BrnGameState::GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER",
          r"BrnGameState::GameStateModuleIO::E_ACTION_FINISHED_MODE_NOTIFY",
          r"BrnGameState::GameStateModuleIO::E_ACTION_BODY_SHOP_DRIVE_THRU",
          r"BrnGameState::GameStateModuleIO::E_ACTION_ROAD_RAGE_PLAYER_DAMAGE",
          r"BrnGameState::GameStateModuleIO::E_ACTION_OVERHEAD_SIGN_HIT",
          r"BrnGameState::GameStateModuleIO::E_ACTION_VEHICLE_HIT",
          r"BrnGameState::GameStateModuleIO::E_ACTION_JUST_BOUNCED",
          r"BrnGameState::GameStateModuleIO::E_ACTION_EVENT_AT_JUNCTION_AVAILABLE",
          r"KI_ACTION_ROAD_RULES_ENTER_ROAD"]
CONSTANTS = ["KF_DRIVE_THRU_ENTRY_SPEED", "KU_BODY_SHOP_ACTION_ENTITY_ID_OFFSET",
             "KI_ACTION_ROAD_RULES_ENTER_ROAD"]


def read(rel):
    if "--pre-fix" in sys.argv:
        rev = sys.argv[sys.argv.index("--pre-fix") + 1]
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout.replace("\r\n", "\n")
    return (REPO / rel).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def code_mask(text):
    def blank(match):
        return re.sub(r"[^\n]", " ", match.group(0))
    return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', blank, text)


def switch_arm(body, label):
    masked = code_mask(body)
    found = re.search(r"case\s+" + label + r"\s*:", masked)
    if not found:
        return None
    depth, position = 0, found.end()
    token = re.compile(r"[{}]|\bcase\b|\bdefault\s*:")
    while True:
        match = token.search(masked, position)
        if match is None:
            raise ValueError("unterminated arm " + label)
        text = match.group(0)
        if text == "{":
            depth += 1
        elif text == "}":
            depth -= 1
            if depth < 0:
                return body[found.start():match.start()]
        elif depth == 0:
            return body[found.start():match.start()]
        position = match.end()


def main():
    module = read(MODULE)
    arming = read(ARMING)
    boost = read(BOOST)
    failures = []
    handler = definition(module, "void RaceCarEntityModule::HandleGameActions(")
    pieces = []
    for name in CONSTANTS:
        constant = re.search(r"^\s*const (?:f32|u32|s32) " + name + r"\s*=[^;]+;", module, re.M)
        if constant:
            pieces.append(constant.group(0).strip())
    arms = []
    for label in LABELS:
        arm = switch_arm(handler, label)
        print(("found   " if arm else "MISSING ") + "arm " + label.split("::")[-1])
        if arm:
            arms.append(arm)
    pieces.append("void RaceCarEntityModule::Dispatch(s32 liType, const CgsModule::Event* lpEvent, "
                  "OutputFixture* lpOutput)\n{\n    (void)lpEvent; (void)lpOutput;\n    switch (liType)\n    {\n"
                  + "\n".join(arms) + "\n    default:\n        break;\n    }\n}\n")
    try:
        bounce = definition(boost, "    void OnBounceBoost()")
    except ValueError:
        bounce = ""
        print("MISSING BoostManager::OnBounceBoost")

    # G68-D6 (structural): HandlePrepareForModeAction pushes the live base-deformation pair into
    # the saved pair (0x82309554..0x82309570) -- the pair action 205's TOTALLED arm restores.
    prepare = code_mask(definition(arming, "void RaceCarEntityModule::HandlePrepareForModeAction("))
    if (not re.search(r"miPlayerBaseDeformationTypeSaved\s*=\s*miPlayerBaseDeformationTypeMirror\s*;", prepare)
            or not re.search(r"mfPlayerBaseDeformAmountSaved\s*=\s*mfPlayerBaseDeformAmountMirror\s*;", prepare)):
        failures.append("G68-D6 HandlePrepareForModeAction must stash the live mirror pair into the saved pair "
                        "(stwx +0x184D4 @0x82309568, stfsx +0x184E0 @0x82309570)")

    with tempfile.TemporaryDirectory(prefix="brn_rcem2_game_actions_") as directory:
        output = Path(directory)
        (output / "rcem2_game_actions.inc").write_text("\n".join(pieces), encoding="utf-8")
        (output / "rcem2_bounce_boost.inc").write_text(bounce + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("Rcem2GameActions.cpp")}"'
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
