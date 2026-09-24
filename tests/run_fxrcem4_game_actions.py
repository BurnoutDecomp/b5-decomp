"""FX-RCEM4 (crash-parity 2026-09-24): replay G68-D11 arms 4 and 74 of
RaceCarEntityModule::HandleGameActions (ARTIST 0x8230BE08), HandleSetPlayerOpponentsAction
(0x822E96D8) and UpdateStreaming (0x822FEFE0, the junkyard-exit audio-wait leg) against fixtures
(tests/FxRcem4GameActions.cpp).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_game_actions.py [--pre-fix <b5 rev>]
An arm the source does not have is replayed as the console's `default: break;` and a missing
handler as an empty body, so the pre-fix source reports per-check failures instead of failing to
extract. Construct's +0x186D1 store (0x822FDBC0) is checked structurally.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, code_mask, definition, pre_fix_rev, read, switch_arm

MODULE = RCEM + "BrnRaceCarEntityModule.cpp"
LABELS = [r"KI_ACTION_SET_PLAYER_OPPONENTS", r"KI_ACTION_CAR_SELECT_WAITING_FOR_AUDIO"]
CONSTANTS = ["KI_ACTION_SET_PLAYER_OPPONENTS", "KI_ACTION_CAR_SELECT_WAITING_FOR_AUDIO"]
RECORDS = ["CarSelectWaitingForAudioActionRecord"]


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
    # G68-D11 4: the member record and HandleSetPlayerOpponentsAction itself (0x822E96D8).
    try:
        pieces.append(definition(module, "struct RaceCarEntityModule::SetPlayerOpponentsActionRecord\n") + ";")
        pieces.append(definition(module, "void RaceCarEntityModule::HandleSetPlayerOpponentsAction("))
        print("found   HandleSetPlayerOpponentsAction")
    except ValueError:
        print("MISSING RaceCarEntityModule::HandleSetPlayerOpponentsAction -- replayed as an empty body")
        pieces.append("struct RaceCarEntityModule::SetPlayerOpponentsActionRecord { Array<CgsID, 7u> maOpponents; };")
        pieces.append("void RaceCarEntityModule::HandleSetPlayerOpponentsAction(const SetPlayerOpponentsActionRecord*) {}")
    # UpdateStreaming whole (the +0x186D1 leg sits between the car-select sweep and the edge).
    pieces.append(definition(module, "void RaceCarEntityModule::UpdateStreaming("))
    arms = []
    for label in LABELS:
        arm = switch_arm(handler, label)
        print(("found   " if arm else "MISSING ") + "arm " + label)
        if arm:
            arms.append(arm)
    pieces.append("void RaceCarEntityModule::Dispatch(s32 liType, const CgsModule::Event* lpEvent, "
                  "RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput)\n{\n    (void)lpEvent; (void)lpOutput;\n"
                  "    switch (liType)\n    {\n" + "\n".join(arms) + "\n    default:\n        break;\n    }\n}\n")

    # Construct 0x822FDBC0: `stbx r31 (0), r30 (this), r6 (0x186D1)` -- the latch starts clear.
    failures = []
    construct = code_mask(definition(module, "void RaceCarEntityModule::Construct()"))
    if not re.search(r"\bmbHACK_ExitingCarSelectWaitForAudio\s*=\s*false\s*;", construct):
        failures.append("Construct must clear mbHACK_ExitingCarSelectWaitForAudio (+0x186D1, stbx 0 @0x822FDBC0)")
    header = read(RCEM + "BrnRaceCarEntityModule.h", rev)
    if not re.search(r"^\s*bool\s+mbHACK_ExitingCarSelectWaitForAudio\s*;", header, re.M):
        failures.append("the header must declare DWARF :448 bool mbHACK_ExitingCarSelectWaitForAudio")

    rc = build_and_run(REPO / "tests" / "FxRcem4GameActions.cpp",
                       {"fxrcem4_game_actions.inc": "\n".join(pieces)}, "fxrcem4_game_actions",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    for failure in failures:
        print("FAIL:", failure)
    print(f"structural: {len(failures)} failures; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
