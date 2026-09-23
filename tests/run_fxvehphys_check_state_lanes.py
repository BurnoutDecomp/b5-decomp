"""Regression for crash-parity G31-D1 (FX-VEHPHYS, 2026-09-23): ExternalPhysicsBody::CheckState
@0x825A24B0 tests the six Vector3 members on their xyz lanes only.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvehphys_check_state_lanes.py [--pre-fix <b5 rev>]

The console splats lanes 0/1/2 of mTotalLinearForce/mTotalTorque/mTotalLinearImpulse/
mTotalAngularImpulse/mLinearVelocity/mAngularVelocity and self-compares each (vspltw + vcmpeqfp.);
only mfMass is compared as a whole register. The runner extracts the production helper block and
CheckState body from ExternalPhysicsBody.cpp; FxVehphysCheckStateLanes.cpp runs them on a fixture.
"""
import sys

sys.dont_write_bytecode = True
from fxvehphys_common import REPO, build_and_run, definition, pre_fix_rev, read

SOURCE = "src/GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    source = read(SOURCE, rev)
    helpers = definition(source, "    namespace\n    {\n        inline bool IsLaneNaN(")
    method = definition(source, "    void ExternalPhysicsBody::CheckState(")
    method = method.replace("ExternalPhysicsBody::", "CheckStateFixture::")
    rc = build_and_run(REPO / "tests" / "FxVehphysCheckStateLanes.cpp",
                       {"check_state_helpers.inc": helpers, "check_state_method.inc": method},
                       "fxvehphys_check_state")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
