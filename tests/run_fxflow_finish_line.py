"""FX-FLOW (crash parity 2026-09-24, NEW-FINISHLINE): the director GameState's finish line.

  ModeManager::PrepareForMode @0x82342930 posts action 24 (E_ACTION_BROADCAST_MODE_FINISH_LINES,
  48 bytes: the finish landmark's BoxRegion + its id at +0x28, 0x82342F3C..0x82342F74).
  MainDirector::ProcessInputQueue case 24 @0x82238738 is its consumer and the ONLY retail writer of
  GameState::mFinishLineID (+0x160, `ld 0x28(r30)` / `stdx`) and mFinishLineNorthmostDir (+0x170,
  BoxRegion::ComputeDirection of the record). The PC switch had no case 24, so the id kept
  GameState::Clear's 0 and ArbStatePostEvent::Prepare's GetEventCompletionShots asserted
  "Unknown finish line" (BrnDirectorResourceManager.cpp:325) at every offline race finish.

Numeric: tests/FxFlowFinishLine.cpp compiles the extracted case-24 arm (a labelled empty stand-in
when the revision has none) against the real GameState, the real ComputeDirection
(SharedClasses/Trigger/BrnRegion.cpp) and the extracted GetEventCompletionShots, and feeds it the
console's byte-built wire record.
Wiring: the producer posts the same DWARF record type the consumer reads.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_finish_line.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, REPO, STRSTREAM_CPP

DIRECTOR_CPP = "src/GameSource/Director/BrnMainDirector.cpp"
RESOURCES_CPP = "src/GameSource/Director/BrnDirectorResourceManager.cpp"
PREPARE_CPP = "src/GameSource/GameState/ModeManager/BrnModeManager_Prepare.cpp"
REGION_CPP = REPO / "src/SharedClasses/Trigger/BrnRegion.cpp"
NUMERIC_CHECKS = 13

PROCESS_INPUT_QUEUE = "void MainDirector::ProcessInputQueue(const DirectorInputOutput* lpIO)"
COMPLETION_SHOTS = "const Attrib::Gen::shotgroup& DirectorResourceManager::GetEventCompletionShots("
PREPARE_FOR_MODE = "void ModeManager::PrepareForMode("


def case_arm(function_text, case_id):
    """The text of `case <id>:` and its brace block inside a switch (comments stripped), or None."""
    code = code_only(function_text)
    match = re.search(r"\bcase\s+%d\s*:" % case_id, code)
    if match is None:
        return None
    brace = code.find("{", match.end())
    between = code[match.end():brace]
    if brace < 0 or between.strip():
        return None   # an unbraced arm (e.g. a fall-through label) is not this arm's shape
    depth = 0
    for index in range(brace, len(code)):
        if code[index] == "{":
            depth += 1
        elif code[index] == "}":
            depth -= 1
            if depth == 0:
                return code[match.start():index + 1]
    return None


def wiring(tree):
    director = tree.read(DIRECTOR_CPP)
    prepare = tree.read(PREPARE_CPP)
    try:
        pinq = definition(director, PROCESS_INPUT_QUEUE)
    except ValueError:
        pinq = ""
    arm = re.sub(r"\s+", "", case_arm(pinq, 24) or "")
    yield ("ProcessInputQueue has a case-24 arm reading the record as "
           "BroadcastModeFinishLinesAction (0x82238738)",
           "BroadcastModeFinishLinesAction" in arm)
    yield ("the arm stores mFinishLineID from the record and mFinishLineNorthmostDir from "
           "mBoxRegion.ComputeDirection() (0x8223874C / 0x82238760)",
           ".mFinishLineID=" in arm and ".mFinishLineNorthmostDir=" in arm
           and ".mBoxRegion.ComputeDirection()" in arm)
    try:
        pfm = re.sub(r"\s+", "", code_only(definition(prepare, PREPARE_FOR_MODE)))
    except ValueError:
        pfm = ""
    yield ("PrepareForMode posts the same DWARF record (BroadcastModeFinishLinesAction) under "
           "E_ACTION_BROADCAST_MODE_FINISH_LINES (0x82342F74)",
           re.search(r"BroadcastModeFinishLinesAction(\w+);", pfm) is not None
           and re.search(r"AddEvent\(&\w+,GameStateModuleIO::E_ACTION_BROADCAST_MODE_FINISH_LINES\)", pfm)
           is not None)


def numeric(tree):
    director = tree.read(DIRECTOR_CPP)
    resources = tree.read(RESOURCES_CPP)
    try:
        completion = definition(resources, COMPLETION_SHOTS)
    except ValueError:
        print("NUMERIC: cannot build -- production body absent: GetEventCompletionShots")
        return None
    try:
        arm = case_arm(definition(director, PROCESS_INPUT_QUEUE), 24)
    except ValueError:
        arm = None
    if arm is None:
        print("NUMERIC: ProcessInputQueue has no case-24 arm in this revision (empty stand-in)")
        arm = "case 24: { /* [stand-in: no case-24 arm in this revision] */ break; }"
    inc = ("namespace BrnDirector {\n" + completion + "\n}\n"
           "void FinishLineDirector::Drain(s32 liActionType, const u8* lpacPayload)\n{\n"
           "    using namespace BrnDirector;\n"
           "    switch (liActionType)\n    {\n" + arm + "\n    default:\n        break;\n    }\n}\n")
    return compile_and_run(Path(__file__).with_name("FxFlowFinishLine.cpp"), "finish_line.inc", inc,
                           "FxFlowFinishLine", extra_sources=[STRSTREAM_CPP, REGION_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxflow_finish_line", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
