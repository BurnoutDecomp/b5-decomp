"""FX-TRAFFIC5 (crash parity wave 5, 2026-09-24): the sign the traffic response arms take.

  PhysicalTrafficManager::SetTrafficVehicleChecked @0x8262D748 and ::SetTrafficVehicleSlammed @0x825EFDE8
  build their steering-side and drive-direction signs with `vcmpgtfp ; vcmpgefp ; vsel ; vsel`, and
  ::TestForNearMissFreakOut @0x82637A30 calls rw::math::fpu::Sgn<VecFloat> @0x825BC920 (==0 -> 0,
  >=0 -> 1, else -1). Both give -1 for a NaN; the tree's shared SignumLane returned 0.

Wiring: the three arms take their signs from SignumLane.
Numeric: tests/FxTraffic5Signum.cpp compiles the PRODUCTION SignumLane (working tree, or --rev <b5 rev>).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic5_signum.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report

RESPONSE_CPP = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_CrashResponse.cpp"
NUMERIC_CHECKS = 7
HELPER = "    inline f32 SignumLane(f32 lfValue)"


def wiring(tree):
    source = tree.read(RESPONSE_CPP)
    side = body_or_empty(source, "    inline f32 SteeringSideSignum(")
    checked = body_or_empty(source, "void PhysicalTrafficManager::SetTrafficVehicleChecked(")
    slammed = body_or_empty(source, "void PhysicalTrafficManager::SetTrafficVehicleSlammed(")
    near = body_or_empty(source, "void PhysicalTrafficManager::TestForNearMissFreakOut(")
    return [
        ("the side sign of the checked / slammed arms is SignumLane (0x8262D908 / 0x825EFFD0)",
         "SignumLane(" in side and "SteeringSideSignum(" in checked and "SteeringSideSignum(" in slammed),
        ("the drive sign of the checked / slammed arms is SignumLane (0x8262D9C8 / 0x825F0090)",
         "SignumLane(lpRaceCarPhysics->GetSpeedMPH()" in checked and "SignumLane(lpRaceCarPhysics->GetSpeedMPH()" in slammed),
        ("the near-miss side is SignumLane == rw::math::fpu::Sgn (0x82637C30)", "SignumLane(" in near),
    ]


def numeric(tree):
    try:
        text = definition(tree.read(RESPONSE_CPP), HELPER)
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic5Signum.cpp"), "signum.inc", text, "FxTraffic5Signum")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic5_signum", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
