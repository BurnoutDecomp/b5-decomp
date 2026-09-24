"""FX-BRIDGES (crash parity 2026-09-24) CC-8: BridgeGameStateToDirector @0x823CD170, the player-taken-down leg.

Console live arm 0x823CD330..0x823CD3C8: read the player's race-car index from the scoring output interface, walk the
game state's takedown event queue and, for every event whose victim is the player, raise the director input's
mbPlayerTakenDown (+0x7AC0) and record the aggressor as mePlayerKillerCarIndex (+0x7AAC) -- the inlined
InputBuffer::SetPlayerKiller (DWARF :226) -- with the "lePlayerKillerRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID"
tripwire (GameBridgeGameStateToX.cpp:228); THEN Append the game-action queue. The PC body only Appended, so
MainDirector::ProcessInputQueue never saw a taken-down frame and ArbStateCrashing never allocated its taken-down
camera: the player's crash after a rival takedown never showed the killer.

Numeric: tests/FxBridgesTakedownCam.cpp compiles the PRODUCTION body (by its definition signature) against the real
InputBuffer / TakedownEvent / EventQueue. The old body compiles too -- it then fails the leg checks.
Wiring: the walk precedes the Append in the source; InputBuffer declares the inline SetPlayerKiller writing both
members; _AssertLayout pins the pair.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxbridges_takedown_cam.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/Game/BrnGameModule.cpp"
INPUT_H = "src/GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"
INPUT_CPP = "src/GameSource/Director/DirectorModule/BrnDirectorModuleIOInputBuffer.cpp"
SIGNATURE = "void BrnGameModule::BridgeGameStateToDirector("
NUMERIC_CHECKS = 10


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def body_text(tree):
    """The definition's body (from its first brace), or None when the revision has no definition."""
    try:
        text = definition(tree.read(MODULE_CPP), SIGNATURE)
    except ValueError:
        return None
    return text[text.index("{"):] + "\n"


def wiring(tree):
    body = squash(body_text(tree) or "")
    setter = body.find("lpDirectorInput->SetPlayerKiller(")
    append = body.find("lpDirectorInput->GetGameActionQueue()->Append(")
    yield ("the taken-down walk publishes through SetPlayerKiller BEFORE the Append (0x823CD3B8 < 0x823CD3FC)",
           0 <= setter < append)
    yield ("the walk carries the :228 tripwire text verbatim",
           '"lePlayerKillerRaceCarIndex!=E_ACTIVE_RACE_CAR_INDEX_INVALID"' in body)
    header = squash(tree.read(INPUT_H))
    yield ("InputBuffer declares SetPlayerKiller (DWARF :226) raising the flag and recording the killer",
           "voidSetPlayerKiller(EActiveRaceCarIndexlePlayerKillerCarIndex){mbPlayerTakenDown=true;"
           "mePlayerKillerCarIndex=lePlayerKillerCarIndex;}" in header)
    layout = squash(tree.read(INPUT_CPP))
    yield ("_AssertLayout pins the pair at +0x7AAC / +0x7AC0",
           "offsetof(InputBuffer,mePlayerKillerCarIndex)==0x7AAC+" in layout
           and "offsetof(InputBuffer,mbPlayerTakenDown)==0x7AC0+" in layout)


def numeric(tree):
    text = body_text(tree)
    if text is None:
        print("NUMERIC: cannot build -- BridgeGameStateToDirector has no definition in this revision")
        return None
    shadow = {INPUT_H: tree.read(INPUT_H)} if tree.rev else None
    return compile_and_run(Path(__file__).with_name("FxBridgesTakedownCam.cpp"),
                           "fxbridges_takedown_cam_body.inc", text, "FxBridgesTakedownCam",
                           shadow=shadow, extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxbridges_takedown_cam", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
