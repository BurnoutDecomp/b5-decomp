"""Regression for crash-parity G44-D2 part 1 (FX-VMNET, 2026-09-23): VehicleDriver::StartCatchupInterpolation.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvmnet_catchup.py [--pre-fix <b5 rev>]

FxVmnetCatchup.cpp compiles the shipped body @0x825FED30 (and its file-scope constants) from
BrnVehicleDriver.cpp as a member of the real VehicleDriver and checks it against numbers produced by
EXECUTING the console's machine code on the same inputs (see the harness banner). Pre-fix the method
was declared and had no body anywhere, so the harness is compiled with a body that does nothing --
which is what a network catch-up amounted to on the PC.
"""
import sys

sys.dont_write_bytecode = True
from fxvmnet_common import REPO, build_and_run, definition, pre_fix_rev, read

DRIVER = "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.cpp"
CONSTANTS = "    namespace\n    {\n        // DWARF BrnVehicleDriver.cpp:239"
SIGNATURE = "    void VehicleDriver::StartCatchupInterpolation("
NO_BODY = ("void VehicleDriver::StartCatchupInterpolation(VehiclePhysics*, const Matrix44Affine&,\n"
           "    const Vector3, const Vector3, bool) {}\n")


def main():
    rev = pre_fix_rev(sys.argv)
    source = read(DRIVER, rev)
    failures = []
    try:
        body = definition(source, CONSTANTS) + "\n" + definition(source, SIGNATURE)
    except ValueError:
        failures.append("G44-D2: VehicleDriver::StartCatchupInterpolation has no body (harness runs a no-op)")
        body = NO_BODY

    rc = build_and_run(REPO / "tests" / "FxVmnetCatchup.cpp", {"catchup_body.inc": body}, "fxvmnet_catchup")
    for failure in failures:
        print("FAIL:", failure)
    print(f"structural/extraction: {len(failures)} of 1 failed; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
