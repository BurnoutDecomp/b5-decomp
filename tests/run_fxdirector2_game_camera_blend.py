"""FX-DIRECTOR2 (crash parity 2026-09-25, CC-14): MainDirector applies the ICE take's game-camera blend.

  MainDirector::Update @0x82274070 eases the frame camera toward the shared chase camera whenever the ICE take keys a
  CAMERA_BLEND_AMOUNT (0x822749D4..0x82274A24). The call sits between the camera validation and the effect-hook hand-over:
  CameraInterpolationController::Update(this + 0x121B0, &lCamera, the gameplay-external camera, the car's transform).
  Otherwise it resets both interpolaters. The PC dropped the call inside the Update's broad GATE and kept the controller
  as a raw u8[0x40] span, which Construct never reset (0x8225B810..0x8225B840). Every blended ICE take hard-cut in and
  out: "Takendown", Crash_Spring*, Takedown_ICE_*, Event_Win*, and the race / stunt / Road Rage starts (53 of the 549
  retail takes key a blend).

Wiring: the typed member; Construct's reset between GameState::Clear and the DebugLog; Update's statement after the
arbitrator and before the hook hand-over. Numeric: tests/FxDirector2GameCameraBlend.cpp compiles the PRODUCTION
statement (lifted out of Update) in a fixture with the director's member names -- 7 checks.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector2_game_camera_blend.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, compile_and_run, report

DIRECTOR_CPP = "src/GameSource/Director/BrnMainDirector.cpp"
DIRECTOR_H = "src/GameSource/Director/BrnMainDirector.h"
# The race-car array holds VehicleInfo by value, whose RaceCarState constructor calls Clear().
SOURCES = ("src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp",)
HEADERS = (
    DIRECTOR_H,
    "src/GameSource/Director/Shots/ShotControllers/BrnCameraInterpolationController.h",
    "src/GameSource/Director/Camera/BrnSharedCameraContainer.h",
    "src/GameSource/Director/Camera/Camera.h",
    "src/GameSource/Director/Camera/BrnCameraEffects.h",
    "src/GameSource/Director/Camera/Utils/BrnInterpolater.h",
    "src/GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h",
    "src/GameSource/Director/Camera/BrnBehaviourManager.h",
)
BLEND_IF = "if (lCamera.GetEffects().mfGameCameraBlend"
NUMERIC_CHECKS = 7


def blend_statement(update):
    """The production `if (...) { ... } else { ... }` blend statement, or None."""
    if BLEND_IF not in update:
        return None
    head = definition(update, BLEND_IF)
    rest = update[update.index(BLEND_IF) + len(head):]
    match = re.match(r"\s*else\s*", rest)
    if not match:
        return None
    return head + "\n" + definition(rest, "else")


def wiring(tree):
    header = code_only(tree.read(DIRECTOR_H))
    yield ("MainDirector holds a typed CameraInterpolationController at +0x121B0 (was a u8[0x40] span)",
           re.search(r"\bCameraInterpolationController\s+mCameraInterpolationController\s*;", header) is not None)
    source = tree.read(DIRECTOR_CPP)
    try:
        construct = re.sub(r"\s+", "", code_only(definition(source, "void MainDirector::Construct(")))
    except ValueError:
        construct = ""
    clear = construct.find("maGameState.Clear();")
    reset = construct.find("mCameraInterpolationController.Construct();")
    log = construct.find("mDebugLog.Construct();")
    yield ("Construct resets the controller (0x8225B810..0x8225B840) after GameState::Clear, before the DebugLog",
           -1 < clear < reset < log)
    try:
        update = code_only(definition(source, "void MainDirector::Update("))
    except ValueError:
        update = ""
    arbitrator = update.find("UpdateArbitrator(lpIO, lCamera, liPlayerCarIndex);")
    blend = update.find(BLEND_IF)
    handover = update.find("RegisterStartingEffectWithName")
    yield ("Update runs the blend statement (0x822749D4) after UpdateArbitrator and before the hook hand-over",
           -1 < arbitrator < blend < handover)


def numeric(tree):
    source = tree.read(DIRECTOR_CPP)
    try:
        update = code_only(definition(source, "void MainDirector::Update("))
    except ValueError:
        update = ""
    statement = blend_statement(update)
    constant = re.search(r"const\s+f32\s+KF_GAME_CAMERA_BLEND_OFF\s*=\s*[^;]+;", code_only(source))
    if statement is None or constant is None:
        print("NUMERIC: the revision's MainDirector::Update has no game-camera blend statement")
        return None
    shadow = {relative: tree.read(relative) for relative in HEADERS} if tree.rev is not None else None
    with tempfile.TemporaryDirectory(prefix="brn_fxd2_blend_") as directory:
        extra = []
        for relative in SOURCES:
            target = Path(directory) / Path(relative).name
            target.write_text(tree.read(relative), encoding="utf-8")
            extra.append(target)
        return compile_and_run(Path(__file__).with_name("FxDirector2GameCameraBlend.cpp"), "fxd2_blend_block.inc",
                               "// GENERATED by run_fxdirector2_game_camera_blend.py\n" + statement + "\n",
                               "FxDirector2GameCameraBlend", shadow=shadow,
                               extra_files={"fxd2_blend_const.inc": constant.group(0) + "\n"},
                               extra_sources=tuple(extra))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector2_game_camera_blend", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
