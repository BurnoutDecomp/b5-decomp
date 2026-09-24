"""FX-AI-RUMBLE G07-D1: the traffic legs of ResetOnTrackManager::TestCarHNG (X360 0x82790BD8).

Extracts the PRODUCTION bodies of ResetOnTrackManager::TestCarHNG (BrnResetOnTrackManager_AvoidObstacles.cpp),
BrnAI::LineTestTrafficHNG @0x8277A878 (BrnHNGTest.cpp) and the 4-argument BrnAI::DistancePointToLine
@0x827653C0 (BrnAIUtils.cpp) and checks them against values worked from the ARTIST asm. A body that the
revision under test does not have is replaced by a stub that answers "not in the way" (the pre-fix
behaviour), so the old tree reports the missing behaviour as failed checks instead of a link error.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxairumble_traffic_hng.py [--rev <b5 rev>]
"""
import sys
sys.dont_write_bytecode = True
from aimod_common import Tree, parse_args, compile_and_run, body_or_stub, definition, REPO

AVOID = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_AvoidObstacles.cpp"
HNG = "src/GameSource/World/AI/BrnHNGTest.cpp"
UTILS = "src/GameSource/World/AI/BrnAIUtils.cpp"

TRAFFIC_STUB = """bool LineTestTrafficHNG(const NearbyVehicles*, Vector2, Vector2)
{
    return false;   // [stub] no body in this revision: traffic is never 'in the way'
}"""

DISTANCE_STUB = """f32 DistancePointToLine(Vector2, Vector2, Vector2, Vector2&)
{
    return std::nanf("");   // [stub] no body in this revision
}"""


def main():
    tree = Tree(parse_args().rev)
    avoid, hng, utils = tree.read(AVOID), tree.read(HNG), tree.read(UTILS)
    chunks = ["#include <cstdlib>",
              "namespace BrnAI {",
              body_or_stub(utils, "f32 DistancePointToLine(Vector2 l2DPoint,", DISTANCE_STUB),
              body_or_stub(hng, "bool LineTestTrafficHNG(", TRAFFIC_STUB),
              # TestCarHNG's [DIAG] BRN_ROT_TRAFFIC_DIAG witness helper (file-local); absent pre-fix.
              "namespace {", body_or_stub(avoid, "    void NoteTrafficLegs(", ""), "}",
              definition(avoid, "bool ResetOnTrackManager::TestCarHNG("),
              "}"]
    # BrnMathUtils.cpp: TestCarHNG's :2216 assert calls BrnMath::IsNormal(Vector2) (FX-NANPOL
    # 2026-09-24, the G07-D1 leftover).
    sys.exit(compile_and_run("FxAiRumbleTrafficHng.cpp", chunks,
                             [REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",
                              REPO / "src/GameSource/Math/BrnMathUtils.cpp"],
                             prefix="brn_fxairumble_hng_"))


if __name__ == "__main__":
    main()
