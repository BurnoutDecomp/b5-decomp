"""The two asm-attested black-bar producer stores must target +A8, separately from +84."""
from pathlib import Path
import argparse
import re
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    args = parser.parse_args()
    tree = Tree(args.rev)
    key = code_only(tree.read("src/GameSource/Director/Shots/ShotControllers/BrnKeyAnimController.cpp"))
    stunt = code_only(tree.read("src/GameSource/Director/MomentController/Moments/BrnMomentPlayerStunt.cpp"))
    keyed = re.search(r"const f32 lfLetterbox\s*=.*?;\s*lrEffects\.mf\w+\s*=.*?;", key, re.S)
    stunted = re.search(r"GetNonConstCamera\(\)\.GetEffects\(\)\.mf\w+\s*=\s*KF_STUNT_RACE_END_EFFECT\s*;", stunt)
    constants = [re.search(r"(?:static )?const f32 KF_LETTERBOX_EFFECT_AMOUNT\s*=.*?;", key),
                 re.search(r"const f32 KF_STUNT_RACE_END_EFFECT\s*=.*?;", stunt)]
    if not keyed or not stunted or not all(constants):
        numeric = None
    else:
        numeric = compile_and_run(Path(__file__).with_name("PlaytestCameraBarsProducers.cpp"),
            "playtest_camera_bars_keyed.inc", keyed[0], "PlaytestCameraBarsProducers",
            extra_files={"playtest_camera_bars_stunt.inc": stunted[0],
                         "playtest_camera_bars_constants.inc": "\n".join(c[0] for c in constants)})
    return report("run_playtest_camera_bars_producers", [], numeric, 5)

if __name__ == "__main__":
    sys.exit(main())
