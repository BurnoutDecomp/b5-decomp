"""FX-TAILS-A item 6 (crash parity 2026-09-24): ResetOnTrackManager::AvoidObstacles normalises the flattened reset
direction with no zero guard.

Extracts the PRODUCTION AvoidObstacles (@0x827941E0), TestRecentResets, GetAICar and the TU helper namespace and
replays AvoidObstacles with a zero / vertical reset direction, observing the direction TestCarHNG receives: the
console's vrsqrtefp + two Newton-Raphson steps (0x827942A0..0x827942FC) hand it NaN lanes. See
FxTailsAAvoidNormalise.cpp.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsa_avoid_normalise.py [--rev <rev>]

`--rev <b5 rev>` reads the production files from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, compile_and_run, definition, parse_args  # noqa: E402

AVOID = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_AvoidObstacles.cpp"
MANAGER = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.cpp"
EXTRA = [REPO / "src/GameSource/Math/BrnMathUtils.cpp",
         REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"]


def main():
    args = parse_args()
    tree = Tree(args.rev)
    avoid, manager = tree.read(AVOID), tree.read(MANAGER)
    chunks = ["namespace BrnAI {",
              definition(avoid, "namespace\n{"),
              definition(manager, "    AICar* ResetOnTrackManager::GetAICar("),
              definition(avoid, "bool ResetOnTrackManager::TestRecentResets("),
              definition(avoid, "bool ResetOnTrackManager::AvoidObstacles("),
              "}"]
    sys.exit(compile_and_run("FxTailsAAvoidNormalise.cpp", chunks, extra_sources=EXTRA,
                             prefix="brn_fxtailsa_avoidnorm_"))


if __name__ == "__main__":
    main()
