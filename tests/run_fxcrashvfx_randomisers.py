"""FX-CRASHVFX (crash parity 2026-09-24): the two per-lane vector randomisers
Vector3Randomiser::RandomiseXYZ @0x82277EC8 and Vector4Randomiser::RandomiseXYZW @0x82277FB8.

Their combine `mVecA + mVecB * r` is ONE vmaddfp on the console -- fused, rounded once. The tree wrote it as a
C++ `a + b * r`, which MSVC rounds twice; 28 of the 192 lanes this test draws land one bit off that way.

NUMERIC -- tests/FxCrashVfxRandomisers.cpp compiles the two PRODUCTION bodies (extracted from BrnEffectsUtils.cpp)
against the revision's header and the real CgsNumeric::Random, and compares 24 draws of each with the console's
own outputs (tests/FxCrashVfxRandomiserData.h, the functions' real instruction words run by
scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_randomiser_data.py): every lane and the ring / seed / cursor.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_randomisers.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, compile_and_run, definition, report

SOURCE = "src/GameSource/Effects/BrnEffectsUtils.cpp"
HEADER = "src/GameSource/Effects/BrnEffectsUtils.h"
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
SIGNATURES = ("Vector3 Vector3Randomiser::RandomiseXYZ(", "Vector4 Vector4Randomiser::RandomiseXYZW(")
NUMERIC_CHECKS = 4


def numeric(tree):
    source = tree.read(SOURCE).replace("\r\n", "\n")
    bodies = []
    for signature in SIGNATURES:
        try:
            bodies.append(definition(source, signature))
        except ValueError:
            print("NUMERIC: cannot build -- " + signature + " is absent")
            return None
    return compile_and_run(Path(__file__).with_name("FxCrashVfxRandomisers.cpp"), "fxcrashvfx_randomisers.inc",
                           "\n".join(bodies) + "\n", "FxCrashVfxRandomisers", shadow={HEADER: tree.read(HEADER)},
                           extra_sources=(RANDOM_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    args = parser.parse_args()
    return report("run_fxcrashvfx_randomisers", [], numeric(Tree(args.rev)), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
