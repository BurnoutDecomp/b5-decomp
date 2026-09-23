"""FX-RCEM3 (crash-parity 2026-09-23): replay the production arms of
RaceCarEntityModule::HandleGameActions (ARTIST 0x8230BE08) against fixture race cars
(tests/FxRcem3GameActions.cpp).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem3_game_actions.py [--pre-fix <b5 rev>]
An arm the source does not have is replayed as the console's `default: break;`, so the pre-fix
source reports per-check failures instead of failing to extract.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, code_mask, definition, pre_fix_rev, read, switch_arm

MODULE = RCEM + "BrnRaceCarEntityModule.cpp"
LABELS = [r"BrnGameState::GameStateModuleIO::E_ACTION_START_PLAYING_MODE",
          r"KI_ACTION_CAR_SELECT_MODIFICATION_SCREEN",
          r"KI_ACTION_UPCOMING_ROAD_CHANGE",
          r"KI_ACTION_SWITCH_CAR_CORONAS_ON_OFF",
          r"KI_ACTION_CAR_SELECT_TRANSITION_IN",
          r"BrnGameState::GameStateModuleIO::E_ACTION_PAINT_SHOP_DRIVE_THRU",
          r"BrnGameState::GameStateModuleIO::E_ACTION_AWARD_SEQUENCE_START",
          r"BrnGameState::GameStateModuleIO::E_ACTION_AWARD_SEQUENCE_END",
          r"KI_ACTION_SET_SIXAXIS_STEERING",
          r"KI_ACTION_SWITCH_CAR_COLOUR",
          r"KI_ACTION_SET_BOOST",
          r"KI_ACTION_WAIT_FOR_STREAMING",
          r"KI_ACTION_LOAD_PROFILE"]
CONSTANTS = ["KF_MIN_STUNT_RESET_SPEED", "KI_ACTION_CAR_SELECT_MODIFICATION_SCREEN",
             "KI_ACTION_UPCOMING_ROAD_CHANGE", "KI_ACTION_SWITCH_CAR_CORONAS_ON_OFF",
             "KI_ACTION_CAR_SELECT_TRANSITION_IN", "KI_ACTION_SET_SIXAXIS_STEERING",
             "KI_ACTION_SWITCH_CAR_COLOUR", "KI_ACTION_SET_BOOST", "KI_ACTION_WAIT_FOR_STREAMING",
             "KI_ACTION_LOAD_PROFILE"]
RECORDS = ["CarSelectModificationScreenActionRecord", "SwitchCarCoronasOnOffActionRecord",
           "CarSelectTransitionInActionRecord", "SetSixaxisSteeringActionRecord",
           "SwitchCarColourActionRecord", "PaintShopDriveThruActionRecord"]


def main():
    rev = pre_fix_rev(sys.argv)
    module = read(MODULE, rev)
    handler = definition(module, "void RaceCarEntityModule::HandleGameActions(")
    pieces = []
    for name in CONSTANTS:
        constant = re.search(r"^\s*const (?:f32|u32|s32) " + name + r"\s*=[^;]+;", module, re.M)
        if constant:
            pieces.append(constant.group(0).strip())
    for name in RECORDS:
        one_line = re.search(r"^\s*struct " + name + r" \{[^\n]*\};", module, re.M)
        if one_line:
            pieces.append(one_line.group(0).strip())
            continue
        try:
            pieces.append(definition(module, "    struct " + name + "\n") + ";")
        except ValueError:
            pass
    # G68-D11 170: the member record and HandleSetBoost itself (0x822A4648).
    try:
        pieces.append(definition(module, "struct RaceCarEntityModule::SetBoostActionRecord\n") + ";")
        pieces.append(definition(module, "void RaceCarEntityModule::HandleSetBoost("))
    except ValueError:
        print("MISSING RaceCarEntityModule::HandleSetBoost -- replayed as an empty body")
        pieces.append("struct RaceCarEntityModule::SetBoostActionRecord {};")
        pieces.append("void RaceCarEntityModule::HandleSetBoost(const SetBoostActionRecord*, OutputFixture*) {}")
    arms = []
    for label in LABELS:
        arm = switch_arm(handler, label)
        print(("found   " if arm else "MISSING ") + "arm " + label.split("::")[-1])
        if arm:
            arms.append(arm)
    pieces.append("void RaceCarEntityModule::Dispatch(s32 liType, const CgsModule::Event* lpEvent, "
                  "OutputFixture* lpOutput)\n{\n    (void)lpEvent; (void)lpOutput;\n    switch (liType)\n    {\n"
                  + "\n".join(arms) + "\n    default:\n        break;\n    }\n}\n")

    # G67-D6 (structural): ResetActiveRaceCar hands AddHandlingModel the module's mbInCarModScreen
    # (lbzx +0x186CA @0x822F4D48 -> mr r7, r30 @0x822F4D70), not a pinned false.
    failures = []
    reset = code_mask(definition(module, "void RaceCarEntityModule::ResetActiveRaceCar("))
    if (not re.search(r"const bool lbResettingPhysicsState\s*=\s*mbInCarModScreen\s*;", reset)
            or not re.search(r"AddHandlingModel\([^;]*lbResettingPhysicsState", reset)):
        failures.append("G67-D6 ResetActiveRaceCar must pass mbInCarModScreen as AddHandlingModel's "
                        "physics-state-reset flag (lbzx +0x186CA @0x822F4D48)")

    rc = build_and_run(REPO / "tests" / "FxRcem3GameActions.cpp",
                       {"fxrcem3_game_actions.inc": "\n".join(pieces)}, "fxrcem3_game_actions",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    for failure in failures:
        print("FAIL:", failure)
    print(f"structural: {len(failures)} failures; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
