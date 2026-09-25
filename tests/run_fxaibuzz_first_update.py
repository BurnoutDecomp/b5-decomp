"""FX-AIBUZZ item 3 (crash parity 2026-09-24): GameStateModule's start-of-game latch is armed where the console arms it.

+0x32DC4 is mbIsFirstUpdate (DWARF BrnGameStateModule.h:272), this tree's mbSendSetupPlayerCarPending. An image-wide
grep of the ARTIST export for 0x2DC4 finds exactly two functions touching it:
  GameStateModule::Construct @0x82380388   `stbx r24(=1, li @0x823803A0), r31, 0x32DC4` @0x823807A4 -- between
                                           DeveloperChallengeManager::Construct (0x82380794) and `bl ClearData`
                                           (0x823807A8)
  GameStateModule::PreWorldUpdate          tests it, runs SendSetupPlayerCarEvent + SendSetUpAllEventStartsMessage,
                                           clears it (0x823A5510..0x823A5540)
Nothing else writes it (not ClearData, not Prepare, no event handler). The PC armed it at the end of Prepare's
terminal stage on the belief that an event handler does. Structural (the store's seat is the change; its only reader
runs in E_MGS_IN_GAME, so nothing observable moves -- the live case FxAiBuzzFirstUpdateLive.ps1 shows the one-shot
leg firing exactly once, after Prepare, as before):
  1. Construct: `mbSendSetupPlayerCarPending = true;` right after DeveloperChallengeManager::Construct, then ClearData.
  2. Prepare no longer arms it.
  3. The tree's only writers are that store and the consuming leg's clear; its only reader is that leg.
  4. That leg's one call site is inside BrnGameModule's E_MGS_IN_GAME block.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxaibuzz_first_update.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, body_or_empty, code_only, report

MODULE_CPP = "src/GameSource/GameState/BrnGameStateModule.cpp"
GAME_CPP = "src/GameSource/Game/BrnGameModule.cpp"
CONSTRUCT = "void GameStateModule::Construct()"
PREPARE = "bool GameStateModule::Prepare("
LEG = "void GameStateModule::PreWorldUpdateSetupPlayerCarBringUp()"
LATCH = "mbSendSetupPlayerCarPending"


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def all_sources(tree):
    """Every .cpp/.h under src/ that mentions the latch (git grep: the working tree, or the revision), code only."""
    import subprocess
    command = ["git", "-C", str(REPO), "grep", "-l", LATCH] + ([tree.rev] if tree.rev else []) + ["--", "src"]
    listing = subprocess.run(command, capture_output=True, text=True, encoding="utf-8").stdout.split()
    for entry in listing:
        path = entry.split(":", 1)[1] if tree.rev else entry   # `<rev>:<path>` with a revision
        if path.endswith((".cpp", ".h")):
            yield path, code_only(tree.read(path).replace("\r\n", "\n"))


def wiring(tree):
    source = tree.read(MODULE_CPP).replace("\r\n", "\n")
    construct = squash(body_or_empty(source, CONSTRUCT))
    dcm = construct.find("mDeveloperChallengeManager.Construct(")
    after = construct[dcm:] if dcm >= 0 else ""
    dcm_end = after.find(");")
    rest = after[dcm_end + 2:] if dcm_end >= 0 else ""
    yield ("Construct: mbSendSetupPlayerCarPending = true right after DeveloperChallengeManager::Construct, then "
           "ClearData() (0x82380794 -> stbx 0x823807A4 -> bl 0x823807A8)",
           rest.startswith(LATCH + "=true;ClearData();"))
    prepare = squash(body_or_empty(source, PREPARE))
    yield ("Prepare no longer arms the latch (the console's Prepare never touches +0x32DC4)",
           bool(prepare) and LATCH not in prepare)

    # Every mention in code (comments stripped) across src/: the header's member declaration (its `= false`
    # initialiser), and in the .cpp files the writes (`=` not `==`) and the reads.
    writes, reads, declarations = [], [], []
    for path, text in all_sources(tree):
        for match in re.finditer(LATCH + r"\s*(=(?!=))?", text):
            if path.endswith(".h"):
                declarations.append(path)
            elif match.group(1):
                writes.append(text[match.end():match.end() + 8].strip().split(";")[0])
            else:
                reads.append(path)
    leg = squash(body_or_empty(source, LEG))
    yield ("the tree's only writers are Construct's `= true` and the consuming leg's `= false`",
           sorted(writes) == ["false", "true"] and f"{LATCH}=false;" in leg
           and declarations == ["src/GameSource/GameState/BrnGameStateModule.h"])
    yield ("its only readers are the two tests in PreWorldUpdateSetupPlayerCarBringUp",
           leg.count(LATCH) == 3 and len(reads) == 2 and all(path == MODULE_CPP for path in reads))

    game = code_only(tree.read(GAME_CPP).replace("\r\n", "\n"))
    call = game.find("mGameStateModule.PreWorldUpdateSetupPlayerCarBringUp();")
    gate = game.rfind("if (leState == BrnGameMainFlowController::E_MGS_IN_GAME)", 0, call) if call >= 0 else -1
    depth = 0
    for char in game[gate:call] if gate >= 0 else "":
        depth += (char == "{") - (char == "}")
    yield ("the consuming leg's one call site sits inside BrnGameModule's E_MGS_IN_GAME block",
           call >= 0 and game.count("PreWorldUpdateSetupPlayerCarBringUp();") == 1 and gate >= 0 and depth > 0)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    return report("run_fxaibuzz_first_update", list(wiring(Tree(args.rev))), (0, 0), 0)


if __name__ == "__main__":
    sys.exit(main())
