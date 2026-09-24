"""FX-AINAN2: NaN polarity of the race rubber-band clamps (RaceBalancing/*.cpp).

Extracts the production RaceBalancingRoute (ComputeRaceCompletionRatio, GetTime,
GetAISectionSpeed, Prepare, Recalculate), RaceBalancingGraph (Construct, SetPoint,
ComputeSpeedRatio) and RaceBalancingManager (OnRaceStart, ComputeParSpeed, the GraphType
ComputeTargetSpeed) bodies and feeds NaNs into their fpu::Clamp<float> fsel ladders. See
FxAinan2RaceBalancing.cpp for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_race_balancing.py [--rev <rev>]

`--rev <b5 rev>` reads the production files from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aimod_common import REPO, Tree, compile_and_run, constants, definition, parse_args  # noqa: E402

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
              definition(manager, "f32 RaceBalancingManager::ComputeParSpeed("),
              definition(manager, "f32 RaceBalancingManager::ComputeTargetSpeed(GraphType leGraphType,"),
              "}"]
    sys.exit(compile_and_run("FxAinan2RaceBalancing.cpp", chunks,
                             [REPO / "src/GameSource/Math/BrnMathUtils.cpp"], prefix="brn_fxainan2_racebal_"))


if __name__ == "__main__":
    main()
