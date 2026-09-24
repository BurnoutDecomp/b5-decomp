"""FX-AINAN2: NaN polarity of the reset-on-track placement (ResetOnTrack/BrnResetOnTrackManager*.cpp).

Extracts the production bodies of ResetOnTrackManager::ComputeNearestPositionInSegment,
GetRoadSideForStartingLine, ScanBackwardsAlongExtrapolatedRoute and
ScanForwardsAlongExtrapolatedRoute (plus GetAICar and the Strategies helper namespace) and feeds
them NaNs. See FxAinan2ResetOnTrack.cpp for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_reset_on_track.py [--rev <rev>]

`--rev <b5 rev>` reads the production files from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, compile_and_run, definition, parse_args  # noqa: E402

MANAGER = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.cpp"
STRATEGIES = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_Strategies.cpp"
EXTRA = [REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp",
         REPO / "src/GameSource/Math/BrnMathUtils.cpp",
         REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"]


def main():
    args = parse_args()
    tree = Tree(args.rev)
    manager, strategies = tree.read(MANAGER), tree.read(STRATEGIES)
    chunks = ["namespace BrnAI {", "namespace vpu = rw::math::vpu;",
              definition(manager, "    Vector3 ResetOnTrackManager::ComputeNearestPositionInSegment("),
              definition(manager, "    AICar* ResetOnTrackManager::GetAICar("),
              definition(strategies, "namespace\n{"),
              definition(strategies, "f32 ResetOnTrackManager::GetRoadSideForStartingLine("),
              definition(strategies, "bool ResetOnTrackManager::ScanBackwardsAlongExtrapolatedRoute("),
              definition(strategies, "bool ResetOnTrackManager::ScanForwardsAlongExtrapolatedRoute("),
              "}"]
    sys.exit(compile_and_run("FxAinan2ResetOnTrack.cpp", chunks, extra_sources=EXTRA,
                             prefix="brn_fxainan2_rot_"))


if __name__ == "__main__":
    main()
