"""FX-TAILS-A item 6 (crash parity 2026-09-24): AStar::BuildRoute normalises the route's last segment with no zero
guard.

Extracts the PRODUCTION AStar::BuildRoute (@0x8277F930), AStarNodePool::GetNode, AStar::KF_ZERO_EPSILON and
Route::AddNode (BrnRoute.cpp, @0x827642A0) and builds the extrapolated exit node over a last segment of zero
length: the console's vrsqrtefp + two Newton-Raphson steps (0x8277FB74..0x8277FBB4) make the travel direction NaN,
every portal's dot is unordered and the `fcmpu ; ble` @0x8277FD60 keeps the seed portal 0. See
FxTailsAAStarNormalise.cpp.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsa_astar_normalise.py [--rev <rev>]

`--rev <b5 rev>` reads the production files from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, compile_and_run, constant, definition, parse_args  # noqa: E402

ASTAR = "src/GameSource/World/AI/Route/BrnAStar.cpp"
ROUTE = "src/GameSource/World/AI/Route/BrnRoute.cpp"
EXTRA = [REPO / "src/GameSource/Math/BrnMathUtils.cpp"]


def main():
    args = parse_args()
    tree = Tree(args.rev)
    astar, route = tree.read(ASTAR), tree.read(ROUTE)
    chunks = ["namespace BrnAI {",
              constant(astar, "AStar::KF_ZERO_EPSILON"),
              definition(astar, "AStarNode* AStarNodePool::GetNode("),
              definition(astar, "void AStar::BuildRoute(AStarNode* lpBestNode, Route* lpOutRoute)"),
              definition(route, "bool Route::AddNode(const Vector4& lrNode)"),
              "}"]
    sys.exit(compile_and_run("FxTailsAAStarNormalise.cpp", chunks, extra_sources=EXTRA,
                             prefix="brn_fxtailsa_astarnorm_"))


if __name__ == "__main__":
    main()
