"""FX-GS2 (crash parity 2026-09-23, G10-D11 part 2): BrnGameModule::TranslateGuiInterfaceToGuiEvents,
the consumer of the eight GameStateToGuiInterface queues, and its call site.

  0x823E1D90: read all eight queue lengths up front, then post one GUI event per record, loop by loop:
  new dirty trick 177/12, triggered 179/12, ending 181/16 (+ the survived byte), overtake 371/8
  {slot, position byte}, finish 372/8 {slot, finish type}, took lead 484/16, took last 485/16 and
  on tail 486/16 {car id, slot}. BridgeGameStateToGui @0x823EE880 calls it at 0x823EF2BC, right
  after TranslateTakedownsToGuiEvents, on the read-locked game-state output.

Numeric: tests/FxGs2GuiIface.cpp compiles the extracted production bodies (the translate and its two
TU-local wire records from BrnGameModule.cpp; PushGuiEvent from GameBridgeGameStateToX.h; the
interface's Construct, publishers and const accessors from BrnGameStateToGuiIOInterfaces.cpp)
against a recording GUI input buffer.
Wiring: the call in GameMain's GUI leg -- after the takedown translator, inside the write lock, and
before the per-sub-step retire of the interface.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs2_gui_iface.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

GAME_CPP = "src/GameSource/Game/BrnGameModule.cpp"
BRIDGE_H = "src/GameSource/Game/GameBridgeGameStateToX.h"
GUI_CPP = "src/GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.cpp"
NUMERIC_CHECKS = 21

GUI_REQUIRED = [
    "void GameStateToGuiInterface::Construct()",
    "void GameStateToGuiInterface::AddFinishedRaceEvent(",
    "void GameStateToGuiInterface::AddDirtyTrickEnding(",
    "void GameStateToGuiInterface::AddDirtyTrickTriggered(",
    "void GameStateToGuiInterface::AddNewDirtyTrick(",
    "void GameStateToGuiInterface::AddOnTailEvent(",
]
# The accessors are only called by the translate; a revision without the translate needs none.
GUI_ACCESSORS = [
    "const GameStateToGuiInterface::NewDirtyTrickQueue* GameStateToGuiInterface::GetNewDirtyTrickQueue()",
    "const GameStateToGuiInterface::DirtyTrickTriggeredQueue* GameStateToGuiInterface::GetDirtyTrickTriggeredQueue()",
    "const GameStateToGuiInterface::DirtyTrickEndingQueue* GameStateToGuiInterface::GetDirtyTrickEndingQueue()",
    "const GameStateToGuiInterface::OvertakeEventQueue* GameStateToGuiInterface::GetOvertakeEventQueue()",
    "const GameStateToGuiInterface::FinishedRaceEventQueue* GameStateToGuiInterface::GetFinishedRaceEventQueue()",
    "const GameStateToGuiInterface::TookLeadEventQueue* GameStateToGuiInterface::GetTookLeadEventQueue()",
    "const GameStateToGuiInterface::TookLastEventQueue* GameStateToGuiInterface::GetTookLastEventQueue()",
    "const GameStateToGuiInterface::OnTailEventQueue* GameStateToGuiInterface::GetOnTailEventQueue()",
]
WIRE_RECORDS = ["struct GuiOvertakeEventWire371", "struct GuiFinishRaceEventWire372"]
TRANSLATE = "void TranslateGuiInterfaceToGuiEvents("
TRANSLATE_STAND_IN = ("void TranslateGuiInterfaceToGuiEvents(CgsGui::CgsGuiModuleIO::InputBuffer*,"
                      " const BrnGameState::GameStateModuleIO::GameStateToGuiInterface*) {}")


def wiring(tree):
    game = tree.read(GAME_CPP)
    try:
        # code only, and every whitespace run removed: the checks match tokens, not layout
        main = re.sub(r"\s+", "", code_only(definition(game, "bool BrnGameModule::GameMain()")))
    except ValueError:
        main = ""
    takedowns = main.find("TranslateTakedownsToGuiEvents(")
    call = main.find("TranslateGuiInterfaceToGuiEvents(mpGuiInputBuffer,"
                     "lpcGameStateOutput->GetGameStateToGuiInterface());")
    unlock = main.find("mpGuiInputBuffer->UnlockForWrite();", max(takedowns, 0))
    retire = main.find("lpGameStateOutput->GetGameStateToGuiInterface()->Construct();")
    yield ("GameMain's GUI leg calls TranslateGuiInterfaceToGuiEvents(guiInput, "
           "output->GetGameStateToGuiInterface()) right after TranslateTakedownsToGuiEvents, inside the "
           "GUI input write lock (0x823EF278 -> 0x823EF2BC)",
           0 <= takedowns < call < unlock)
    yield ("the interface is translated BEFORE the per-sub-step retire (its Construct) empties it",
           0 <= call < retire)


def numeric(tree):
    game = tree.read(GAME_CPP)
    bridge = tree.read(BRIDGE_H)
    gui = tree.read(GUI_CPP)
    stood_in = []
    try:
        push = "template <class GuiEventT>\n" + definition(bridge, "static void PushGuiEvent(")
    except ValueError:
        print("NUMERIC: cannot build -- production body absent: PushGuiEvent")
        return None
    gui_parts = []
    for signature in GUI_REQUIRED:
        try:
            gui_parts.append(definition(gui, signature))
        except ValueError:
            print("NUMERIC: cannot build -- production body absent: " + signature)
            return None
    try:
        translate = definition(game, TRANSLATE)
        records = [definition(game, signature) + ";" for signature in WIRE_RECORDS]
        for signature in GUI_ACCESSORS:
            gui_parts.append(definition(gui, signature))
    except ValueError:
        translate = "// [stand-in: body absent in this revision]\n" + TRANSLATE_STAND_IN
        records = []
        stood_in.append("TranslateGuiInterfaceToGuiEvents")
    if stood_in:
        print("NUMERIC: bodies absent in this revision (empty stand-ins): " + ", ".join(stood_in))
    inc = ("namespace BrnGameState { namespace GameStateModuleIO {\n" + "\n".join(gui_parts) + "\n} }\n"
           + "namespace BrnGame {\n" + push + "\nnamespace {\n" + "\n".join(records) + "\n" + translate
           + "\n}\n}\n")
    return compile_and_run(Path(__file__).with_name("FxGs2GuiIface.cpp"), "gui_iface_methods.inc", inc,
                           "FxGs2GuiIface", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgs2_gui_iface", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
