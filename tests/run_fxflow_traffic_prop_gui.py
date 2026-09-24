"""FX-FLOW (crash parity 2026-09-24, G10-D9 caller): BrnGameModule::BridgeWorldTrafficAndPropDataToGui.

  BridgeWorldToGui @0x823EDD50 calls BridgeWorldTrafficAndPropDataToGui @0x823E5560 at 0x823EDD84,
  between the vehicle-data leg (0x823EDD68) and the impact leg (0x823EDD94). It walks the world
  output's GUI queue and forwards 512 (1 byte), 208 (0x290) and 210 (0x410) -- the last two as an
  empty record (count word only) when the mode is not in progress -- 209 (0x38, then
  CrashModeScoring::DealWithRemovedTraffic @0x8232BF90, its only caller) and 592 (4). The body was
  absent on PC: the removed-traffic list never reached the Showtime scorer, so a traffic car that
  reused a slot index stayed "recently crashed" until its entry aged out of the 64-entry set.

Numeric: tests/FxFlowTrafficPropGui.cpp compiles the extracted bridge + umbrella and the extracted
DealWithRemovedTraffic against the real CrashModeScoring, the real VariableEventQueue<32768,16> and
the real GUI record types (a labelled empty stand-in where a revision lacks the bridge).
Wiring: the declaration, and the umbrella's call order.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_traffic_prop_gui.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report, REPO, STRSTREAM_CPP

BRIDGE_CPP = "src/GameSource/Game/GameBridgeWorldToGui.cpp"
MODULE_HPP = "src/GameSource/Game/BrnGameModule.hpp"
SCORING_CPP = "src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp"
EXTRA = [STRSTREAM_CPP, REPO / "src/GameShared/GameClasses/Module/VariableEventQueue_32768_16.cpp"]
NUMERIC_CHECKS = 16

BRIDGE = "void BrnGameModule::BridgeWorldTrafficAndPropDataToGui("
BRIDGE_STAND_IN = ("void BrnGameModule::BridgeWorldTrafficAndPropDataToGui("
                   "CgsGui::CgsGuiModuleIO::InputBuffer*, const BrnWorldIO::UpdateOutputBuffer*) {}")
UMBRELLA = "void BrnGameModule::BridgeWorldToGui("
REMOVED = "    void CrashModeScoring::DealWithRemovedTraffic("


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    header = squash(tree.read(MODULE_HPP))
    yield ("BrnGameModule declares BridgeWorldTrafficAndPropDataToGui(InputBuffer* lpGuiInput, const "
           "UpdateOutputBuffer* lpWorldOutput) (DWARF BrnGameModule.h:820)",
           "voidBridgeWorldTrafficAndPropDataToGui(CgsGui::CgsGuiModuleIO::InputBuffer*lpGuiInput,"
           "constBrnWorldIO::UpdateOutputBuffer*lpWorldOutput);" in header)
    umbrella = squash(body_or_empty(tree.read(BRIDGE_CPP), UMBRELLA))
    vehicle = umbrella.find("BridgeWorldVehicleDataToGui(lpGuiInputBuffer,lpWorldOutputBuffer);")
    traffic = umbrella.find("BridgeWorldTrafficAndPropDataToGui(lpGuiInputBuffer,lpWorldOutputBuffer);")
    impact = umbrella.find("BridgeWorldImpactInformationToGui(lpGuiInputBuffer,lpWorldOutputBuffer);")
    yield ("BridgeWorldToGui calls the traffic/prop leg after the vehicle-data leg and before the impact "
           "leg (0x823EDD68 -> 0x823EDD84 -> 0x823EDD94)",
           0 <= vehicle < traffic < impact)


def numeric(tree):
    bridge_source = tree.read(BRIDGE_CPP)
    try:
        umbrella = definition(bridge_source, UMBRELLA)
        removed = definition(tree.read(SCORING_CPP), REMOVED)
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    try:
        bridge = definition(bridge_source, BRIDGE)
    except ValueError:
        bridge = "// [stand-in: body absent in this revision]\n" + BRIDGE_STAND_IN
        print("NUMERIC: bodies absent in this revision (empty stand-ins): BridgeWorldTrafficAndPropDataToGui")
    inc = ("namespace BrnGame {\n" + umbrella + "\n" + bridge + "\n}\n"
           "namespace BrnGameState {\n" + removed + "\n}\n")
    return compile_and_run(Path(__file__).with_name("FxFlowTrafficPropGui.cpp"), "traffic_prop_gui.inc", inc,
                           "FxFlowTrafficPropGui", extra_sources=EXTRA)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxflow_traffic_prop_gui", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
