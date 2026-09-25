"""FX-FOLLOWUPS (crash parity 2026-09-25, stage (b)): the volume line kernels behind the descriptor's lineSegIntersect
slot, which the VolumeLineQuery walk (GetIntersections @0x82BB3470, d6040b9f) calls for every staged primitive.

  SphereVolume::LineSegIntersect   @0x82BA82C8   (the slot was parked NULL)
  BoxVolume::LineSegIntersect      @0x82BA9478   (the slot was parked NULL)
  CapsuleVolume::LineSegIntersect  @0x82BAFCF8   (the slot was parked NULL, the header BLOCKED)
  rwcPlaneLineSegIntersect         @0x82BA8818   (no body)
                                   -- all four in LineSegIntersect.cpp, beside the rwc* kernels they call
  LineSegKernelMath.hpp            the console's rounding for all four (ROUNDING_RULE rules 1 / 3 / 4 / 5)
  VolumeVTables.cpp                the SPHERE / CAPSULE / BOX records bind them

Numeric: tests/FxFollowupsLineKernels.cpp compiles the revision's LineSegIntersect.cpp beside it, with the revision's
headers shadowed in, and checks the plane test's return codes and fused num, the sphere / box / capsule arms and walks
(lineParam as the console sums it, volParam, the world frame), and the rounding idioms over many inputs. A revision
without the kernels cannot build the numeric half: every numeric check then counts as failed.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_line_kernels.py [--rev <b5 rev>]
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
HEADERS = [C + "CollisionVolume.hpp", C + "CapsuleVolume.hpp", C + "LineSegIntersect.hpp", C + "LineSegKernelMath.hpp"]
VTABLES = C + "VolumeVTables.cpp"
KERNELS = ["RwBool SphereVolume::LineSegIntersect(", "RwBool BoxVolume::LineSegIntersect(",
           "RwBool CapsuleVolume::LineSegIntersect(", "s32 rwcPlaneLineSegIntersect("]
NUMERIC_CHECKS = 40
DECLARATION = (r"RwBool\s+LineSegIntersect\s*\(\s*const\s+Vec4\s*&\s*\w+\s*,\s*const\s+Vec4\s*&\s*\w+\s*,\s*"
               r"const\s+Vec4\s*\*\s*\w+\s*,\s*VolumeLineSegIntersectResult\s*&\s*\w+\s*,\s*f32\s+\w+\s*\)\s*const\s*;")


def _read(tree, path):
    try:
        return tree.read(path)
    except (OSError, ValueError):
        return ""


def wiring(tree):
    cv = code_only(_read(tree, C + "CollisionVolume.hpp"))
    cap = code_only(_read(tree, C + "CapsuleVolume.hpp"))
    lsi = code_only(_read(tree, C + "LineSegIntersect.hpp"))
    vt = code_only(_read(tree, VTABLES))

    def struct_body(text, name):
        match = re.search(r"struct\s+" + name + r"\s*:\s*public\s+Volume\s*\{(.*?)\n    \};", text, re.S)
        return match.group(1) if match else ""

    def record(name):
        match = re.search(r"const\s+Volume::VTable\s+" + name + r"\s*=\s*\{(.*?)\};", vt, re.S)
        return [item.strip() for item in match.group(1).split(",")] if match else []

    slots = {name: record(name) for name in ("gVolumeHandler_82F91740", "gVolumeHandler_82F918C0",
                                             "gVolumeHandler_82F9176C")}
    return [
        ("CollisionVolume.hpp: BoxVolume and SphereVolume declare LineSegIntersect(pt1, pt2, tm, result, fatness) const "
         "(DWARF box.h:176 / sphere.h:90)",
         re.search(DECLARATION, struct_body(cv, "BoxVolume")) is not None
         and re.search(DECLARATION, struct_body(cv, "SphereVolume")) is not None),
        ("CapsuleVolume.hpp declares LineSegIntersect (DWARF capsule.h:144)", re.search(DECLARATION, cap) is not None),
        ("LineSegIntersect.hpp declares rwcPlaneLineSegIntersect(Fraction*, f32 orig, f32 seg, f32 sign, f32 disp) "
         "(rwccore.h:3716)",
         re.search(r"s32\s+rwcPlaneLineSegIntersect\s*\(\s*Fraction\s*\*\s*\w+\s*,\s*f32\s+\w+\s*,\s*f32\s+\w+\s*,\s*"
                   r"f32\s+\w+\s*,\s*f32\s+\w+\s*\)\s*;", lsi) is not None),
        ("VolumeVTables.cpp: the lineSegIntersect slot (the 7th word) of SPHERE / CAPSULE / BOX is bound to "
         "Sphere / Capsule / BoxLineSegIntersect",
         len(slots["gVolumeHandler_82F91740"]) > 6 and slots["gVolumeHandler_82F91740"][6] == "SphereLineSegIntersect"
         and len(slots["gVolumeHandler_82F918C0"]) > 6 and slots["gVolumeHandler_82F918C0"][6] == "CapsuleLineSegIntersect"
         and len(slots["gVolumeHandler_82F9176C"]) > 6 and slots["gVolumeHandler_82F9176C"][6] == "BoxLineSegIntersect"),
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
    with tempfile.TemporaryDirectory(prefix="brn_fxfu_lk_") as directory:
        lsi = Path(directory) / "LineSegIntersect.cpp"
        lsi.write_text(lsi_text, encoding="utf-8")
        return compile_and_run(Path(__file__).with_name("FxFollowupsLineKernels.cpp"), "fxfu_lk_unused.inc",
                               "// not included: the kernels come from LineSegIntersect.cpp\n",
                               "FxFollowupsLineKernels", shadow=shadow, extra_sources=(lsi,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxfollowups_line_kernels", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
