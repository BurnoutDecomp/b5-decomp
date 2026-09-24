"""FX-DIRECTOR (crash parity 2026-09-24): the player's end-of-event pair, director input @0x7AD5 / @0x7AD6.

  BrnGameModule::BridgeGameStateToDirector @0x823CD170 publishes two bytes out of the game-state module's
  ScoringOutputInterface (live arm 0x823CD4A0..0x823CD510):
      @0x7AD5 = mabPlayerEliminated[mePlayerRaceCarIndex]
      @0x7AD6 = mfModeTimeRemaining > 0.0f (flt_82001CC0) ? 0 : mbTimerActive
  and MainDirector::ProcessInputQueue's prologue copies them into GameState +0x1D0 / +0x1D1
  (0x82237430..0x82237440), where ArbStateRoaming::ProcessPossibleFX plays "Damage_Crit" / "Wrecked" on them
  in the online modes 12 / 14 / 17. The PC bridge published neither (the director input carried an opaque
  u8[2] there) and the copies were gated, so both GameState bytes stayed 0 for ever.

Numeric: tests/FxDirectorEndFlags.cpp compiles the revision's end-of-event block of BridgeGameStateToDirector
against the REAL ScoringOutputInterface and a stand-in director input, and drives the player slot, the
eliminated array and the mode clock (incl. -0.0 / denormal / inf / NaN). A revision without the block compiles
an empty body, so it FAILS on behaviour. Wiring: the block sits after the game-action Append (console order),
and the director side (named input members, their pins, the un-gated copies) is present. The copies' numbers
are tests/run_fxdirector_input_queue.py's section 21.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector_end_flags.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

GAME_MODULE_CPP = "src/GameSource/Game/BrnGameModule.cpp"
DIRECTOR_IO_H = "src/GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"
INPUT_BUFFER_CPP = "src/GameSource/Director/DirectorModule/BrnDirectorModuleIOInputBuffer.cpp"
DIRECTOR_CPP = "src/GameSource/Director/BrnMainDirector.cpp"
ROAMING_CPP = "src/GameSource/Director/Arbitrator/States/BrnArbStateRoaming.cpp"
NUMERIC_CHECKS = 18

BRIDGE = "void BrnGameModule::BridgeGameStateToDirector("
BLOCK_MARKER = "// ---- the player's end-of-event pair"
APPEND = "lpDirectorInput->GetGameActionQueue()->Append(*lpGameStateOutput->GetGameActionQueue());"
PROCESS_INPUT_QUEUE = "void MainDirector::ProcessInputQueue(const DirectorInputOutput* lpIO)"
POSSIBLE_FX = "void ArbStateRoaming::ProcessPossibleFX(ArbStateSharedInfo& lrSharedInfo)"


def flat(text):
    return re.sub(r"\s+", "", code_only(text))


def bridge_block(tree):
    """(bridge body, the end-of-event block text or None)."""
    try:
        bridge = definition(tree.read(GAME_MODULE_CPP), BRIDGE)
    except ValueError:
        return "", None
    start = bridge.find(BLOCK_MARKER)
    if start < 0:
        return bridge, None
    brace = bridge.find("{", start)
    return bridge, definition(bridge[brace:], "{")


def wiring(tree):
    bridge, block = bridge_block(tree)
    bridge_flat = flat(bridge)
    append = bridge_flat.find(flat(APPEND))
    block_at = bridge_flat.find("lpDirectorInput->SetPlayerEliminated(")
    yield ("BridgeGameStateToDirector publishes @0x7AD5 then @0x7AD6 after the game-action Append "
           "(console order: 0x823CD3FC Append ... 0x823CD4DC / 0x823CD510)",
           block is not None and 0 <= append < block_at < bridge_flat.find("lpDirectorInput->SetModeTimeExpired("))
    io = flat(tree.read(DIRECTOR_IO_H))
    buffer_cpp = flat(tree.read(INPUT_BUFFER_CPP))
    yield ("the director input names the pair (mbPlayerEliminated @0x7AD5 / mbModeTimeExpired @0x7AD6), pins both "
           "offsets and seeds both to 0 in Construct (0x82239514 / 0x82239518)",
           "boolmbPlayerEliminated;boolmbModeTimeExpired;" in io
           and 'offsetof(InputBuffer,mbPlayerEliminated)==0x7AD5+' in buffer_cpp
           and 'offsetof(InputBuffer,mbModeTimeExpired)==0x7AD6+' in buffer_cpp
           and "mbPlayerEliminated=false;mbModeTimeExpired=false;" in buffer_cpp)
    try:
        pinq = flat(definition(tree.read(DIRECTOR_CPP), PROCESS_INPUT_QUEUE))
    except ValueError:
        pinq = ""
    yield ("ProcessInputQueue copies @0x7AD6 -> +0x1D1 then @0x7AD5 -> +0x1D0 before the drain (0x82237430..0x82237440)",
           0 <= pinq.find("maGameState.mbModeTimeExpired=lpInput->GetModeTimeExpired();")
           < pinq.find("maGameState.mbPlayerEliminated=lpInput->GetPlayerEliminated();")
           < pinq.find("GetFirstEvent("))
    try:
        fx = flat(definition(tree.read(ROAMING_CPP), POSSIBLE_FX))
    except ValueError:
        fx = ""
    yield ("ArbStateRoaming::ProcessPossibleFX's online arm reads +0x1D0 then +0x1D1 by the new names "
           "(0x82234C8C / 0x82234CB4)",
           0 <= fx.find("if(lrGameState.mbPlayerEliminated)") < fx.find("if(lrGameState.mbModeTimeExpired)"))


def numeric(tree):
    _, block = bridge_block(tree)
    if block is None:
        print("NUMERIC: this revision's BridgeGameStateToDirector has no end-of-event block -- empty body")
        block = "{ /* [this revision publishes neither @0x7AD5 nor @0x7AD6] */ }"
    return compile_and_run(Path(__file__).with_name("FxDirectorEndFlags.cpp"), "bridge_end_flags.inc", block + "\n",
                           "FxDirectorEndFlags")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector_end_flags", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
