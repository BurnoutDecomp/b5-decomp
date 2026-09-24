"""FX-AINAN2: NaN branch polarity along the AI driver's control outputs -- the steering chain
(PID -> clamp -> StepTo) and the throttle side (boost timer, the Saturate fsel ladder).

Extracts the production BrnAI::StepTo (BrnAIUtils.cpp, ARTIST @0x82766BA8), the PIDController bodies
(BrnPIDController.cpp, GetErrorDerivative @0x82768440) and AIDriver::CalculateSteeringAngle /
UpdateSteeringAngle with their TU helpers (BrnAIDriver.cpp, @0x8277CD18 / @0x827708F0) and feeds
them NaNs. See FxAinan2SteerChain.cpp for the console addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_steer_chain.py [--rev <rev>]

`--rev <b5 rev>` reads every production file from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, body_or_stub, compile_and_run, definition, parse_args  # noqa: E402

AIDRIVER = "src/GameSource/World/AI/BrnAIDriver.cpp"
UTILS = "src/GameSource/World/AI/BrnAIUtils.cpp"
PID = "src/GameSource/World/AI/PID/BrnPIDController.cpp"


def main():
    args = parse_args()
    tree = Tree(args.rev)
    driver = tree.read(AIDRIVER)
    utils = tree.read(UTILS)
    pid = tree.read(PID)
    # The two file-scope statics + WitnessSteeringInputs (the [steer] witness the actuator reads).
    witness_block = definition(driver, "    namespace\n    {\n        Vector2 sgSteerHeading;")
    chunks = ["namespace BrnAI {",
              definition(utils, "    f32 StepTo(f32 lfCurrent, f32 lfTarget, f32 lfStep)"),
              "}",
              "namespace BrnAI {",
              definition(pid, "f32 PIDController::GetError()"),
              definition(pid, "f32 PIDController::GetErrorDerivative()"),
              definition(pid, "f32 PIDController::GetOutput()"),
              definition(pid, "void PIDController::Prepare(const f32* lafCoefficientValues)"),
              definition(pid, "void PIDController::Record(f32 lfError, f32 lfTimeStep)"),
              "}",
              "namespace BrnAI {",
              definition(driver, "    static inline bool IsFinite(f32 lfValue)"),
              definition(driver, "    static Vector2 Normalize2D(Vector2 lVector)"),
              definition(driver, "    static Vector2 To2D(Vector3 lVector)"),
              # new in the fix; the pre-fix tree has no ClampFsel (its clamp is the inline if/if)
              body_or_stub(driver, "    static inline f32 ClampFsel(", ""),
              witness_block,
              definition(driver, "    static void WitnessSteeringInputs(Vector2 lHeading, Vector2 lTarget)"),
              definition(driver, "    void AIDriver::UpdateSteeringAngle(f32 lfTargetAngle)"),
              definition(driver, "    void AIDriver::CalculateSteeringAngle(f32 lfTimeStep)"),
              definition(driver, "    static inline f32 Saturate(f32 lfValue)"),
              definition(driver, "    void AIDriver::AttemptToDriveAtDesiredSpeed(f32 lfTimeStep)"),
              definition(driver, "    f32 AIDriver::ProximitySpeed(f32 lfMinSpeed)"),
              "}"]
    extra = [REPO / "src/GameSource/World/AI/BrnAIUtils_Angles.cpp"]
    sys.exit(compile_and_run("FxAinan2SteerChain.cpp", chunks, extra, prefix="brn_fxainan2_steer_"))


if __name__ == "__main__":
    main()
