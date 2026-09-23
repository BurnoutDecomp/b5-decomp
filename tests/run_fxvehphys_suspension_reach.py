"""Regression for crash-parity G52-D1 (FX-VEHPHYS, 2026-09-23): the suspension-reach gate of
VehiclePhysics::UpdateSuspensionPostSimulation @0x825F6BB0 compares against the line-plane distance.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvehphys_suspension_reach.py [--pre-fix <b5 rev>]

The console's gate is 0x825F7064 `vcmpgefp. v13,v13,v4` with v13 = (p.y - susp.x) + radius and
v4 = splat(t), t = the line-plane distance (0x825F6EB0, f31 -> var_1D0 at 0x825F700C). The runner
extracts the production text of the first wheel loop (its "@0x825F6C74..7130" banner up to the next
loop's "@0x825F7134..729C" banner) from UpdateSuspensionPostSimulation; FxVehphysSuspensionReach.cpp
runs it on three single-wheel fixtures.
"""
import sys

sys.dont_write_bytecode = True
from fxvehphys_common import REPO, build_and_run, definition, pre_fix_rev, read

SOURCE = "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"
LOOP_START = "        // @0x825F6C74..7130:"
LOOP_END = "        // @0x825F7134..729C:"


def main():
    rev = pre_fix_rev(sys.argv)
    body = definition(read(SOURCE, rev), "    void VehiclePhysics::UpdateSuspensionPostSimulation(")
    start = body.index(LOOP_START)
    loop = body[start:body.index(LOOP_END, start)]
    if "lbWheelWithinSuspensionReach" not in loop:
        print("FAIL: the extracted loop has no reach gate")
        sys.exit(1)
    rc = build_and_run(REPO / "tests" / "FxVehphysSuspensionReach.cpp",
                       {"suspension_reach_loop.inc": loop}, "fxvehphys_reach")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
