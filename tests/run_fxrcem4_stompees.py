"""FX-RCEM4 (crash-parity 2026-09-24): CHAIN-STOMPEES part (c) -- RaceCarEntityModule::
ProcessLeapedAndStompedCars @0x822BD5B8 and its PostSceneUpdate slot (bl @0x822FE4B8).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_stompees.py [--pre-fix <b5 rev>]
Extracts the production body from BrnRaceCarEntityModule_CrashExit.cpp (an empty stub when the source
has none) into tests/FxRcem4Stompees.cpp. Structural: PostSceneUpdate calls it with (lpInput, lpOutput)
right after ProcessRaceCarCrashCompleteEvents and before SendResetOnTrackRequests.
"""
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, code_mask, definition, optional_definition, pre_fix_rev, read

CRASHEXIT = RCEM + "BrnRaceCarEntityModule_CrashExit.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    source = read(CRASHEXIT, rev)
    body = optional_definition(source, "void RaceCarEntityModule::ProcessLeapedAndStompedCars(")
    print(("found   " if body else "MISSING ") + "ProcessLeapedAndStompedCars")
    if not body:
        body = ("void RaceCarEntityModule::ProcessLeapedAndStompedCars(const RaceCarEntityModuleIO::"
                "InputBuffer_PostScene*, RaceCarEntityModuleIO::OutputBuffer_PostScene*) {}")

    failures = []
    post = code_mask(definition(source, "void RaceCarEntityModule::PostSceneUpdate("))
    order = [post.find("ProcessRaceCarCrashCompleteEvents("),
             post.find("ProcessLeapedAndStompedCars( lpInput, lpOutput )"),
             post.find("SendResetOnTrackRequests(")]
    if -1 in order or not (order[0] < order[1] < order[2]):
        failures.append("PostSceneUpdate must call ProcessLeapedAndStompedCars( lpInput, lpOutput ) right after "
                        "ProcessRaceCarCrashCompleteEvents (bl @0x822FE4B8), before SendResetOnTrackRequests")

    rc = build_and_run(REPO / "tests" / "FxRcem4Stompees.cpp", {"fxrcem4_stompees.inc": body}, "fxrcem4_stompees",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    for failure in failures:
        print("FAIL:", failure)
    print(f"structural: {len(failures)} failures; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
