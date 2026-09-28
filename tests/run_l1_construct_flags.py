"""L1 (owner's list 2026-09-28, item "camera behind walls / below the map"): the collision tuning the chase cam and the
ICE-anim behaviour give their car-attached policy right after its Construct.

  src/GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.cpp   Construct @0x82224A18
  src/GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.cpp            Construct @0x82256100

The console re-tunes the policy the moment its Construct returns: the chase cam (0x82224AF8 / B34 / B3C / B44) turns
auto-elevate off and world-only line tests, the frustum resolver and radius smoothing on; the ICE-anim behaviour
(0x82256264 / 68) turns on world-only tests and the frustum resolver. The PC made none of the six stores (an 08-01
note mistook the chase cam's four for the policy's own), so both cameras ran the policy's plain arm.

Numeric: tests/L1ConstructFlags.cpp compiles the revision's two Construct bodies (extracted into
l1_construct_flags.inc) against the revision's headers, runs each on a 0xA5-filled behaviour, and compares the
policy's eight bools and two tail floats with tests/L1ConstructFlagsData.h -- the console's own, the ARTIST words of
both Constructs run on emu64 by scratch/OWNERLIST_0927/L1/emu/gen_construct_flags_l1.py.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l1_construct_flags.py [--rev <b5 rev>]
"""
import argparse
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

BEH = "src/GameSource/Director/Camera/Behaviours/"
GAMEPLAY_CPP = BEH + "BrnBehaviourGameplayExternal.cpp"
ICEANIM_CPP = BEH + "BrnBehaviourIceAnim.cpp"
HEADERS = (BEH + "BrnBehaviourGameplayExternal.h", BEH + "BrnBehaviourIceAnim.h",
           "src/GameSource/Director/Camera/BrnCollisionPolicy.h")
VEHICLEREF_CPP = Path(__file__).resolve().parents[1] / "src/GameSource/Director/Utils/BrnVehicleRef.cpp"
NUMERIC_CHECKS = 4   # K1..K4


def _read(tree, path):
    try:
        return tree.read(path)
    except (OSError, ValueError):
        return ""


def numeric(tree):
    try:
        gameplay = definition(_read(tree, GAMEPLAY_CPP), "void BehaviourGameplayExternal::Construct()")
        iceanim = definition(_read(tree, ICEANIM_CPP), "void BehaviourIceAnim::Construct()")
    except ValueError:
        print("NUMERIC: cannot build -- a Construct body is missing")
        return None
    shadow = {path: _read(tree, path) for path in HEADERS}
    return compile_and_run(Path(__file__).with_name("L1ConstructFlags.cpp"), "l1_construct_flags.inc",
                           gameplay + "\n\n" + iceanim + "\n", "L1ConstructFlags", shadow=shadow,
                           extra_sources=(VEHICLEREF_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_l1_construct_flags", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
