"""FX-DIRECTOR (crash parity 2026-09-24): diag hygiene -- every PC-only witness print on the camera path is
default-off AND bounded.

REVIEW-F item 8 and the conductor's queue item (3):
  * `[camrig]` (BehaviourAftertouchCrash::Update, BRN_CAMRIG_DIAG) was rate-limited (the first 10 frames after each
    Prepare, then every 15th) but never capped;
  * `[cam-impact]` and `[cam-aabb]` (BridgeWorldToDirector, BRN_CAM_INPUT_DIAG) were rate-limited (40 then every
    200th / 12 then every 400th) but never capped;
  * `[world->director] publish #...` (BridgeWorldToDirector) printed with NO env gate at all (the first publish,
    then every 3000th, for ever) -- an ungated periodic print is not console behaviour;
  * MomentController::NewMoment's `[jump-ladder] ... allocated type=` line printed whenever a log existed.
None of these is X360 code, so there is no asm to run: the checks are structural, on comment-stripped source, and
each one fails on the pre-fix body. (The live cases that read these lines arm the switches in their DiagEnv.)

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector_diag_hygiene.py [--rev <b5 rev>]
"""
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, report

AFTERTOUCH_CPP = "src/GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.cpp"
MOMENTS_CPP = "src/GameSource/Director/MomentController/BrnMomentController.cpp"
BRIDGE_CPP = "src/GameSource/Game/GameBridgeWorldToX.cpp"


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def block_before(source, marker, span=1400):
    """The comment-stripped, whitespace-squashed code in the `span` characters before the print marker."""
    at = source.find(marker)
    if at < 0:
        return None
    return squash(source[max(0, at - span):at])


def wiring(tree):
    aftertouch = tree.read(AFTERTOUCH_CPP)
    camrig = block_before(aftertouch, '<< "[camrig] Update frame "')
    yield ("[camrig] is env-gated (BRN_CAMRIG_DIAG) AND hard-capped (a line budget checked and spent per line)",
           camrig is not None and 'getenv("BRN_CAMRIG_DIAG")' in camrig
           and re.search(r"staticconsts32|statics32", camrig) is not None
           and re.search(r"(\w+LinesLeft)>0.*--\1;", camrig) is not None)

    moments = tree.read(MOMENTS_CPP)
    new_moment = block_before(moments, '<< "[FLAG PC bring-up] [jump-ladder] MomentController::NewMoment allocated type="', 900)
    yield ("NewMoment's [jump-ladder] allocation line is behind a default-off env switch (BRN_CRASHCAM_DIAG)",
           new_moment is not None and 'getenv("BRN_CRASHCAM_DIAG")' in new_moment
           and re.search(r"if\(sbMomentDiag&&", new_moment) is not None)

    bridge = tree.read(BRIDGE_CPP)
    impact = block_before(bridge, '<< "[cam-impact] #"')
    yield ("[cam-impact] keeps its gate and rate limit and is hard-capped",
           impact is not None and 'getenv("BRN_CAM_INPUT_DIAG")' in impact
           and re.search(r"(\w+LinesLeft)>0&&\(suNonZeroImpacts<=40u.*--\1;", impact) is not None)
    aabb = block_before(bridge, '<< "[cam-aabb] #"', 1800)
    yield ("[cam-aabb] keeps its gate and rate limit and is hard-capped",
           aabb is not None and 'getenv("BRN_CAM_INPUT_DIAG")' in aabb
           and re.search(r"(\w+LinesLeft)>0&&\(suDeformedBoxes<=12u.*--\1;", aabb) is not None)
    publish = block_before(bridge, '<< "[world->director] publish #"', 900)
    yield ("[world->director] publish is gated behind BRN_CAM_INPUT_DIAG (it printed ungated) and capped",
           publish is not None and 'getenv("BRN_CAM_INPUT_DIAG")' in publish
           and re.search(r"if\(sbPublishDiag&&(\w+LinesLeft)>0&&.*--\1;", publish) is not None)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector_diag_hygiene", list(wiring(tree)), (0, 0), 0)


if __name__ == "__main__":
    sys.exit(main())
