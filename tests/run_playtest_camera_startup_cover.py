"""The ARTIST car-select readiness latch must release E_FLAG_VALID startup masking."""
from pathlib import Path
import argparse
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    args = parser.parse_args()
    tree = Tree(args.rev)
    source = tree.read("src/GameSource/Director/Arbitrator/BrnDirectorArbitrator.cpp")
    try:
        latch = definition(source, "if (mStateContainer.GetCurrentState() ==")
        cover = definition(source, "if (mbStartOfGame)")
    except ValueError:
        latch = cover = ""
    numeric = None if not latch else compile_and_run(Path(__file__).with_name("PlaytestCameraStartupCover.cpp"),
        "playtest_camera_startup_latch.inc", latch, "PlaytestCameraStartupCover",
        extra_files={"playtest_camera_startup_cover.inc": cover})
    return report("run_playtest_camera_startup_cover", [], numeric, 4)

if __name__ == "__main__":
    sys.exit(main())
