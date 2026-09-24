"""FX-AINAN2: NaN polarity of AStarNodePool::ExtractBestOpenNode's cost-weight assert and its
best-score select (Route/BrnAStar.cpp).

Extracts the production body and counts the asserts a NaN / ordered cost weight fires, and checks
that a NaN score never wins the open-set scan. See FxAinan2AStar.cpp for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_astar.py [--rev <rev>]

`--rev <b5 rev>` reads the production file from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, definition, parse_args  # noqa: E402

ASTAR = "src/GameSource/World/AI/Route/BrnAStar.cpp"


def main():
    args = parse_args()
    source = Tree(args.rev).read(ASTAR)
    chunks = ["namespace BrnAI {",
              "s32 dword_8300D530 = -1;",
              definition(source, "AStarNode* AStarNodePool::GetNode("),
              definition(source, "AStarNode* AStarNodePool::ExtractBestOpenNode("),
              "}"]
    sys.exit(compile_and_run("FxAinan2AStar.cpp", chunks, prefix="brn_fxainan2_astar_"))


if __name__ == "__main__":
    main()
