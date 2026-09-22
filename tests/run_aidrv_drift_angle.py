"""FX-AIDRV G03-D3: replay the production AIDriver::DetermineDriftSteeringAngle (ARTIST @0x827931D0).

CAR_MOVING with a degenerate planar velocity falls back to the facing; a degenerate chosen vector
returns 0.0 before FindFinalDriftDirection. See AIDrvDriftAngle.cpp.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aidrv_drift_angle.py [--rev <rev>]
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, compile_and_run, definition, parse_args  # noqa: E402

AIDRIVER_UPDATE = "src/GameSource/World/AI/BrnAIDriver_Update.cpp"


def main():
    args = parse_args()
    source = Tree(args.rev).read(AIDRIVER_UPDATE)
    chunks = ["namespace BrnAI {",
              "namespace {",
              definition(source, "        Vector2 Normalize2DU(Vector2 lVector)"),
              definition(source, "        Vector2 To2DU(Vector3 lVector)"),
              "}",
              definition(source, "    f32 AIDriver::DetermineDriftSteeringAngle(EDriftDirectionSelection leSelection)"),
              "}"]
    extra = [REPO / "src/GameSource/World/AI/BrnAIUtils_Angles.cpp"]
    sys.exit(compile_and_run("AIDrvDriftAngle.cpp", chunks, extra, prefix="brn_aidrv_drift_"))


if __name__ == "__main__":
    main()
