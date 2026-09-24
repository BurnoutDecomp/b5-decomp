"""FX-TAILS-A item 6 (crash parity 2026-09-24): the ResetOnTrack Strategies TU's Normalise3D has no zero guard.

Extracts the PRODUCTION Strategies helper namespace (Normalise3D, IsSimilar3D, Dot3D, ...),
ResetOnTrackManager::ResetNearRoutelessPlayer (@0x827844D8) and GetAICar, and checks that a zero vector
normalises to what the console's vmsum3fp128 + vrsqrtefp + two Newton-Raphson steps give (NaN lanes),
including the reset direction a degenerate portal pair hands ResetNearRoutelessPlayer
(0x8278462C..0x82784684). See FxTailsAResetNormalise.cpp.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsa_reset_normalise.py [--rev <rev>]

`--rev <b5 rev>` reads the production files from that revision (the RED side of the fix).
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
              definition(strategies, "bool ResetOnTrackManager::ResetNearRoutelessPlayer("),
              # the test reaches the TU-local helper through this name
              "Vector3 TestNormalise3D(Vector3 lVector) { return Normalise3D(lVector); }",
              "}"]
    sys.exit(compile_and_run("FxTailsAResetNormalise.cpp", chunks, extra_sources=EXTRA,
                             prefix="brn_fxtailsa_rotnorm_"))


if __name__ == "__main__":
    main()
