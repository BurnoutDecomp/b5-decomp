"""OWNERLIST 2026-09-27, lane L5 MENUS: the crash-nav fly-by (BehaviourRoadRunner) -- the pause camera after a crash.

ArbStateCrashNav::Update @0x8226DC98 hands the pause frame to the road-runner fly-by while the player car's
mbStartedDeforming is set. Two defects made that pause an assert storm on a camera near the world origin:
  1. BehaviourRoadRunner::Update @0x82247E98 seats its lane truck on the lane nearest shared-info +0x280
     (0x82247F08..0x82247F14, `lvx128 v1, info, 0x280`) == mPlayerInfo.mRaceCarState.mTransform.wAxis, the player
     car's position. The PC passed the ORIGIN under a stale "mPlayerInfo is not mapped" FLAG.
  2. TrafficLaneTruck::MoveAlongTrafficLaneBackwards @0x8222B100 dropped the three 0.0001 guards (flt_82002540):
     the parameter landed ON the rung, every later step consumed zero distance, and both "Current rung got out of
     sync" asserts fired every step -- 2.05 million in one pause.

Numeric: tests/MenusRoadRunnerBackwards.cpp compiles the revision's backward walk and split picker over a synthetic
two-section lane. The expected parameters come from a float32 model of the console body (each op rounded once):
    A 12 m inside a section -> section 0 rung 1 param 0x3FA65FD8   B across the split -> section 1 rung 3 0x406CC986
    C 35 m, four segments   -> section 1 rung 3 param 0x406FF2E5   D past the dead end -> invalid, no assert
Wiring: Update's truck seed is the player car's position, not a literal, and Update raises E_FLAG_VALID on its camera
(0x82247F54..0x82247F64, `ori r11, r11, 2` into the current flag set) on every frame past the seat.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_menus_roadrunner.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report

ROADRUNNER_CPP = "src/GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.cpp"
NUMERIC_CHECKS = 14


def anonymous_block(source):
    """The anonymous namespace after PickSplitToTakeBackwards: the walkers' constants and EndOfLane."""
    marker = source.find("const f32 KF_SEGMENT_END_PARAM")
    if marker < 0:
        return None
    start = source.rfind("namespace", 0, marker)
    return definition(source, source[start:source.index("{", start) + 1])


def numeric(tree):
    source = tree.read(ROADRUNNER_CPP)
    try:
        picker = definition(source, "void TrafficLaneTruck::PickSplitToTakeBackwards(")
        walker = definition(source, "void TrafficLaneTruck::MoveAlongTrafficLaneBackwards(")
    except ValueError:
        print("NUMERIC: the backward walk is not in this revision")
        return None
    block = anonymous_block(source)
    if block is None:
        print("NUMERIC: the walkers' constant block is not in this revision")
        return None
    inc = block + "\n" + picker + "\n" + walker + "\n"
    return compile_and_run(Path(__file__).with_name("MenusRoadRunnerBackwards.cpp"), "menus_roadrunner.inc", inc,
                           "MenusRoadRunnerBackwards")


def wiring(tree):
    update = body_or_empty(tree.read(ROADRUNNER_CPP), "bool BehaviourRoadRunner::Update(")
    seed = re.search(r"lSeedPoint\s*=\s*([^;]+);", update)
    seeded_from_player = seed is not None and re.search(
        r"mPlayerInfo\s*\.\s*mRaceCarState\s*\.\s*mTransform\s*\.\s*wAxis", seed.group(1)) is not None
    return [("BehaviourRoadRunner::Update seats the truck at the player car's position (info +0x280, 0x82247F14)",
             seeded_from_player),
            ("BehaviourRoadRunner::Update raises E_FLAG_VALID on its camera past the seat (0x82247F54 `ori r11, r11, 2`)",
             re.search(r"SetBit\(\s*CameraState::E_FLAG_VALID\s*\)", update) is not None)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_menus_roadrunner", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
