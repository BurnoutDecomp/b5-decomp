"""FX-FLOW (crash parity 2026-09-24, NEW-EMMTAIL): the tail of GameStateModule::EmmPreWorldUpdate.

  EmmPreWorldUpdate @0x8238EF50 does not stop at UpdateRoadRulesManager. From 0x8238F1BC (where the
  paused and unpaused arms meet) it publishes the player's game-mode elapsed time
  (SetGameModeElapsedTime 0x8238F214), refreshes the network interface's active-race-car mapping from
  every scored car (0x8238F22C..0x8238F288, SetActiveRaceCarIndex 0x823558A0) and posts the player's
  overtake record onto the GUI interface (0x8238F28C..0x8238F33C, +0xC8 -> GUI 371). None of the
  three existed on PC, so GUI 371 had no producer.

Numeric: tests/FxFlowEmmTail.cpp compiles the extracted tail with the extracted
GameStateToGuiInterface Construct / AddOvertakeEvent / GetOvertakeEventQueue, ScoringSystem
GetCarData (both) / GetRaceCarTotalTime / GetCarRacePosition and GameStateToNetworkInterface
SetActiveRaceCarIndex / GetActiveRaceCarIndex, against the real scoring, GUI-interface and
network-interface types (labelled empty stand-ins where a revision lacks a body).
Wiring: PreWorldUpdateStuntBringUp runs the tail right after the road-rules leg, outside its
IsSimPaused gate and before the scoring publish.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_emm_tail.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report, REPO, STRSTREAM_CPP

GUI_CPP = "src/GameSource/GameState/GameStateModule_gUI_00.cpp"
IFACE_CPP = "src/GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.cpp"
LOOKUP_CPP = "src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_Lookup.cpp"
QUERIES_CPP = "src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_Queries.cpp"
NET_CPP = "src/GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.cpp"
EXTRA = [STRSTREAM_CPP]
NUMERIC_CHECKS = 11

TAIL = "void GameStateModule::EmmPreWorldUpdateTailBringUp("
TAIL_STAND_IN = ("void GameStateModule::EmmPreWorldUpdateTailBringUp("
                 "const CgsSystem::TimerStatusInterface&) {}")
OVERTAKE = "void GameStateToGuiInterface::AddOvertakeEvent("
OVERTAKE_STAND_IN = "void GameStateToGuiInterface::AddOvertakeEvent(u8, ::EActiveRaceCarIndex) {}"


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    body = squash(body_or_empty(tree.read(GUI_CPP), "void GameStateModule::PreWorldUpdateStuntBringUp("))
    road = body.find("UpdateRoadRulesManagerImpactTimeBringUp(lpActionQueue);")
    tail = body.find("EmmPreWorldUpdateTailBringUp(lrTimerStatusInterface);")
    publish = body.find("CopyScoringDataToOutput(mpOutputBuffer,lrTimerStatusInterface);")
    # The tail must sit outside the `if (!IsSimPaused(true, false)) { ... }` road-rules block: the
    # first `}` after the road-rules call closes that block, and the tail comes after it.
    close = body.find("}", road) if road >= 0 else -1
    yield ("PreWorldUpdateStuntBringUp runs the tail after the road-rules leg, outside its IsSimPaused "
           "gate, before CopyScoringDataToOutput (0x8238F188 bne -> 0x8238F1BC; #86 before #89)",
           0 <= road < close < tail < publish)


def numeric(tree):
    parts = []
    stood_in = []

    def need(path, signature):
        parts.append(definition(tree.read(path), signature))

    try:
        need(LOOKUP_CPP, "CarData* ScoringSystem::GetCarData(EActiveRaceCarIndex")
        need(LOOKUP_CPP, "const CarData* ScoringSystem::GetCarData(EActiveRaceCarIndex")
        need(QUERIES_CPP, "    u32 ScoringSystem::GetCarRacePosition(")
        need(QUERIES_CPP, "    const CgsSystem::Time ScoringSystem::GetRaceCarTotalTime(")
        iface = tree.read(IFACE_CPP)
        parts.append("namespace GameStateModuleIO {")
        parts.append(definition(iface, "void GameStateToGuiInterface::Construct("))
        parts.append(definition(iface, "const GameStateToGuiInterface::OvertakeEventQueue* "
                                       "GameStateToGuiInterface::GetOvertakeEventQueue("))
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    try:
        parts.append(definition(iface, OVERTAKE))
    except ValueError:
        parts.append("// [stand-in: body absent in this revision]\n" + OVERTAKE_STAND_IN)
        stood_in.append("AddOvertakeEvent")
    parts.append("}")
    try:
        parts.append(definition(tree.read(GUI_CPP), TAIL))
    except ValueError:
        parts.append("// [stand-in: body absent in this revision]\n" + TAIL_STAND_IN)
        stood_in.append("EmmPreWorldUpdateTailBringUp")
    if stood_in:
        print("NUMERIC: bodies absent in this revision (empty stand-ins): " + ", ".join(stood_in))

    net = tree.read(NET_CPP)
    try:
        net_parts = [definition(net, "        const EActiveRaceCarIndex GameStateToNetworkInterface::GetActiveRaceCarIndex("),
                     definition(net, "        void GameStateToNetworkInterface::SetActiveRaceCarIndex(")]
    except ValueError as error:
        print("NUMERIC: cannot build -- network interface body absent: " + str(error))
        return None
    inc = ("namespace BrnGameState {\n" + "\n".join(parts) + "\n}\n"
           "namespace BrnNetwork { namespace BrnNetworkModuleIO {\n" + "\n".join(net_parts) + "\n} }\n")
    return compile_and_run(Path(__file__).with_name("FxFlowEmmTail.cpp"), "emm_tail.inc", inc,
                           "FxFlowEmmTail", extra_sources=EXTRA)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxflow_emm_tail", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
