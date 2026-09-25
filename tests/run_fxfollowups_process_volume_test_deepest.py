"""FX-FOLLOWUPS (crash parity 2026-09-25, item 2 step E2): SceneManagerModule::ProcessVolumeTestDeepest @0x828D4460
(a CGS_ASSERT(false) trap before) -- the scene manager's deepest-volume query, the director camera's sphere.

tests/FxFollowupsProcessVolumeTestDeepest.cpp compiles the revision's body against fakes of its collaborators and
checks the world arm (the sphere it builds, the map and tags it passes, the {id, 0.0, 1} post and the return; the
non-sphere assert; a miss falling through), the entity arm (the VolumeTest arguments inside one batch, the empty
answer, the fine query it builds, the fine answer posted as it came back, the excluded-only candidate, exclusion mode
1, an unknown exclude id, attempted != written).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_process_volume_test_deepest.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

MODULE = "src/GameShared/GameClasses/SceneManager/CgsSceneManagerModule.cpp"
# The event header is shadowed from the same revision (REVIEW-K, 2026-09-25: it names its members since then).
EVENT_H = "src/GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestDeepest.h"
NUMERIC_CHECKS = 11


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    try:
        body = definition(tree.read(MODULE), "void SceneManagerModule::ProcessVolumeTestDeepest(")
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return report("run_fxfollowups_process_volume_test_deepest", [], None, NUMERIC_CHECKS)
    numeric = compile_and_run(Path(__file__).with_name("FxFollowupsProcessVolumeTestDeepest.cpp"), "fxfu_pvtd_body.inc",
                              body, "FxFollowupsProcessVolumeTestDeepest", shadow={EVENT_H: tree.read(EVENT_H)})
    return report("run_fxfollowups_process_volume_test_deepest", [], numeric, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
