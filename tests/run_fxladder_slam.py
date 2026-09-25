"""FX-LADDER slam commit: VehiclePhysics::AddSlam @0x825D4870, VehiclePhysics::UpdateSlam @0x825D4950 and
VehicleManager::CalculateSlamData @0x825C7568 -- the console's NaN arms (fcmpu + bgt / blt, and the fsel
clamps, which take their ELSE operand on a NaN) and its fused roundings (fnmsubs / fmadds).

Extracts the three production bodies, CalculateSlamData's per-situation tables and SignOrZero, and runs
FxLadderSlam.cpp's checks on the real VehicleManager / RaceCarPhysics structs against a model of the asm.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxladder_slam.py [--rev <rev>]

`--rev <b5 rev>` reads the two sources from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, constant, definition, parse_args  # noqa: E402

VEHICLE_PHYSICS = "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"
IMPACT_HELPERS = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_ImpactHelpers.cpp"
TABLES = ["KAF_SLAM_SITUATION_SCALE", "KAF_SLAM_RECOVERY_TIME", "KAF_SLAM_RECOVERY_TIME_ALT",
          "KAF_SLAM_VULNERABLE_BASE"]


def main():
    args = parse_args()
    tree = Tree(args.rev)
    physics = tree.read(VEHICLE_PHYSICS).replace("\r\n", "\n")
    helpers = tree.read(IMPACT_HELPERS).replace("\r\n", "\n")
    chunks = ["namespace BrnPhysics { namespace Vehicle {", "namespace vpu = rw::math::vpu;",
              definition(physics, "    s8 VehiclePhysics::AddSlam("),
              definition(physics, "    void VehiclePhysics::UpdateSlam("),
              "namespace {"]
    chunks += [constant(helpers, name) for name in TABLES]
    chunks.append(definition(helpers, "        inline f32 SignOrZero("))
    chunks.append("}")
    chunks.append(definition(helpers, "    void VehicleManager::CalculateSlamData("))
    chunks.append("} }")
    sys.exit(compile_and_run("FxLadderSlam.cpp", chunks, prefix="brn_fxladder_slam_"))


if __name__ == "__main__":
    main()
