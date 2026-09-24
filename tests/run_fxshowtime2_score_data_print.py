"""FX-SHOWTIME2 (crash parity 2026-09-24): CrashModeScoring::GetVehicleScoreData @0x82312AB0 prints the console's
"Unknown traffic vehicle in Showtime scoring: <id>" line for a type id its 24-row table lacks.

  The fallback tail (0x82312C4C..0x82312CC0) un-compresses the id and, when bit 0 of
  CgsDev::Message::gxMessageFilterFlags is set, streams the literal @0x8202304C, the id and "\n" @0x82001CC4
  through gpDebugPrint. The tree kept only the un-compress and FLAG-omitted the print as "not homed",
  although both symbols are homed in CgsLog.h.

Numeric: tests/FxShowtime2ScoreDataPrint.cpp compiles the extracted production GetVehicleScoreData against a
capturing DebugPrint: a table hit prints nothing, an unknown id prints exactly the console's line, and a filter
with bit 0 clear prints nothing.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxshowtime2_score_data_print.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, extract, compile_and_run, report, STRSTREAM_CPP

SCORING_CPP = "src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp"
NUMERIC_CHECKS = 9

METHODS = ["    void CrashModeScoring::GetVehicleScoreData("]


def numeric(tree):
    methods, missing = extract(tree, SCORING_CPP, METHODS)
    if missing:
        print("NUMERIC: cannot build -- production bodies absent: " + ", ".join(missing))
        return None
    return compile_and_run(Path(__file__).with_name("FxShowtime2ScoreDataPrint.cpp"), "score_data_methods.inc",
                           "\n".join(methods) + "\n", "FxShowtime2ScoreDataPrint",
                           extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxshowtime2_score_data_print", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
