"""FX-GS2 (crash parity 2026-09-23, G10-D11 part 2): BrnGameModule::TranslateGuiInterfaceToGuiEvents,
the consumer of the eight GameStateToGuiInterface queues, and its call site.

  0x823E1D90: read all eight queue lengths up front, then post one GUI event per record, loop by loop:
  new dirty trick 177/12, triggered 179/12, ending 181/16 (+ the survived byte), overtake 371/8
  {slot, position byte}, finish 372/8 {slot, finish type}, took lead 484/16, took last 485/16 and
  on tail 486/16 {car id, slot}. BridgeGameStateToGui @0x823EE880 calls it at 0x823EF2BC, right
  after TranslateTakedownsToGuiEvents, on the read-locked game-state output.

Numeric: tests/FxGs2GuiIface.cpp compiles the extracted production bodies (the qualified member from GameBridgeGameStateToX.cpp, or the historical free function
and its two wire records from BrnGameModule.cpp; PushGuiEvent from GameBridgeGameStateToX.h; the
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
BRIDGE_CPP = "src/GameSource/Game/GameBridgeGameStateToX.cpp"
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
    bridge_cpp = tree.read(BRIDGE_CPP)
    try:
        main = re.sub(r"\s+", "", code_only(definition(game, "bool BrnGameModule::GameMain()")))
    except ValueError:
        main = ""
    try:
        wrapper = re.sub(r"\s+", "", code_only(definition(
            bridge_cpp, "void BrnGameModule::BridgeGameStateToGui(")))
    except ValueError:
        wrapper = ""
    retire = main.find("lpGameStateOutput->GetGameStateToGuiInterface()->Construct();")
    if wrapper:
        call = main.find("BridgeGameStateToGui(mpGuiInputBuffer,lpcGameStateOutput);")
        lock = main.rfind("mpGuiInputBuffer->LockForWrite();", 0, call)
        unlock = main.find("mpGuiInputBuffer->UnlockForWrite();", max(call, 0))
        takedowns = wrapper.find("TranslateTakedownsToGuiEvents(")
        interface = wrapper.find("TranslateGuiInterfaceToGuiEvents(lpGuiInput,lpGameStateOutput->GetGameStateToGuiInterface());")
        time = wrapper.find("PushGuiEvent(lTimeInfo,lpGuiInput);")
        ordered = 0 <= lock < call < unlock and 0 <= takedowns < interface < time
    else:
        # Preserve the historical baseline's original free-function seat literally.
        takedowns = main.find("TranslateTakedownsToGuiEvents(")
        call = main.find("TranslateGuiInterfaceToGuiEvents(mpGuiInputBuffer,lpcGameStateOutput->GetGameStateToGuiInterface());")
        unlock = main.find("mpGuiInputBuffer->UnlockForWrite();", max(takedowns, 0))
        ordered = 0 <= takedowns < call < unlock
    yield ("GUI bridge calls the interface translator after takedowns, within the GUI input write lock", ordered)
    yield ("the interface is translated before per-sub-step retirement", 0 <= call < retire)


def numeric(tree):
    game = tree.read(GAME_CPP)
    bridge_cpp = tree.read(BRIDGE_CPP)
    bridge = tree.read(BRIDGE_H)
    gui = tree.read(GUI_CPP)
    try:
        push = "template <class GuiEventT>\n" + definition(bridge, "static void PushGuiEvent(")
    except ValueError:
        print("NUMERIC: cannot build -- production body absent: PushGuiEvent")
        return None
    gui_parts = []
    for signature in GUI_REQUIRED + GUI_ACCESSORS:
        try:
            gui_parts.append(definition(gui, signature))
        except ValueError:
            print("NUMERIC: cannot build -- production body absent: " + signature)
            return None
    try:
        translate = definition(bridge_cpp, "void BrnGameModule::TranslateGuiInterfaceToGuiEvents(")
        # Explicit owner boundary double: production uses this only for AddGuiEvent's
        # embedded GUI module; the recovered queue pusher does not inspect it.
        owner = ("struct BrnGameModule { void TranslateGuiInterfaceToGuiEvents("
                 "CgsGui::CgsGuiModuleIO::InputBuffer*, const BrnGameState::GameStateModuleIO::GameStateToGuiInterface*); };\n")
        implementation = owner + translate
        invocation = "BrnGameModule().TranslateGuiInterfaceToGuiEvents(lpInput, lpInterface);"
    except ValueError:
        try:
            translate = definition(game, TRANSLATE)
            records = [definition(game, signature) + ";" for signature in WIRE_RECORDS]
            implementation = "namespace {\n" + "\n".join(records) + "\n" + translate + "\n}\n"
            invocation = "TranslateGuiInterfaceToGuiEvents(lpInput, lpInterface);"
        except ValueError:
            print("NUMERIC: body absent in this revision: TranslateGuiInterfaceToGuiEvents")
            implementation = "// [stand-in: body absent in this revision]\n" + TRANSLATE_STAND_IN
            invocation = "TranslateGuiInterfaceToGuiEvents(lpInput, lpInterface);"
    # This fixture adapter selects the actual production owner without changing its body.
    adapter = ("static void InvokeTranslator(CgsGui::CgsGuiModuleIO::InputBuffer* lpInput, "
               "const BrnGameState::GameStateModuleIO::GameStateToGuiInterface* lpInterface) { "
               + invocation + " }\n")
    inc = ("namespace BrnGameState { namespace GameStateModuleIO {\n" + "\n".join(gui_parts) + "\n} }\n"
           + "namespace BrnGame {\n" + push + "\n" + implementation + "\n" + adapter + "}\n")
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
