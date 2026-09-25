"""FX-FOLLOWUPS (crash parity 2026-09-25): the HOST layout of the in-place rw::collision query objects.

VolumeVolumeQuery::Construct @0x82BB38F0 carves its two VolumeBBoxQuery sub-queries at `this + 0x50` and
VolumeBBoxQuery::Initialize @0x82BBBD90 carves its stack VolRef records at `this + 0x100`: on the console those are
the objects' own sizes (0x48 / 0xF1 rounded to 16). The host objects are 0x88 / 0x110 bytes, so the port's console
constants put sub-query A on the parent's m_intersectionBuffer .. m_bBoxQueryBtoA (GetPrimitiveBBoxOverlaps then
wrote through the clobbered m_bBoxQueryBtoA) and an aggregate's stack record on the sub-query's own
m_curSpatialMapQuery / m_tag / m_numTagBits.

tests/FxFollowupsVolumeQueryLayout.cpp is compiled against the revision's VolumeQuery.cpp and VolumeBBoxQuery.cpp
(separate TUs) and the working tree's VolRef.cpp; it runs Initialize + GetPrimitiveBBoxOverlaps on an overlapping
sphere pair (the pass under SEH, so the pre-fix wild write is reported, not fatal) and an aggregate AddVolumeRef, and
checks the carve, the parent's fields and guard patterns around every carve.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_volume_query_layout.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, compile_and_run, report

VQ_CPP = "src/vendor/renderware/collision/VolumeQuery.cpp"
VBQ_CPP = "src/vendor/renderware/collision/VolumeBBoxQuery.cpp"
VOLREF_CPP = REPO / "src/vendor/renderware/collision/VolRef.cpp"
NUMERIC_CHECKS = 18


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read VolumeQuery.cpp / VolumeBBoxQuery.cpp from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    vq, vbq = tree.read(VQ_CPP), tree.read(VBQ_CPP)
    if not vq or not vbq:
        print("NUMERIC: cannot build -- a production TU is absent at this revision")
        return report("run_fxfollowups_volume_query_layout", [], None, NUMERIC_CHECKS)
    with tempfile.TemporaryDirectory(prefix="brn_fxfu_vq_") as directory:
        sources = Path(directory)
        (sources / "VolumeQuery.cpp").write_text(vq, encoding="utf-8")
        (sources / "VolumeBBoxQuery.cpp").write_text(vbq, encoding="utf-8")
        numeric = compile_and_run(Path(__file__).with_name("FxFollowupsVolumeQueryLayout.cpp"),
                                  "fxfu_vq_unused.inc", "", "FxFollowupsVolumeQueryLayout",
                                  extra_sources=(sources / "VolumeQuery.cpp", sources / "VolumeBBoxQuery.cpp",
                                                 VOLREF_CPP))
    return report("run_fxfollowups_volume_query_layout", [], numeric, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
