"""FX-TAILS-A item 8: production ResetOnTrack log gates and typed route-node consumers.

The console prints "<AI> routeless fail" (0x8278450C..0x8278451C), "<AI> Car tried to start ahead of player..."
(0x82784738..0x82784748), "<AI> Nodes in same place" (0x82785E9C..0x82785EB4), "Lerping t from a to b"
(0x82785ED4..0x82785F38) and "Converting nodes to position & direction" (0x82790380..0x82790390) only under
`CgsDev::Message::gxMessageFilterFlags & 1`; the tree printed the first four whenever gpDebugPrint was set and never
printed the fifth.

Numeric: FxTailsARotLogGates.cpp runs the PRODUCTION ResetNearRoutelessPlayer, DeterminePositionBetweenNodes and
ConvertNodesToPositionAndDirection (with the TU helper namespace and GetAICar) against a capturing gpDebugPrint with
bit 0 of the filter clear and set. Historical comment/commit wording is not a runtime witness.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsa_rot_log_gates.py [--rev <rev>]
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, compile_and_run, definition, parse_args  # noqa: E402

MANAGER = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.cpp"
STRATEGIES = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_Strategies.cpp"
EXTRA = [REPO / "src/GameSource/Math/BrnMathUtils.cpp",
         REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"]


def main():
    args = parse_args()
    tree = Tree(args.rev)
    manager, strategies = tree.read(MANAGER), tree.read(STRATEGIES)

    chunks = ["namespace BrnAI {",
              definition(manager, "    AICar* ResetOnTrackManager::GetAICar("),
              definition(strategies, "namespace\n{"),
              definition(strategies, "Vector3 ResetOnTrackManager::DeterminePositionBetweenNodes("),
              definition(strategies, "bool ResetOnTrackManager::ConvertNodesToPositionAndDirection("),
              definition(strategies, "bool ResetOnTrackManager::ResetNearRoutelessPlayer("),
              "}"]
    rc = compile_and_run("FxTailsARotLogGates.cpp", chunks, extra_sources=EXTRA, prefix="brn_fxtailsa_rotlog_")
    print(f"run_fxtailsa_rot_log_gates: numeric rc={rc}")
    sys.exit(rc)


if __name__ == "__main__":
    main()
