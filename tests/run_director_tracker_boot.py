"""The main director must keep the tracker in the console's zero-initialized state.

ARTIST main @0x827E622C calls DebugMemoryInit @0x823A8ED8 before the module
constructors: the director range stays zero. Neither MainDirector's C++ ctor
@0x827E4AB8 nor Construct @0x8225B448 calls VehicleTracker::Construct.
That reset's only call is ReplayDirector::PreSceneQueryUpdate @0x8225BD28.
The extra PC call seeded mbFirstFrame, making the first unpaused Update fill
eight zero timesteps, then GetImplicitVelocity asserted/divided by zero.

Extract the production initialization span between its unchanged snapshot and
behaviour-manager calls; run it with the real tracker Update and accessors.
The enclosing module is zero-initialized, as in the console boot path.
"""
import argparse
from pathlib import Path
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

MAIN = "src/GameSource/Director/BrnMainDirector.cpp"
TRACKER = "src/GameSource/Director/Utils/BrnDirectorVehicleTracker.cpp"
TRACKER_H = "src/GameSource/Director/Utils/BrnDirectorVehicleTracker.h"
JOURNAL_H = "src/GameSource/Director/Utils/BrnDirectorDataJournal.h"


def numeric(tree):
    body = code_only(definition(tree.read(MAIN),
        "void MainDirector::Construct(const DirectorResourceManager* lpResourceManager, f32 lfTime)"))
    start = body.index("mAllVehicleData.Construct();")
    end = body.index("mBehaviourManager.Construct();", start) + len("mBehaviourManager.Construct();")
    inc = tree.read(TRACKER) + "\nvoid DirectorInitFixture::Construct() {\n" + body[start:end] + "\n}\n"
    return compile_and_run(Path(__file__).with_name("DirectorTrackerBoot.cpp"),
        "director_tracker_boot.inc", inc, "DirectorTrackerBoot",
        shadow={p: tree.read(p) for p in (TRACKER_H, JOURNAL_H)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    args = parser.parse_args()
    return report("run_director_tracker_boot", [], numeric(Tree(args.rev)), 8)


if __name__ == "__main__":
    sys.exit(main())
