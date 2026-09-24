"""FX-TAILS-A item 6 (crash parity 2026-09-24): the AI driver's planar normalises have no zero guard.

Extracts the PRODUCTION Normalize2D / To2D / ClampFsel / Saturate and AIDriver::CorneringTopSpeed
(BrnAIDriver.cpp, ARTIST @0x8277D0F0) and Normalize2DU / To2DU, AIDriver::GetQuickTurnSteering
(@0x8277C600) and AIDriver::UpdateBrakingAnticipationData (@0x827964C0, image bytes) from
BrnAIDriver_Update.cpp, and checks that a zero vector normalises to what the console's vrsqrtefp + two
Newton-Raphson steps give (NaN lanes) and what that does one level up. See FxTailsADriverNormalise.cpp.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsa_driver_normalise.py [--rev <rev>]

`--rev <b5 rev>` reads the production files from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, compile_and_run, definition, parse_args  # noqa: E402

AIDRIVER = "src/GameSource/World/AI/BrnAIDriver.cpp"
UPDATE = "src/GameSource/World/AI/BrnAIDriver_Update.cpp"


def main():
    args = parse_args()
    tree = Tree(args.rev)
    driver, update = tree.read(AIDRIVER), tree.read(UPDATE)
    chunks = ["namespace BrnAI {",
              definition(driver, "    static Vector2 Normalize2D(Vector2 lVector)"),
              definition(driver, "    static Vector2 To2D(Vector3 lVector)"),
              definition(driver, "    static inline f32 ClampFsel("),
              definition(driver, "    static inline f32 Saturate(f32 lfValue)"),
              definition(driver, "    f32 AIDriver::CorneringTopSpeed(f32 lfInputSpeed)"),
              "namespace {",
              definition(update, "        Vector2 Normalize2DU(Vector2 lVector)"),
              definition(update, "        Vector2 To2DU(Vector3 lVector)"),
              "}",
              definition(update, "    f32 AIDriver::GetQuickTurnSteering(Vector2 lVectorToTarget)"),
              definition(update, "    void AIDriver::UpdateBrakingAnticipationData()"),
              # the test reaches the two TU-local helpers through these names
              "Vector2 TestNormalize2D(Vector2 lVector) { return Normalize2D(lVector); }",
              "Vector2 TestNormalize2DU(Vector2 lVector) { return Normalize2DU(lVector); }",
              "}"]
    extra = [REPO / "src/GameSource/World/AI/BrnAIUtils_Angles.cpp"]
    sys.exit(compile_and_run("FxTailsADriverNormalise.cpp", chunks, extra, prefix="brn_fxtailsa_drvnorm_"))


if __name__ == "__main__":
    main()
