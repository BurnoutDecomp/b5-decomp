"""FX-DIRECTOR (crash parity 2026-09-24): BehaviourRoadRunner::Update raises E_FLAG_ROAD_FOLLOWING_CAM (flag 27).

  The console's fly-by camera ORs bit 27 into the camera's current flag set every frame it runs
  (0x8224889C `ld r11, 0x140(r30)` / 0x822488A0 `oris r11, r11, 0x800` / 0x822488A4 `std`). Only the HasFailed exit
  (0x82247EC8 -> 0x82248ECC) and the Fail(6) exit (0x8224814C -> 0x82248ECC) skip it. It is the image's only setter of
  flag 27; TrafficEntityModule::UpdateCollidableVehicles (0x82730CB0..0x82730CE0) adds the camera as a collidable /
  avoidance source while it is up (FX-TRAFFIC5 landed that consumer). The PC Update never set it.

Numeric: tests/FxDirectorRoadRunnerFlag.cpp runs the production statement on the REAL CameraState (a revision without
it runs an empty tail). Wiring: the store sits after the mode-time block and before the final return, and the two
early exits return before it.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector_roadrunner_flag.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, compile_and_run, report

ROADRUNNER_CPP = "src/GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.cpp"
UPDATE = "bool BehaviourRoadRunner::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)"
STATEMENT = re.compile(r"lrCamera\.GetState\(\)\.mCurrentFlags\.SetBit\(CameraState::E_FLAG_ROAD_FOLLOWING_CAM\);")
NUMERIC_CHECKS = 6


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def update_body(tree):
    try:
        return code_only(definition(tree.read(ROADRUNNER_CPP), UPDATE))
    except ValueError:
        return ""


def wiring(tree):
    body = squash(update_body(tree))
    store = body.find("lrCamera.GetState().mCurrentFlags.SetBit(CameraState::E_FLAG_ROAD_FOLLOWING_CAM);")
    mode_time = body.find("mfCurrentModeTime+=lfTimestep;")
    last_return = body.rfind("returntrue;")
    yield ("Update ORs E_FLAG_ROAD_FOLLOWING_CAM into the current set after the mode-time block and right before its "
           "final return (0x8224889C..0x822488A4, then 0x82248ECC)",
           0 <= mode_time < store < last_return and body.count("E_FLAG_ROAD_FOLLOWING_CAM") == 1)
    failed = body.find("if(HasFailed()){returntrue;")
    fail6 = body.find("Fail(lrCamera,6);returntrue;")
    yield ("the HasFailed exit (0x82247EC8) and the Fail(6) exit (0x8224814C) return before the store",
           0 <= failed < store and 0 <= fail6 < store)


def numeric(tree):
    match = STATEMENT.search(update_body(tree))
    if match is None:
        print("NUMERIC: this revision's Update never raises flag 27 -- empty tail")
        statement = "/* [this revision does not set E_FLAG_ROAD_FOLLOWING_CAM] */"
    else:
        statement = match.group(0)
    return compile_and_run(Path(__file__).with_name("FxDirectorRoadRunnerFlag.cpp"), "fxdirector_roadrunner_tail.inc",
                           statement + "\n", "FxDirectorRoadRunnerFlag")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector_roadrunner_flag", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
