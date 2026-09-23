"""Regression for crash-parity G51-D2 (FX-VEHPHYS, 2026-09-23): VehiclePhysics::
UpdateLinearVelocityMagnitude @0x825C0000 writes the direction only when |v| > FLT_EPSILON.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvehphys_linear_velocity_magnitude.py [--pre-fix <b5 rev>]

The console stores (0,0,0,|v|) to +0x1340 and returns (0x825C00C0 beqlr) unless |v| > stru_8208F620
lane 0 (0x34000000); only then does it write v/|v| with w = |v|. The runner extracts the production
body from VehiclePhysics.cpp; FxVehphysLinearVelocityMagnitude.cpp runs it on six velocities.
"""
import sys

sys.dont_write_bytecode = True
from fxvehphys_common import REPO, build_and_run, definition, pre_fix_rev, read

SOURCE = "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    method = definition(read(SOURCE, rev), "    void VehiclePhysics::UpdateLinearVelocityMagnitude(")
    method = method.replace("VehiclePhysics::", "SpeedFixture::")
    rc = build_and_run(REPO / "tests" / "FxVehphysLinearVelocityMagnitude.cpp",
                       {"linear_velocity_magnitude.inc": method}, "fxvehphys_speed")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
