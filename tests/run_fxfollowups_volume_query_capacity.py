"""FX-FOLLOWUPS (crash parity 2026-09-25): the HOST capacity of VolumeVolumeQuery's report region.

GetPrimitiveBBoxOverlaps @0x82BB3AB0 meters the 1xN staging in console words (8 bytes per result, 12 per group
header, 4 per pair) and Construct @0x82BB38F0 carves 8 * results for it; GetResourceDescriptor @0x82BB3A20's
`2072 * a3 + 192` leaves 192 * (results + 1) for the GPInstance scratch PrimitiveBatchIntersect @0x82BABC78 fills.
The host 1xN group is 16 + 8n bytes and the host GPInstance 0xC8, so the same staging decisions overran both
regions: ten 10-pair groups spill 160 bytes into the result array (which the narrow phase then writes over while
groups are still unread), and one 100-pair group writes 808 bytes of instances past the end of the descriptor.

tests/FxFollowupsVolumeQueryCapacity.cpp is compiled against the revision's VolumeQuery.cpp and VolumeBBoxQuery.cpp
(separate TUs, the revision's headers shadowed in), the working tree's VolRef.cpp, and the revision's PRODUCTION
PrimitiveBatchIntersect (PrimitiveIntersect.cpp) pasted in; the volume descriptors, the aggregate and the two
narrow-phase kernels are fixtures. It checks the descriptor total, the staged bytes against the staging region,
every GPInstance against the descriptor end, a guard after the descriptor, and that every group reaches the
narrow phase intact.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_volume_query_capacity.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, definition, compile_and_run, report

COLLISION = "src/vendor/renderware/collision/"
VQ_CPP = COLLISION + "VolumeQuery.cpp"
VBQ_CPP = COLLISION + "VolumeBBoxQuery.cpp"
PI_CPP = COLLISION + "PrimitiveIntersect.cpp"
HEADERS = (COLLISION + "VolumeQuery.hpp", COLLISION + "VolumeBBoxQuery.hpp", COLLISION + "VolumeQueryHostLayout.hpp",
           COLLISION + "GPInstance.hpp")
VOLREF_CPP = REPO / (COLLISION + "VolRef.cpp")
NUMERIC_CHECKS = 12


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    vq, vbq, pi = tree.read(VQ_CPP), tree.read(VBQ_CPP), tree.read(PI_CPP)
    try:
        if not vq or not vbq or not pi:
            raise ValueError("a production TU is absent at this revision")
        driver = "\n".join([
            definition(pi, "namespace\n{\n    // Collision-volume dispatch table, reached through"),
            definition(pi, "s32 PrimitiveBatchIntersect(PrimitivePairIntersectResult* lapResults,"),
        ])
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return report("run_fxfollowups_volume_query_capacity", [], None, NUMERIC_CHECKS)
    shadow = {path: text for path, text in ((path, tree.read(path)) for path in HEADERS) if text}
    with tempfile.TemporaryDirectory(prefix="brn_fxfu_vqcap_") as directory:
        sources = Path(directory)
        (sources / "VolumeQuery.cpp").write_text(vq, encoding="utf-8")
        (sources / "VolumeBBoxQuery.cpp").write_text(vbq, encoding="utf-8")
        numeric = compile_and_run(Path(__file__).with_name("FxFollowupsVolumeQueryCapacity.cpp"),
                                  "fxfu_vqcap.inc", driver, "FxFollowupsVolumeQueryCapacity", shadow=shadow,
                                  extra_sources=(sources / "VolumeQuery.cpp", sources / "VolumeBBoxQuery.cpp",
                                                 VOLREF_CPP))
    return report("run_fxfollowups_volume_query_capacity", [], numeric, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
