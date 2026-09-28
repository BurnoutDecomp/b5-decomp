"""L2 CAMCOLLIDE stage (c) (owner's list 2026-09-28): the cylinder's line kernel against the ARTIST words run on emu64.

  rw::collision::CylinderVolume::ThinLineSegIntersect @0x82BADCE0   (src/vendor/renderware/collision/LineSegIntersect.cpp)

The zero-fatness arm of CylinderVolume::LineSegIntersect @0x82BAF688 (the dispatcher and the fat arm @0x82BAEB10 are
the next stage). On the PC it had no body: the cylinder's descriptor slot is parked, so the camera's line tests
(FineIntersectionTestModule::ComputeLineTestNearest -> VolumeLineQuery) could not hit a cylinder.

Numeric: tests/L2LineKernelNan.cpp built with /DL2_LKN_CYLINDER=1 compiles the revision's LineSegIntersect.cpp beside
it (the revision's headers shadowed in) and replays tests/L2CylinderLineData.h, generated from the console's words on
emu64 by scratch/OWNERLIST_0927/L2/emu/gen_cylinder_line.py (seed 928, 5588 rows): 96 fixed finite rows (four frames,
one translated only -- the radial early-out's CONSOLE QUIRK reads the volume's own translation row -- twelve local
segments: the barrel, both caps, both beyond a cap, radially away, inside, parallel to the axis in and out of the
radius, grazing, a cap-disc miss into the barrel; each with and without a transform) and 500 random, every lane bit
for bit; every fixed base with one lane of pt1 / pt2 / the half height / the radius / a frame lane / tm set to
+-NaN / +-inf. The two fcmpu sites are grouped by their NaN arm:
  0x82BAE1A0  the cap point's x^2 + y^2 < r^2 (`bge`: a NaN goes on to the barrel)
  0x82BAF95C  rwcCylinderLineSegIntersect's discriminant (`bge`: a NaN carries on)
A revision without the body does not build (every numeric check counts as failed).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l2_cylinder_line.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

C = "src/vendor/renderware/collision/"
LSI_CPP = C + "LineSegIntersect.cpp"
HEADERS = [C + "CollisionVolume.hpp", C + "CapsuleVolume.hpp", C + "CylinderVolume.hpp", C + "LineSegIntersect.hpp",
           C + "LineSegKernelMath.hpp"]
KERNELS = ["RwBool CylinderVolume::ThinLineSegIntersect("]
NUMERIC_CHECKS = 4   # F1 + the two NaN arms + O


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
    with tempfile.TemporaryDirectory(prefix="brn_l2_cyl_") as directory:
        lsi = Path(directory) / "LineSegIntersect.cpp"
        lsi.write_text(lsi_text, encoding="utf-8")
        return compile_and_run(Path(__file__).with_name("L2LineKernelNan.cpp"), "l2_cyl_unused.inc",
                               "// not included: the kernel comes from LineSegIntersect.cpp\n",
                               "L2LineKernelNan", extra_flags="/DL2_LKN_CYLINDER=1", shadow=shadow,
                               extra_sources=(lsi,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_l2_cylinder_line", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
