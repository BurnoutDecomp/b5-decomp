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
from fxrcem3_common import RCEM, REPO, build_and_run, definition, pre_fix_rev, read, switch_arm

MODULE = RCEM + "BrnRaceCarEntityModule.cpp"
LABELS = [r"BrnGameState::GameStateModuleIO::E_ACTION_START_PLAYING_MODE"]
CONSTANTS = ["KF_MIN_STUNT_RESET_SPEED"]


def main():
    rev = pre_fix_rev(sys.argv)
    module = read(MODULE, rev)
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
    rc = build_and_run(REPO / "tests" / "FxRcem3GameActions.cpp",
                       {"fxrcem3_game_actions.inc": "\n".join(pieces)}, "fxrcem3_game_actions",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
