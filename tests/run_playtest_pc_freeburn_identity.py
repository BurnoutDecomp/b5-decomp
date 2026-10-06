"""Verify normal-PC LAN entry and peer identity isolation, using the production resolver."""
from pathlib import Path
import argparse
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, compile_and_run, report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    args = parser.parse_args()
    tree = Tree(args.rev)
    source = tree.read("src/GameShared/GameClasses/System/PC/CgsPcNetIdentity.cpp")
    numeric = compile_and_run(Path(__file__).with_name("PlaytestPcFreeburnIdentity.cpp"),
        "playtest_pc_freeburn_identity.inc", source, "PlaytestPcFreeburnIdentity")
    return report("run_playtest_pc_freeburn_identity", [], numeric, 12)

if __name__ == "__main__":
    sys.exit(main())
