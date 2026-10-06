"""ARTIST 0x82245084 uses E_WORLD for gyro-cam AttachmentTruck motion."""
from pathlib import Path
import argparse
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, extract, compile_and_run, report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    args = parser.parse_args()
    tree = Tree(args.rev)
    cpp = "src/GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.cpp"
    source = tree.read(cpp)
    methods, missing = extract(tree, cpp, ("void AttachmentTruck::Set(", "Vector3 AttachmentTruck::GetVelocity(", "void AttachmentTruck::Update("))
    try:
        call = definition(source, "if (mpParameters->mbUseTruck || mbIsPlanted)")
    except ValueError:
        call = ""
    if missing or not call:
        print("Production truck path missing:", missing)
        numeric = None
    else:
        numeric = compile_and_run(Path(__file__).with_name("PlaytestCrashTruckClock.cpp"),
            "playtest_crash_truck_methods.inc", "\n".join(methods), "PlaytestCrashTruckClock",
            extra_files={"playtest_crash_truck_call.inc": call.replace("VecFloat(", "BrnDirector::VecFloat(")})
    return report("run_playtest_crash_truck_clock", [], numeric, 5)

if __name__ == "__main__":
    sys.exit(main())
