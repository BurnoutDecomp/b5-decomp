"""L2 CAMCOLLIDE stage (c) (owner's list 2026-09-28): rw::collision::AALineClipper against the ARTIST words on emu64.

  rw::collision::AALineClipper::Init           @0x828AED60
  rw::collision::AALineClipper::AALineClipper  @0x82BAE3C8
                                      -- both in src/vendor/renderware/collision/AALineClipper.cpp

The fat cylinder's torus arm (CylinderVolume::FatLineSegIntersect @0x82BAEB10, `bl` @0x82BAF2D8) clips its segment
with one, and KdTreeLineQuery @0x828AEE80 calls Init. On the PC it was a "semantic" reconstruction with its .rdata
words GUESSED: flt_820F2708, the scale of the segment's delta, was taken as 1e-6 -- the image holds 0x3F000000 (0.5)
-- and the reciprocal was an exact divide with an invented zero guard where the console refines vrefp twice; the w
lane's sign was +-1 where the console stores an integer 0; std::fmax dropped a NaN where vmaxfp keeps it.

Numeric: tests/L2AALineClipper.cpp compiles the revision's AALineClipper.cpp (the revision's headers shadowed in) and
replays tests/L2AALineClipperData.h, generated from the console's words on emu64 by
scratch/OWNERLIST_0927/L2/emu/gen_aalineclipper.py (seed 9281): the fat cylinder's torus boxes, 600 random segments
at six scales (zero-length lanes, +-0 lanes, tiny segments where the pad floor wins), and every fixed base with one
lane of start / end / seed / box min / box max set to +-NaN / +-inf -- all 16 words of the object bit for bit (a NaN
by class).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l2_aalineclipper.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import Tree, compile_and_run, report

C = "src/vendor/renderware/collision/"
AALC_CPP = C + "AALineClipper.cpp"
HEADERS = [C + "AALineClipper.hpp", C + "LineSegKernelMath.hpp", C + "FeatureEdge.hpp"]
NUMERIC_CHECKS = 3   # I, C, S


def _read(tree, path):
    try:
        return tree.read(path)
    except (OSError, ValueError):
        return ""


def numeric(tree):
    source = _read(tree, AALC_CPP)
    if not source:
        print("NUMERIC: cannot build -- " + AALC_CPP + " is absent")
        return None
    shadow = {}
    for path in HEADERS:
        text = _read(tree, path)
        if text:
            shadow[path] = text
    with tempfile.TemporaryDirectory(prefix="brn_l2_aalc_") as directory:
        aalc = Path(directory) / "AALineClipper.cpp"
        aalc.write_text(source, encoding="utf-8")
        return compile_and_run(Path(__file__).with_name("L2AALineClipper.cpp"), "l2_aalc_unused.inc",
                               "// not included: the clipper comes from AALineClipper.cpp\n",
                               "L2AALineClipper", shadow=shadow, extra_sources=(aalc,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_l2_aalineclipper", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
