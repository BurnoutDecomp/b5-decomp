"""FX-GS2 (crash parity 2026-09-23, G10-D9): BrnGameState::CrashModeScoring::DealWithRemovedTraffic.

  0x8232BF90: for each removed traffic index of the GUI-209 record (count read once, `lhz` u16),
  scan maRecentCrashes (count re-read per id, operator[] + `lhz` +0, `cmplw`) and Erase the FIRST
  match (RecentCrash<64>::Erase 0x82319308, order-preserving). Sole caller
  BrnGameModule::BridgeWorldTrafficAndPropDataToGui @0x823E5560 (not in this build yet).

Numeric: tests/FxGs2RemovedTraffic.cpp compiled against the extracted production body (and
GetRecentCrash, the reader DealWithHitTrafficCar gates on), the real CrashModeScoring layout and the
real BrnGui::GuiRemovedTrafficEvent. A revision without the body gets a labelled empty stand-in.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs2_removed_traffic.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP

SCORING_CPP = "src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp"
NUMERIC_CHECKS = 10

REQUIRED = ["    CrashModeScoring::RecentCrash* CrashModeScoring::GetRecentCrash("]
OPTIONAL = [("    void CrashModeScoring::DealWithRemovedTraffic(",
             "void CrashModeScoring::DealWithRemovedTraffic(const BrnGui::GuiRemovedTrafficEvent*) {}")]


def numeric(tree):
    source = tree.read(SCORING_CPP)
    parts, stood_in = [], []
    for signature in REQUIRED:
        try:
            parts.append(definition(source, signature))
        except ValueError:
            print("NUMERIC: cannot build -- production body absent: " + signature.strip())
            return None
    for signature, stand_in in OPTIONAL:
        try:
            parts.append(definition(source, signature))
        except ValueError:
            parts.append("// [stand-in: body absent in this revision]\n" + stand_in)
            stood_in.append(signature.split("::", 1)[1].strip())
    if stood_in:
        print("NUMERIC: bodies absent in this revision (empty stand-ins): " + ", ".join(stood_in))
    inc = "namespace BrnGameState {\n" + "\n".join(parts) + "\n}\n"
    return compile_and_run(Path(__file__).with_name("FxGs2RemovedTraffic.cpp"), "removed_traffic.inc", inc,
                           "FxGs2RemovedTraffic", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgs2_removed_traffic", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
