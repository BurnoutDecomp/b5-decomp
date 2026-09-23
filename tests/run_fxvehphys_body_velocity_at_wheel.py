"""Regression for crash-parity G54-D1 (FX-VEHPHYS, 2026-09-23): the airborne arm of
VehiclePhysics::CalculateBodyVelocityAtWheelContact @0x825FB200 rotates the body-local wheel position
(mPosition, radius off .y) into world space with no translation.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvehphys_body_velocity_at_wheel.py [--pre-fix <b5 rev>]

The console's airborne arm (0x825FB2B0..0x825FB448) loads wheel+0x80, subtracts splat(wheel+0x40 .w)
from lane y only, forms Up*y + Right*x + At*z and never loads this+0x40; the grounded arm
(0x825FB224..0x825FB2A4) uses RoadContact.mPosition - Pos(). The runner extracts the production body;
FxVehphysBodyVelocityAtWheel.cpp checks both arms on a yawed car far from the origin.
"""
import sys

sys.dont_write_bytecode = True
from fxvehphys_common import REPO, build_and_run, definition, pre_fix_rev, read

SOURCE = "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    method = definition(read(SOURCE, rev), "    void VehiclePhysics::CalculateBodyVelocityAtWheelContact(")
    method = method.replace("VehiclePhysics::", "WheelVelocityFixture::")
    rc = build_and_run(REPO / "tests" / "FxVehphysBodyVelocityAtWheel.cpp",
                       {"body_velocity_at_wheel.inc": method}, "fxvehphys_wheelvel")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
