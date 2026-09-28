"""L2 CAMCOLLIDE stage (c) (owner's list 2026-09-28): the cylinder's line kernel against the ARTIST words run on emu64.

  rw::collision::CylinderVolume::LineSegIntersect      @0x82BAF688   (the descriptor's lineSegIntersect entry)
  rw::collision::CylinderVolume::ThinLineSegIntersect  @0x82BADCE0   (total fatness exactly 0)
  rw::collision::CylinderVolume::FatLineSegIntersect   @0x82BAEB10   (any other: the fattened cylinder, its cap edge
                                                                      rounded by a torus)
  all in src/vendor/renderware/collision/LineSegIntersect.cpp

On the PC none of them had a body: the cylinder's descriptor slot was parked (a LOUD trap in VolumeLineQuery::
GetIntersections @0x82BB3470), so the camera's line tests (FineIntersectionTestModule::ComputeLineTestNearest ->
VolumeLineQuery) could not hit a cylinder.

Numeric: tests/L2LineKernelNan.cpp built with /DL2_LKN_CYLINDER=1 compiles the revision's LineSegIntersect.cpp beside
it (the revision's headers shadowed in) and replays tests/L2CylinderLineData.h, generated from the console's words on
emu64 by scratch/OWNERLIST_0927/L2/emu/gen_cylinder_line.py (seed 928, 8758 rows). Kind 5 calls the thin arm
directly: 96 fixed finite rows (four frames, one translated only -- the radial early-out's CONSOLE QUIRK reads the
volume's own translation row -- twelve local segments: the barrel, both caps, both beyond a cap, radially away, inside,
parallel to the axis in and out of the radius, grazing, a cap-disc miss into the barrel; each with and without a
transform), 30 cap-disc boundary rows, 500 random. Kind 6 calls the dispatcher: 240 fixed rows (three frames, the
volume's fatness / the caller's / both / neither, ten segments: the barrel, both caps, down past the rim, into the rim,
across just above the cap, both above, radially away, inside, up past the cap edge) and 500 random. Every fixed base
has one lane of pt1 / pt2 / the half height / the radius / the fatness / a frame lane / tm set to +-NaN / +-inf.
Every lane bit for bit (a NaN by class), grouped by the fcmpu site whose NaN arm the row reaches:
  0x82BAE1A0  thin: the cap point's x^2 + y^2 < r^2 (`bge`: a NaN goes on to the barrel)
  0x82BAF95C  rwcCylinderLineSegIntersect's discriminant (`bge`: a NaN carries on)
  0x82BAEFA4  fat: the cap point's x^2 + y^2 < r^2 * 1.001 (double; `bge`)
  0x82BAEFC8  fat: the torus is wanted, x^2 + y^2 < (r + fat)^2 (`bge`)
  0x82BAF370  fat: the torus clip interval is empty, enter < exit (`bge`)
  0x82BAF3F0  fat: the torus root is nearer than the result's lineParam (`blt`)
except the rows that run the torus arm (check T, FLAG PC-platform: SolveQuarticRoots' powf cascades are powf on the
PC, VmxPowF), judged to a tolerance. A revision without the bodies does not build (every numeric check fails).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l2_cylinder_line.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

C = "src/vendor/renderware/collision/"
LSI_CPP = C + "LineSegIntersect.cpp"
AALC_CPP = C + "AALineClipper.cpp"      # the fat arm's torus clip (AALineClipper::AALineClipper @0x82BAE3C8)
HEADERS = [C + "CollisionVolume.hpp", C + "CapsuleVolume.hpp", C + "CylinderVolume.hpp", C + "LineSegIntersect.hpp",
           C + "LineSegKernelMath.hpp", C + "AALineClipper.hpp"]
KERNELS = ["RwBool CylinderVolume::ThinLineSegIntersect(", "RwBool CylinderVolume::FatLineSegIntersect(",
           "RwBool CylinderVolume::LineSegIntersect("]
VTABLES = C + "VolumeVTables.cpp"
NUMERIC_CHECKS = 7   # F1 + the four reached NaN arms (N0..N3) + O + T


def _read(tree, path):
    try:
        return tree.read(path)
    except (OSError, ValueError):
        return ""


def wiring(tree):
    header = code_only(_read(tree, C + "CylinderVolume.hpp"))
    vt = code_only(_read(tree, VTABLES))
    declared = all(re.search(r"RwBool\s+" + name + r"\s*\(\s*const\s+Vec4\s*&\s*\w+\s*,\s*const\s+Vec4\s*&\s*\w+\s*,"
                             r"\s*const\s+Vec4\s*\*\s*\w+\s*,\s*VolumeLineSegIntersectResult\s*&\s*\w+\s*,\s*f32\s+\w+\s*\)"
                             r"\s*const\s*;", header) is not None
                   for name in ("LineSegIntersect", "ThinLineSegIntersect", "FatLineSegIntersect"))
    match = re.search(r"const\s+Volume::VTable\s+gVolumeHandler_82F91894\s*=\s*\{(.*?)\};", vt, re.S)
    slots = [item.strip() for item in match.group(1).split(",")] if match else []
    return [
        ("CylinderVolume.hpp declares LineSegIntersect / ThinLineSegIntersect / FatLineSegIntersect (pt1, pt2, tm, "
         "result, fatness) const", declared),
        ("VolumeVTables.cpp: the CYLINDER record's lineSegIntersect slot (the 7th word, @0x82F91894 + 0x18) is bound to "
         "CylinderLineSegIntersect (it was parked NULL: a LOUD trap in VolumeLineQuery::GetIntersections)",
         len(slots) > 6 and slots[6] == "CylinderLineSegIntersect"),
    ]


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
    if not aalc_text:
        print("NUMERIC: cannot build -- " + AALC_CPP + " is absent")
        return None
    with tempfile.TemporaryDirectory(prefix="brn_l2_cyl_") as directory:
        lsi = Path(directory) / "LineSegIntersect.cpp"
        lsi.write_text(lsi_text, encoding="utf-8")
        aalc = Path(directory) / "AALineClipper.cpp"
        aalc.write_text(aalc_text, encoding="utf-8")
        return compile_and_run(Path(__file__).with_name("L2LineKernelNan.cpp"), "l2_cyl_unused.inc",
                               "// not included: the kernel comes from LineSegIntersect.cpp\n",
                               "L2LineKernelNan", extra_flags="/DL2_LKN_CYLINDER=1", shadow=shadow,
                               extra_sources=(lsi, aalc))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_l2_cylinder_line", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
