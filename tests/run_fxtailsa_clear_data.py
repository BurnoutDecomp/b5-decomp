"""FX-TAILS-A item 2 (crash parity 2026-09-24): GameStateModule::ClearData @0x8236B3A8 and its two calls.

The tree had NO body for it (hasbody); its values rode as member initialisers and two "construct insurance"
interface Clears. The console calls it from Construct @0x82380388 (`bl` @0x823807A8, right after
DeveloperChallengeManager::Construct @0x82380794) and from Prepare @0x8239E578 case 0 (`bl` @0x8239E6A8, before
the two DebugComponent::Register calls). The Prepare one-shot that Constructed mReceiverQueue "until ClearData
lands" was built on a false premise -- ClearData never touches the queue; Construct builds it inline right after
ProgressionManager::Construct (0x82380510..0x82380530) -- so it moves to that seat.

  1. WIRING -- the body exists; Construct calls it right after DeveloperChallengeManager::Construct; Prepare's
     START stage calls it before mResetPlayerDebugComponent.Register(); the receiver queue is Constructed in
     Construct right after ProgressionManager::Construct and no longer in Prepare; the two insurance Clears are
     gone from Construct.
  2. NUMERIC -- tests/FxTailsAClearData.cpp runs the extracted body on raw 0xA5 storage of the REAL
     GameStateModule layout and checks every console store by member name, plus the members it must NOT write.
     A revision without the body cannot build it: every numeric check then counts as failed.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsa_clear_data.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, STRSTREAM_CPP, Tree, body_or_empty, code_only, compile_and_run, definition, report

MODULE_CPP = "src/GameSource/GameState/BrnGameStateModule.cpp"
MODULE_H = "src/GameSource/GameState/BrnGameStateModule.h"
AICAR_CPP = REPO / "src/GameSource/World/AI/SharedIO/BrnAICarOutputInterface.cpp"
CLEAR = "void GameStateModule::ClearData()"
CONSTRUCT = "void GameStateModule::Construct()"
PREPARE = "bool GameStateModule::Prepare("
NUMERIC_CHECKS = 31


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    source = tree.read(MODULE_CPP).replace("\r\n", "\n")
    yield ("GameStateModule::ClearData has a body (X360 0x8236B3A8)", bool(body_or_empty(source, CLEAR)))
    construct = squash(body_or_empty(source, CONSTRUCT))
    dcm = construct.find("mDeveloperChallengeManager.Construct(")
    after = construct[dcm:] if dcm >= 0 else ""
    # Plain assignments may stand between the two calls: the console stores mbSendSetupPlayerCarPending
    # (mbIsFirstUpdate, `stbx r24(=1), r31, 0x32DC4` @0x823807A4) between DeveloperChallengeManager::Construct
    # and `bl ClearData` (FX-AIBUZZ 2026-09-24, conductor-approved). The match is ANCHORED at the statement
    # after the DeveloperChallengeManager call, so any call there -- free, member, or on an assignment's
    # right-hand side -- still turns this red (the old unanchored search skipped member calls).
    dcm_end = after.find(");")
    first_call = re.match(r"(?:[\w.]+=[^;(]+;)*(\w+)\(", after[dcm_end + 2:]) if dcm_end >= 0 else None
    yield ("Construct calls ClearData() right after DeveloperChallengeManager::Construct (0x82380794 -> 0x823807A8)",
           dcm >= 0 and first_call is not None and first_call.group(1) == "ClearData")
    prepare = squash(body_or_empty(source, PREPARE))
    start = prepare.find("caseE_PREPARESTAGE_START:")
    yield ("Prepare's START stage opens with ClearData(), then the debug-component Register (0x8239E6A8, 0x8239E6B4)",
           start >= 0 and prepare.startswith("ClearData();mResetPlayerDebugComponent.Register();",
                                             start + len("caseE_PREPARESTAGE_START:")))
    progression = construct.find("mProgressionManager.Construct(")
    tail = construct[progression:] if progression >= 0 else ""
    queue = tail.find(");mReceiverQueue.Construct();")
    yield ("mReceiverQueue is Constructed in Construct right after ProgressionManager::Construct (0x82380510..0x82380530)",
           progression >= 0 and queue >= 0 and tail[:queue].count(";") == 0)
    yield ("...and no longer by a one-shot in Prepare", "mReceiverQueue.Construct()" not in prepare
           and "mbReceiverQueueConstructed" not in code_only(source))
    yield ("the two 'construct insurance' interface Clears are gone from Construct (ClearData makes them)",
           "mLastActiveRaceCarInterface.Clear()" not in construct
           and "mLastGlobalRaceCarInterface.Clear()" not in construct)
    header = squash(tree.read(MODULE_H))
    yield ("the header declares ClearData (DWARF BrnGameStateModule.h:688)", "voidClearData();" in header)


def numeric(tree):
    source = tree.read(MODULE_CPP).replace("\r\n", "\n")
    try:
        body = definition(source, CLEAR)
    except ValueError:
        print("NUMERIC: cannot build -- GameStateModule::ClearData absent")
        return None
    constant = re.search(r"namespace\s*\{[^{}]*const u16 K_INVALID_VEHICLE_INDEX\s*=[^;]+;\s*\}", source)
    if constant is None:
        print("NUMERIC: cannot build -- the file-scope K_INVALID_VEHICLE_INDEX block is absent")
        return None
    shadow = {MODULE_H: tree.read(MODULE_H)}
    return compile_and_run(Path(__file__).with_name("FxTailsAClearData.cpp"), "fxtailsa_clear_data.inc",
                           constant.group(0) + "\n" + body + "\n", "FxTailsAClearData", shadow=shadow,
                           extra_sources=(AICAR_CPP, STRSTREAM_CPP))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtailsa_clear_data", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
