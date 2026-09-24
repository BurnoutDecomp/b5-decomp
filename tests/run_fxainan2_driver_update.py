"""FX-AINAN2: NaN branch polarity in the AIDriver partfile BrnAIDriver_Update.cpp.

Extracts the production SaturateU / Dot2D / To2DU helpers and AIDriver::HardShoulderSpeed,
DoSlowTurn, FindPositionInFuture, AttemptToDriveAtDesiredSpeedInDrift, UpdatePlayerTimers,
ChooseAggressiveSteeringFan and UpdateStuck, then feeds them NaNs. See FxAinan2DriverUpdate.cpp
for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_driver_update.py [--rev <rev>]

`--rev <b5 rev>` reads the production file from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, definition, parse_args  # noqa: E402

AIDRIVER_UPDATE = "src/GameSource/World/AI/BrnAIDriver_Update.cpp"


def main():
    args = parse_args()
    source = Tree(args.rev).read(AIDRIVER_UPDATE)
    chunks = ["namespace BrnAI {",
              "namespace {",
              definition(source, "        Vector2 To2DU(Vector3 lVector)"),
              definition(source, "        inline f32 SaturateU(f32 lfValue)"),
              definition(source, "        inline f32 Dot2D(Vector2 lA, Vector2 lB)"),
              "}",
              definition(source, "    f32 AIDriver::HardShoulderSpeed(f32 lfInputSpeed)"),
              definition(source, "    void AIDriver::DoSlowTurn(f32 lfTimeStep)"),
              definition(source, "    bool AIDriver::FindPositionInFuture("),
              definition(source, "    void AIDriver::AttemptToDriveAtDesiredSpeedInDrift()"),
              definition(source, "    void AIDriver::UpdatePlayerTimers(f32 lfTimeStep, AICar* lpPlayerCar)"),
              definition(source, "    s32 AIDriver::ChooseAggressiveSteeringFan(AICar* lpPlayerCar)"),
              definition(source, "    void AIDriver::UpdateStuck(f32 lfTimeStep)"),
              "}"]
    sys.exit(compile_and_run("FxAinan2DriverUpdate.cpp", chunks, prefix="brn_fxainan2_drvupd_"))


if __name__ == "__main__":
    main()
