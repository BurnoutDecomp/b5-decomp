"""FX-AIMOD G07-D1: replay the production ResetOnTrackManager::TestCarHNG geometry.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aimod_car_hng.py [--rev <b5 rev>]
"""
import sys
sys.dont_write_bytecode = True
from aimod_common import Tree, parse_args, compile_and_run, definition, REPO

AVOID = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_AvoidObstacles.cpp"


def main():
    source = Tree(parse_args().rev).read(AVOID)
    chunks = ["namespace BrnAI {", definition(source, "bool ResetOnTrackManager::TestCarHNG("), "}"]
    sys.exit(compile_and_run("AIModCarHNG.cpp", chunks,
                             [REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"],
                             prefix="brn_aimod_carhng_"))


if __name__ == "__main__":
    main()
