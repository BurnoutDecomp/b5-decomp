"""Regression for crash-parity G51-D1 (FX-VEHPHYS, 2026-09-23): the velocity term of
VehiclePhysics::MaintainDriftSpeed @0x825D2270 is mLinearVelocity / speedParam, not Normalize(v).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvehphys_maintain_drift_speed.py [--pre-fix <b5 rev>]

The console computes 1/speedParam (0x825D2398 vrefp v2 + two Newton steps), multiplies it by
[this+0x50] (0x825D23C0) and by deficit*mass, and accumulates it times AlongVel (0x825D23C8). The
runner extracts the production body from VehiclePhysics.cpp; FxVehphysMaintainDriftSpeed.cpp runs it
on five fixtures; three of them (A, D, E: |v| != speedParam) separate the two readings.
"""
import sys

sys.dont_write_bytecode = True
from fxvehphys_common import REPO, build_and_run, definition, pre_fix_rev, read

SOURCE = "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    method = definition(read(SOURCE, rev), "    void VehiclePhysics::MaintainDriftSpeed(")
    method = method.replace("VehiclePhysics::", "DriftFixture::")
    rc = build_and_run(REPO / "tests" / "FxVehphysMaintainDriftSpeed.cpp",
                       {"maintain_drift_speed.inc": method}, "fxvehphys_drift")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
