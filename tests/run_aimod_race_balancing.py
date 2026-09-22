"""FX-AIMOD G04-D4 + G05-D4: replay the production race rubber-band chain.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aimod_race_balancing.py [--rev <b5 rev>]
"""
import sys
sys.dont_write_bytecode = True
from aimod_common import Tree, parse_args, body_or_stub, constants, compile_and_run, definition, REPO

MANAGER = "src/GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.cpp"
ROUTE = "src/GameSource/World/AI/RaceBalancing/BrnRaceBalancingRoute.cpp"
GRAPH = "src/GameSource/World/AI/RaceBalancing/BrnRaceBalancingGraph.cpp"


def main():
    tree = Tree(parse_args().rev)
    manager, route, graph = tree.read(MANAGER), tree.read(ROUTE), tree.read(GRAPH)
    chunks = ['#include "GameSource/World/BrnWorldSharedConstants.h"', "namespace BrnAI {",
              definition(route, "namespace\n{"),
              definition(route, "f32 RaceBalancingRoute::ComputeRaceCompletionRatio("),
              definition(route, "f32 RaceBalancingRoute::GetTime("),
              definition(route, "f32 RaceBalancingRoute::GetAISectionSpeed("),
              definition(route, "bool RaceBalancingRoute::Prepare("),
              definition(route, "bool RaceBalancingRoute::Recalculate("),
              definition(graph, "void RaceBalancingGraph::Construct("),
              definition(graph, "void RaceBalancingGraph::SetPoint("),
              definition(graph, "f32 RaceBalancingGraph::ComputeSpeedRatio("),
              constants(manager, r"^const f32 KF_\w+\s*=[^;]+;"),
              definition(manager, "void RaceBalancingManager::OnRaceStart("),
              definition(manager, "void RaceBalancingManager::OnRaceStartPlaying("),
              definition(manager, "void RaceBalancingManager::OnRaceEnd("),
              body_or_stub(manager, "void RaceBalancingManager::Update(",
                           "void RaceBalancingManager::Update(const AICar*, f32) {}  // ABSENT in this revision"),
              body_or_stub(manager, "void RaceBalancingManager::UpdateOpponentRoute(",
                           "void RaceBalancingManager::UpdateOpponentRoute(const AICar*, const AISectionsData*) {}  // ABSENT"),
              definition(manager, "void RaceBalancingManager::CalculateScheduleOffset("),
              "}"]
    sys.exit(compile_and_run("AIModRaceBalancing.cpp", chunks,
                             [REPO / "src/GameSource/Math/BrnMathUtils.cpp"], prefix="brn_aimod_racebal_"))


if __name__ == "__main__":
    main()
