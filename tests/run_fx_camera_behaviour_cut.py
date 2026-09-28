"""Camera-cut parity: MainDirector::Update, ARTIST 0x822745D0..0x82274628.

Compile the production behaviour-transition block. --rev dd3d0174 reproduces
the missing cut that let the first race-countdown camera blend toward origin.
"""
import argparse
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, compile_and_run, report

SOURCE = "src/GameSource/Director/BrnMainDirector.cpp"
START = "static const Camera::Behaviour* spPreviousCameraBehaviour"
END = "spPreviousCameraBehaviour = lCamera.mpDebugInfoBehaviour;"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    args = parser.parse_args()
    update = code_only(definition(Tree(args.rev).read(SOURCE), "void MainDirector::Update("))
    begin = update.find(START)
    end = update.find(END, max(0, begin))
    block = update[begin:end + len(END)] if begin >= 0 and end >= 0 else ""
    wiring = [("behaviour transition is after arbitration and before camera blend/finaliser",
               -1 < update.find("UpdateArbitrator(lpIO, lCamera, liPlayerCarIndex);") < begin
               < update.find("mCameraInterpolationController.Update(")
               < update.find("mCameraFinaliser.Update("))]
    numeric = compile_and_run(Path(__file__).with_name("FxCameraBehaviourCut.cpp"),
                              "fx_camera_behaviour_cut.inc", block,
                              "FxCameraBehaviourCut")
    return report("run_fx_camera_behaviour_cut", wiring, numeric, 12)


if __name__ == "__main__":
    sys.exit(main())
