"""FX-AINAN2: RouteRequestManager::ChooseDistanceFunction's driving-away test (BrnRouteRequestManager.cpp).

Extracts the production body (+ KF_HACK_CONTRYSIDE_DIVIDE) and checks which A* heuristic it picks
for a car heading toward / away from its destination, and with a NaN heading. See
FxAinan2RouteRequest.cpp for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_route_request.py [--rev <rev>]

`--rev <b5 rev>` reads the production file from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, compile_and_run, constant, definition, parse_args  # noqa: E402

MANAGER = "src/GameSource/World/AI/BrnRouteRequestManager.cpp"


def main():
    args = parse_args()
    source = Tree(args.rev).read(MANAGER)
    chunks = ["namespace BrnAI {",
              constant(source, "KF_HACK_CONTRYSIDE_DIVIDE"),
              definition(source, "AStarDistanceFunction RouteRequestManager::ChooseDistanceFunction("),
              "}"]
    sys.exit(compile_and_run("FxAinan2RouteRequest.cpp", chunks,
                             extra_sources=[REPO / "src/GameSource/Math/BrnMathUtils.cpp"],
                             prefix="brn_fxainan2_rrm_"))


if __name__ == "__main__":
    main()
