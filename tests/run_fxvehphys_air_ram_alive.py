"""Regression for crash-parity G54-D2 (FX-VEHPHYS, 2026-09-23): VehiclePhysics::UpdateAirRam
@0x825FC8D8 releases a slot unless |mImpulse|^2 > flt_82F2A430 == 0x3C23D70B.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvehphys_air_ram_alive.py [--pre-fix <b5 rev>]

x360rd reads 0x82F2A430 = 0x3C23D70B (0.0100000007f); the literal 0.01f is 0x3C23D70A, one ULP low.
An impulse of (0.1f,0,0) squares to exactly 0x3C23D70B, so the console (0x825FCA40 vcmpgtfp.)
releases it while the one-ULP-low threshold fired it once more. The runner extracts the production
UpdateAirRam body; FxVehphysAirRamAlive.cpp runs it on three slots.
"""
import sys

sys.dont_write_bytecode = True
from fxvehphys_common import REPO, build_and_run, definition, pre_fix_rev, read

SOURCE = "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    method = definition(read(SOURCE, rev), "    void VehiclePhysics::UpdateAirRam(")
    method = method.replace("VehiclePhysics::", "AirRamAliveFixture::")
    rc = build_and_run(REPO / "tests" / "FxVehphysAirRamAlive.cpp",
                       {"air_ram_alive.inc": method}, "fxvehphys_airram")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
