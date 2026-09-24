"""FX-CRASHSND item 1 (crash parity 2026-09-24): the reset-on-track sting flood.

The reset-on-track FX sting (FxMessage type 5) fired on EVERY gameplay-camera frame -- ~26
audible posts per car placement, bursts of 4 that kept all four FxEffect voices busy and
silently dropped every other FX sting (the crash-in-water one among them). The consumer,
CameraControl::UpdateParams @0x826F6540 (0x826F69F0..0x826F6ABC), is faithful: it posts on
HasChanged(RACING_GAMEPLAY_CAMERA) && IsFlagSet(RACING_GAMEPLAY_CAMERA) &&
!HasChanged(IS_PICTURE_PARADISE). The edge was stale upstream: MainDirector::Update
@0x82274070 rolls the published camera's previous flag set before carrying it over,

  0x82275074  ori r10, r10, 0x3050 ; this + 0x33050 == mLastCamera.mState.mFlags
  0x82275088  ldx r11, r30, r10
  0x8227508C  std r11, var_3C8     ; lCamera.mState.mPreviousFlags
  0x82275090  bl  Camera::operator=(this + 0x32F10, &lCamera)

(DWARF CameraState::CopyFlagsToPrevious, BrnCameraState.h:144, inlined), and the PC Update did not.

Wiring: the roll sits in MainDirector::Update between the slomo gate and `mLastCamera = lCamera;`,
CopyFlagsToPrevious copies the other state's CURRENT set into this PREVIOUS set, and the consumer
condition is the console's. Numeric: tests/FxCrashSndCameraRoll.cpp replays the director's frame
pipeline through the PRODUCTION tail, CameraState bodies and CameraControl conditions.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashsnd_camera_roll.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report

DIRECTOR_CPP = "src/GameSource/Director/BrnMainDirector.cpp"
STATE_CPP = "src/GameSource/Director/Camera/BrnCameraState.cpp"
STATE_H = "src/GameSource/Director/Camera/BrnCameraState.h"
CONTROL_CPP = "src/GameSource/Sound/Global/BrnCameraControl.cpp"

UPDATE = "void MainDirector::Update(const DirectorInputOutput* lpIO)"
UPDATE_PARAMS = "void CameraControl::UpdateParams("
STATE_BODIES = [
    "void CameraState::SetFlag(u32 luIndex, bool lbValue)",
    "void CameraState::ClearFlag(u32 luIndex)",
    "bool CameraState::HasChanged(u32 luIndex) const",
]
NUMERIC_CHECKS = 27

# The consumer conditions compiled into the fixture: (function, marker that follows the `if`).
CONSUMERS = [
    ("ResetOnTrackEdge", "Message<FxMessage_ResetOnTrack>"),
    ("CameraPhotoEdge", "Message<FxMessage_CameraPhoto>"),
    ("CrashSnapshotEdge", "E_SNAPSHOT_TYPE_CRASH,"),
]


def director_tail(update_code):
    """MainDirector::Update's code from the end of the slomo gate through `mLastCamera = lCamera;`."""
    match = re.search(r"mfSimTimeScale\s*=\s*1\.0f\s*;\s*\}(.*?mLastCamera\s*=\s*lCamera\s*;)", update_code, re.S)
    return match.group(1) if match else ""


def condition_before(code, marker):
    """The parenthesised condition of the last `if` before `marker` (comments already stripped)."""
    at = code.index(marker)
    start = code.rfind("if (", 0, at)
    if start < 0:
        raise ValueError("no if before " + marker)
    opening = code.index("(", start)
    depth = 0
    for index in range(opening, len(code)):
        if code[index] == "(":
            depth += 1
        elif code[index] == ")":
            depth -= 1
            if depth == 0:
                return code[opening + 1:index]
    raise ValueError("unbalanced condition before " + marker)


def normalise(text):
    return re.sub(r"\s+", "", text)


def wiring(tree):
    update = body_or_empty(tree.read(DIRECTOR_CPP), UPDATE)
    tail = director_tail(update)
    roll = re.search(r"lCamera\s*\.\s*GetState\(\)\s*\.\s*CopyFlagsToPrevious\(\s*mLastCamera\s*\.\s*GetState\(\)\s*\)\s*;",
                     tail)
    state_h = code_only(tree.read(STATE_H))
    copy = re.search(r"void\s+CopyFlagsToPrevious\s*\(\s*const\s+CameraState\s*&\s*(\w+)\s*\)\s*\{\s*"
                     r"mPreviousFlags\s*=\s*(\w+)\s*\.\s*mCurrentFlags\s*;\s*\}", state_h)
    control = body_or_empty(tree.read(CONTROL_CPP), UPDATE_PARAMS)
    try:
        reset = normalise(condition_before(control, "Message<FxMessage_ResetOnTrack>"))
    except ValueError:
        reset = ""
    return [
        ("MainDirector::Update rolls lCamera's previous set from mLastCamera BEFORE `mLastCamera = lCamera;` "
         "(0x82275088/0x8227508C ahead of 0x82275090)",
         roll is not None and roll.end() <= tail.rfind("mLastCamera")),
        ("CameraState::CopyFlagsToPrevious copies the other state's CURRENT set into this PREVIOUS set "
         "(DWARF BrnCameraState.h:144)",
         copy is not None and copy.group(1) == copy.group(2)),
        ("CameraControl::UpdateParams posts reset-on-track on HasChanged(3) && IsFlagSet(3) && !HasChanged(14) "
         "(0x826F6A28 / 0x826F6A44 / 0x826F6A7C)",
         reset == "lrCameraState.HasChanged(CameraState::E_FLAG_RACING_GAMEPLAY_CAMERA)&&"
                  "lrCameraState.IsFlagSet(CameraState::E_FLAG_RACING_GAMEPLAY_CAMERA)&&"
                  "!lrCameraState.HasChanged(CameraState::E_FLAG_IS_PICTURE_PARADISE)"),
    ]


def numeric(tree):
    state_cpp = tree.read(STATE_CPP)
    try:
        bodies = [definition(state_cpp, signature) for signature in STATE_BODIES]
        update = code_only(definition(tree.read(DIRECTOR_CPP), UPDATE))
        control = code_only(definition(tree.read(CONTROL_CPP), UPDATE_PARAMS))
        consumers = []
        for name, marker in CONSUMERS:
            consumers.append("static bool %s(const CameraState& lrCameraState)\n{\n    return (%s);\n}\n"
                             % (name, condition_before(control, marker)))
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    tail = director_tail(update)
    if not tail:
        print("NUMERIC: cannot build -- MainDirector::Update has no slomo-gate -> carry-over tail")
        return None
    return compile_and_run(Path(__file__).with_name("FxCrashSndCameraRoll.cpp"), "fxcrashsnd_camerastate.inc",
                           "\n".join(bodies), "FxCrashSndCameraRoll",
                           shadow={STATE_H: tree.read(STATE_H)},
                           extra_files={"fxcrashsnd_director_tail.inc": tail,
                                        "fxcrashsnd_consumers.inc": "\n".join(consumers)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxcrashsnd_camera_roll", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
