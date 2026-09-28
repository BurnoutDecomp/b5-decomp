"""L2 CAMCOLLIDE (owner's list 2026-09-27): the ROUNDING of the two primitive line tests the volume line kernels call,
against the ARTIST words run on emu64.

  rwcSphereLineSegIntersect   @0x82BA81D8
  rwcCylinderLineSegIntersect @0x82BAF8A0      -- both in src/vendor/renderware/collision/LineSegIntersect.cpp
  linemath::Cross             (LineSegKernelMath.hpp) -- the console's two-permute cross product

The wave-2 lowering rounded every product and sum separately. The console (scratch ROUNDING_RULE.md):
  vmsum3fp128                              one rounding of the f64 sum (rule 1)        linemath::Dot3
  vpermwi128 0x63 / vmulfp128 / vnmsubfp   x = round(a.y*b.z) - a.z*b.y in ONE rounding (rule 3), etc.
                                           rwcSphere 0x82BA8250..0x82BA8270, rwcCylinder 0x82BAF8A4..0x82BAF8C4 /
                                           0x82BAF8FC..0x82BAF910                     linemath::Cross
  fmsubs f11,f12,f0,f11 (rwcSphere 0x82BA8280), fmsubs f13,f11,f11,f13 / fmadds f13,f10,f0,f13 (rwcCylinder
  0x82BAF950 / 0x82BAF954)                 one rounding each (rule 3)                  std::fma

Numeric: the same tests/L2LineKernelNan.cpp as run_l2_line_kernel_nan.py, built with /DL2_LKN_FINITE_ONLY: the 1299
FINITE rows of tests/L2LineKernelNanData.h must match the console bit for bit --
  F1  659 kernel rows (spheres, boxes and capsules, with and without a transform and a fatness, covering every arm of
      the walks; 600 of them random): return, result.v, position, normal, volParam and lineParam;
  F2  640 rows of the two primitives called directly (the kernels only ever hand the cylinder a unit coordinate axis,
      whose crosses are exact -- these give it arbitrary axes; 40 sphere rows put |toCentre|^2 between its one-rounding
      and its sequential-sum value, so the immediate-inside test flips on the rounding alone): return and the
      Fraction.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l2_line_kernel_rounding.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

C = "src/vendor/renderware/collision/"
LSI_CPP = C + "LineSegIntersect.cpp"
HEADERS = [C + "CollisionVolume.hpp", C + "CapsuleVolume.hpp", C + "LineSegIntersect.hpp", C + "LineSegKernelMath.hpp",
           C + "CylinderVolume.hpp", C + "AALineClipper.hpp"]
# LineSegIntersect.cpp holds the cylinder's line kernel since 2026-09-28 (lane L2, stage (c)): its fat arm builds an
# AALineClipper, so the revision's AALineClipper.cpp links beside it.
AALC_CPP = C + "AALineClipper.cpp"
KERNELS = ["RwBool SphereVolume::LineSegIntersect(", "RwBool BoxVolume::LineSegIntersect(",
           "RwBool CapsuleVolume::LineSegIntersect("]
NUMERIC_CHECKS = 2    # F1 the kernels, F2 the primitives: the finite rows, bit for bit


def _read(tree, path):
    try:
        return tree.read(path)
    except (OSError, ValueError):
        return ""


def numeric(tree):
    lsi_text = _read(tree, LSI_CPP)
    for signature in KERNELS:
        try:
            definition(lsi_text, signature)
        except ValueError:
            print("NUMERIC: cannot build -- LineSegIntersect.cpp has no body for " + signature.strip())
            return None
    shadow = {}
    for path in HEADERS:
        text = _read(tree, path)
        if not text:
            print("NUMERIC: cannot build -- " + path + " is absent")
            return None
        shadow[path] = text
    aalc_text = _read(tree, AALC_CPP)
    with tempfile.TemporaryDirectory(prefix="brn_l2_lkr_") as directory:
        lsi = Path(directory) / "LineSegIntersect.cpp"
        lsi.write_text(lsi_text, encoding="utf-8")
        sources = [lsi]
        if aalc_text:
            aalc = Path(directory) / "AALineClipper.cpp"
            aalc.write_text(aalc_text, encoding="utf-8")
            sources.append(aalc)
        return compile_and_run(Path(__file__).with_name("L2LineKernelNan.cpp"), "l2_lkr_unused.inc",
                               "// not included: the kernels come from LineSegIntersect.cpp\n",
                               "L2LineKernelNan", extra_flags="/DL2_LKN_FINITE_ONLY=1", shadow=shadow,
                               extra_sources=tuple(sources))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_l2_line_kernel_rounding", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
