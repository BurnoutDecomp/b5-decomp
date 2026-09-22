"""FX-AIMOD G07-D2 + G07-D4: reset type 5 (ResetAheadFromSideTurnings, ScanForwardsAndAlongJunction,
InterpolatePositionFromAngle) replayed from the production bodies.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aimod_side_turnings.py [--rev <b5 rev>]
A body the revision under test does not have is replaced by a stub, so the RED side reports the
missing behaviour instead of failing to link.
"""
import re
import sys
sys.dont_write_bytecode = True
from aimod_common import Tree, parse_args, compile_and_run, definition, body_or_stub, REPO

STRATEGIES = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_Strategies.cpp"
MANAGER = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.cpp"
AICAR_UPDATE = "src/GameSource/World/AI/BrnAICar_Update.cpp"

STUB_INTERPOLATE = """Vector3 ResetOnTrackManager::InterpolatePositionFromAngle(Vector2, Vector2, Vector3, Vector3, f32)
{ return Vector3{ -999.0f, -999.0f, -999.0f, 0.0f }; }"""
STUB_SCAN = """bool ResetOnTrackManager::ScanForwardsAndAlongJunction(ResetOnTrackCoords*) { return false; }"""


def main():
    tree = Tree(parse_args().rev)
    strategies, manager, aicar = tree.read(STRATEGIES), tree.read(MANAGER), tree.read(AICAR_UPDATE)
    stream = re.search(r"^[ \t]*static CgsNumeric::Random gAICarRandom;", aicar, re.M)[0]
    chunks = ["namespace BrnAI {", "namespace vpu = rw::math::vpu;",
              stream,
              definition(aicar, "    f32 AICar::GetRandomNumber() const"),
              definition(manager, "    AICar* ResetOnTrackManager::GetAICar("),
              definition(strategies, "namespace\n{"),
              body_or_stub(strategies, "Vector3 ResetOnTrackManager::InterpolatePositionFromAngle(",
                           STUB_INTERPOLATE),
              body_or_stub(strategies, "bool ResetOnTrackManager::ScanForwardsAndAlongJunction(",
                           STUB_SCAN),
              definition(strategies, "bool ResetOnTrackManager::ResetAheadFromSideTurnings("),
              "}"]
    sys.exit(compile_and_run("AIModSideTurnings.cpp", chunks,
                             [REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp",
                              REPO / "src/GameSource/Math/BrnMathUtils.cpp",
                              REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"],
                             prefix="brn_aimod_sideturn_"))


if __name__ == "__main__":
    main()
