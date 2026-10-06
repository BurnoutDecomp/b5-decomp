"""Actual AptCIH tick/seek dispatch preserves native frame0 child state on wrap."""
from pathlib import Path
import argparse
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    args = parser.parse_args()
    source = Tree(args.rev).read("src/SDKs/EATech/include/Apt/AptCIH.cpp")
    bodies = "\n".join(definition(source, name) for name in (
        "int AptCIH::jumpToFrame(", "int AptCIH::tick("))
    result = compile_and_run(Path(__file__).with_name("PlaytestAptTimelineWrap.cpp"),
        "playtest_apt_timeline_wrap_bodies.inc", bodies, "PlaytestAptTimelineWrap")
    return report("run_playtest_apt_timeline_wrap", [], result, 63)

if __name__ == "__main__":
    sys.exit(main())
