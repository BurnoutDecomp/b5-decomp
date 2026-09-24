"""FX-RCEM4 (crash parity 2026-09-24, G68-D10): RaceCarEntityModule::HandleGameActions' online arms
11 (REMOTE_PLAYER_DISCONNECTED), 27 / 41 (FINISH_MODE_FINAL_ONLINE / QUIT_MODE_ONLINE) and 220
(ONLINE_PLAYER_REMOVED) (ARTIST 0x8230BE08), and RemoveAllNetworkCarsFromWorld (0x82306028).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_network_arms.py [--pre-fix <b5 rev>]
Extracts the arms and the helper into tests/FxRcem4NetworkArms.cpp. A missing arm is replayed as the
console's `default: break;`, a missing helper as an empty body.
"""
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, definition, optional_definition, pre_fix_rev, read, switch_arm

MODULE = RCEM + "BrnRaceCarEntityModule.cpp"
RIVALS = RCEM + "BrnRaceCarEntityModule_Rivals.cpp"
LABELS = [r"BrnGameState::GameStateModuleIO::E_ACTION_REMOTE_PLAYER_DISCONNECTED",
          r"BrnGameState::GameStateModuleIO::E_ACTION_FINISH_MODE_FINAL_ONLINE",
          r"BrnGameState::GameStateModuleIO::E_ACTION_QUIT_MODE_ONLINE",
          r"BrnGameState::GameStateModuleIO::E_ACTION_ONLINE_PLAYER_REMOVED"]


def main():
    rev = pre_fix_rev(sys.argv)
    handler = definition(read(MODULE, rev), "void RaceCarEntityModule::HandleGameActions(")
    arms = []
    for label in LABELS:
        arm = switch_arm(handler, label)
        print(("found   " if arm else "MISSING ") + "arm " + label.split("::")[-1])
        if arm:
            arms.append(arm)
    helper = optional_definition(read(RIVALS, rev), "void RaceCarEntityModule::RemoveAllNetworkCarsFromWorld(")
    print(("found   " if helper else "MISSING ") + "RemoveAllNetworkCarsFromWorld")
    if not helper:
        helper = ("void RaceCarEntityModule::RemoveAllNetworkCarsFromWorld("
                  "RaceCarEntityModuleIO::OutputBuffer_PreScene*) {}")
    dispatch = ("void RaceCarEntityModule::Dispatch(s32 liType, const CgsModule::Event* lpEvent, "
                "OutputFixture* lpOutput)\n{\n    (void)lpEvent; (void)lpOutput;\n    switch (liType)\n    {\n"
                + "\n".join(arms) + "\n    default:\n        break;\n    }\n}\n")
    rc = build_and_run(REPO / "tests" / "FxRcem4NetworkArms.cpp", {"fxrcem4_na_pieces.inc": helper + "\n\n" + dispatch},
                       "fxrcem4_na", extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
