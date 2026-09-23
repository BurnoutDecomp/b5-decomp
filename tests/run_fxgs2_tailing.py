"""FX-GS2 (crash parity 2026-09-23, G10-D11): GameStateModule::CheckForTailingRivals and its wiring.

  0x82375F90: in an offline race (or any online mode) with the controller in the active-mode state,
  the player's car in use and |velocity| > 50: for every other slot, a clock runs while that rival is
  behind the player by at most 20 m, in use and above 50 mph; otherwise `fsel` resets it (a negative or
  NaN clock is kept). At !(clock < 3.0) the on-tail record {GetRivalId(slot), slot} is added to the
  GameStateToGuiInterface (+0x160) and the clock restarts. Slot 7 of the f32[7] clock is the next
  word, muNetworkGameRandomSeed (the console's r27 steps across the player's slot too).

Numeric: tests/FxGs2Tailing.cpp compiles the extracted production bodies (CheckForTailingRivals,
Get/SetRivalTailingTime from GameStateModule_gUI_00.cpp; AddOnTailEvent + Construct from
BrnGameStateToGuiIOInterfaces.cpp) against a GameStateModule fixture exposing the members they name.
Wiring: the call site in PreWorldUpdateTrainingBringUp and the per-sub-step retire in BrnGameModule.cpp.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs2_tailing.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report, STRSTREAM_CPP

GUI_UI_CPP = "src/GameSource/GameState/GameStateModule_gUI_00.cpp"
GSM_CPP = "src/GameSource/GameState/BrnGameStateModule.cpp"
GAME_CPP = "src/GameSource/Game/BrnGameModule.cpp"
GUI_CPP = "src/GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.cpp"
NUMERIC_CHECKS = 18

OPTIONAL = [
    ("f32 GameStateModule::GetRivalTailingTime(",
     "f32 GameStateModule::GetRivalTailingTime(s32) const { return 0.0f; }"),
    ("void GameStateModule::SetRivalTailingTime(",
     "void GameStateModule::SetRivalTailingTime(s32, f32) {}"),
    ("void GameStateModule::CheckForTailingRivals(",
     "void GameStateModule::CheckForTailingRivals(GameStateModuleIO::OutputBuffer*,"
     " const BrnPhysics::Vehicle::VehicleOutputInterface*, f32) {}"),
]
GUI_REQUIRED = ["void GameStateToGuiInterface::Construct()"]
GUI_OPTIONAL = [
    ("void GameStateToGuiInterface::AddOnTailEvent(",
     "void GameStateToGuiInterface::AddOnTailEvent(CgsID, ::EActiveRaceCarIndex) {}"),
]


def wiring(tree):
    training = body_or_empty(tree.read(GSM_CPP), "void GameStateModule::PreWorldUpdateTrainingBringUp(")
    call = training.find("CheckForTailingRivals(mpOutputBuffer, &mpTakedownCache->mVehicleOutputInterface, lfGameTimestep)")
    tips = training.find("ShouldAllowTimedTutorialTips()")
    yield ("PreWorldUpdate calls CheckForTailingRivals(out, the post-world vehicle output, dt) right before "
           "ShouldAllowTimedTutorialTips (0x823A57A0 -> 0x823A57A8)", 0 <= call < tips)
    game = tree.read(GAME_CPP)
    retire = game.find("lpGameStateOutput->GetGameStateToGuiInterface()->Construct();")
    actions = game.find("lpGameStateOutput->GetGameActionQueue()->Clear();")
    yield ("the GUI interface is retired every sub-step with the action queue (the console rebuilds the "
           "OutputBuffer, and with it GameStateToGuiInterface::Construct, every frame)",
           actions >= 0 and retire > actions and retire - actions < 4000)


def numeric(tree):
    source = tree.read(GUI_UI_CPP)
    gui = tree.read(GUI_CPP)
    parts, gui_parts, stood_in = [], [], []
    for signature, stand_in in OPTIONAL:
        try:
            parts.append(definition(source, signature))
        except ValueError:
            parts.append("// [stand-in: body absent in this revision]\n" + stand_in)
            stood_in.append(signature.split("::", 1)[1].strip())
    for signature in GUI_REQUIRED:
        try:
            gui_parts.append(definition(gui, signature))
        except ValueError:
            print("NUMERIC: cannot build -- production body absent: " + signature)
            return None
    for signature, stand_in in GUI_OPTIONAL:
        try:
            gui_parts.append(definition(gui, signature))
        except ValueError:
            gui_parts.append("// [stand-in: body absent in this revision]\n" + stand_in)
            stood_in.append(signature.split("::", 1)[1].strip())
    if stood_in:
        print("NUMERIC: bodies absent in this revision (empty stand-ins): " + ", ".join(stood_in))
    inc = ("namespace BrnGameState {\n" + "\n".join(parts) + "\n}\n"
           + "namespace BrnGameState { namespace GameStateModuleIO {\n" + "\n".join(gui_parts) + "\n} }\n")
    return compile_and_run(Path(__file__).with_name("FxGs2Tailing.cpp"), "tailing_methods.inc", inc,
                           "FxGs2Tailing", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgs2_tailing", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
