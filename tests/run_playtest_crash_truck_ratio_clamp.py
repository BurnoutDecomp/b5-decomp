"""The gyro-cam attachment truck clamps its first-frame speed ratio to the original [0, 1.25]."""
from pathlib import Path
import argparse
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, extract, compile_and_run, report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    args = parser.parse_args()
    tree = Tree(args.rev)
    cpp = "src/GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.cpp"
    methods, missing = extract(tree, cpp, ("void AttachmentTruck::Set(", "Vector3 AttachmentTruck::GetVelocity(",
                                           "void AttachmentTruck::Update("))
    if missing:
        print("Production truck path missing:", missing)
        numeric = None
    else:
        numeric = compile_and_run(Path(__file__).with_name("PlaytestCrashTruckRatioClamp.cpp"),
                                  "playtest_crash_truck_ratio_methods.inc", "\n".join(methods),
                                  "PlaytestCrashTruckRatioClamp")
    return report("run_playtest_crash_truck_ratio_clamp", [], numeric, 6)


if __name__ == "__main__":
    sys.exit(main())
