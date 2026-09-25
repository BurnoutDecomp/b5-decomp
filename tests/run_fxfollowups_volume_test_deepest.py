"""FX-FOLLOWUPS (crash parity 2026-09-25, item 2 step E1): the fine module's deepest-volume test.

  FineIntersectionTestModule::ComputeVolumeTestDeepest @0x828C90D0   a LOUD trap before
  VolumeManager::GetVolumeTypeFlags (DWARF CgsVolumeManager.h:138)   no body before (its inline copy at
      0x828C9250..0x828C92C4 attests the h:203 / h:204 asserts and the mxFlags read)

tests/FxFollowupsVolumeTestDeepest.cpp compiles the revision's two bodies against fakes of their collaborators and
checks the priming of the VolumeVolumeQuery, the deepest pick (-distance > deepest, numPoints != 0, NaN never wins,
carried across instances and entities), the depth left alone on a miss, the exclude rule (a bit-superset test against
the masked exclude id; parts mask 0xFFFFFC00; 0xFFFFFFFF without an exclude), the volume-type gate, the CONST chain
walk, and GetVolumeTypeFlags' two asserts. A revision without GetVolumeTypeFlags gets a stand-in that answers 0.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_volume_test_deepest.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

SCENE = "src/GameShared/GameClasses/SceneManager/"
FINE_SQ1 = SCENE + "FineIntersectionTestModule/CgsFineIntersectionTestModule_wSQ1.cpp"
VOLUME_MANAGER = SCENE + "CgsVolumeManager.cpp"
NUMERIC_CHECKS = 12

FLAGS_STANDIN = ("VolumeManagerVolume::VolumeTypeFlags VolumeManager::GetVolumeTypeFlags(s32) const { return 0; }"
                 "  // stand-in: no body at this revision\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    try:
        body = definition(tree.read(FINE_SQ1), "void FineIntersectionTestModule::ComputeVolumeTestDeepest(")
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return report("run_fxfollowups_volume_test_deepest", [], None, NUMERIC_CHECKS)
    try:
        flags = definition(tree.read(VOLUME_MANAGER),
                           "VolumeManagerVolume::VolumeTypeFlags VolumeManager::GetVolumeTypeFlags(")
        print("found   VolumeManager::GetVolumeTypeFlags")
    except ValueError:
        print("MISSING VolumeManager::GetVolumeTypeFlags -- stand-in answers 0")
        flags = FLAGS_STANDIN
    numeric = compile_and_run(Path(__file__).with_name("FxFollowupsVolumeTestDeepest.cpp"), "fxfu_cvtd_body.inc",
                              body, "FxFollowupsVolumeTestDeepest", extra_files={"fxfu_cvtd_flags.inc": flags})
    return report("run_fxfollowups_volume_test_deepest", [], numeric, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
