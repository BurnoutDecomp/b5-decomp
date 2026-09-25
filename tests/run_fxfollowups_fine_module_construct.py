"""FX-FOLLOWUPS (crash parity 2026-09-25, item 2 step D, commit 1): the fine intersection-test module's construction.

  rw::collision::VolumeLineQuery::GetResourceDescriptor @0x82BB3838 / Initialize @0x82BB3888  no host bodies before
  CgsSceneManager::FineIntersectionTestModule::Construct @0x828B0BF0 (the unmounted TU)  the VVQ buffer at the
      host size (0x49600; console 0x49000) and the VLQ descriptor into a whole block (it was one u32)

tests/FxFollowupsFineModuleConstruct.cpp compiles the revision's CgsFineIntersectionTestModule.cpp, VolumeQuery.cpp and
VolumeBBoxQuery.cpp (their headers shadowed from the revision) and the working tree's VolRef.cpp, with the production
PrimitiveBatchIntersect pasted in so VolumeQuery.cpp links. It checks the VLQ descriptor and carves, then runs the
module's Construct and checks its stages, its two in-place queries, that each query's carve lies inside its buffer,
and the buffer contiguity. A revision without the VLQ bodies cannot link: every numeric check then counts as failed.

Commit 2 (the mount) adds Prepare @0x828AA630 / Release @0x828AA730 from every stage (P1-P4, R1-R4, C1). Release
from START must run START -> MANAGER -> DONE in one call (the console's fall-through into the MANAGER arm at
0x828AA79C); the body before the mount stopped at MANAGER, so R1 and C1 fail on b5 26802ef3.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_fine_module_construct.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, definition, compile_and_run, report

COLLISION = "src/vendor/renderware/collision/"
FINE = "src/GameShared/GameClasses/SceneManager/FineIntersectionTestModule/"
VQ_CPP = COLLISION + "VolumeQuery.cpp"
VBQ_CPP = COLLISION + "VolumeBBoxQuery.cpp"
PI_CPP = COLLISION + "PrimitiveIntersect.cpp"
FINE_CPP = FINE + "CgsFineIntersectionTestModule.cpp"
HEADERS = (COLLISION + "VolumeQuery.hpp", COLLISION + "VolumeBBoxQuery.hpp", COLLISION + "VolumeQueryHostLayout.hpp",
           COLLISION + "GPInstance.hpp", FINE + "CgsFineIntersectionTestModule.h")
VOLREF_CPP = REPO / (COLLISION + "VolRef.cpp")
# The rwcollision side of the fixture (its own TU: the vendor vpu vocabulary must not meet the scene manager one).
VENDOR_FIXTURE = REPO / "tests" / "FxFollowupsFineModuleConstructVendor.cpp"
NUMERIC_CHECKS = 21


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    vq, vbq, pi, fine = tree.read(VQ_CPP), tree.read(VBQ_CPP), tree.read(PI_CPP), tree.read(FINE_CPP)
    try:
        if not vq or not vbq or not pi or not fine:
            raise ValueError("a production TU is absent at this revision")
        driver = "\n".join([
            definition(pi, "namespace\n{\n    // Collision-volume dispatch table, reached through"),
            definition(pi, "s32 PrimitiveBatchIntersect(PrimitivePairIntersectResult* lapResults,"),
        ])
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return report("run_fxfollowups_fine_module_construct", [], None, NUMERIC_CHECKS)
    shadow = {path: text for path, text in ((path, tree.read(path)) for path in HEADERS) if text}
    with tempfile.TemporaryDirectory(prefix="brn_fxfu_fine_") as directory:
        sources = Path(directory)
        for name, text in (("VolumeQuery.cpp", vq), ("VolumeBBoxQuery.cpp", vbq),
                           ("CgsFineIntersectionTestModule.cpp", fine)):
            (sources / name).write_text(text, encoding="utf-8")
        numeric = compile_and_run(Path(__file__).with_name("FxFollowupsFineModuleConstruct.cpp"), "fxfu_fine.inc",
                                  driver, "FxFollowupsFineModuleConstruct", shadow=shadow,
                                  extra_sources=(sources / "VolumeQuery.cpp", sources / "VolumeBBoxQuery.cpp",
                                                 sources / "CgsFineIntersectionTestModule.cpp", VOLREF_CPP,
                                                 VENDOR_FIXTURE))
    return report("run_fxfollowups_fine_module_construct", [], numeric, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
